// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/plugin_list.h"

#include <algorithm>
#include <dlfcn.h>
#if defined(OS_OPENBSD)
#include <sys/exec_elf.h>
#else
#include <elf.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/cpu.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sha1.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/npapi/bindings/nphostapi.h"

// These headers must be included in this order to make the declaration gods
// happy.
#include "base/third_party/nspr/prcpucfg_linux.h"

namespace content {

namespace {

// We build up a list of files and mtimes so we can sort them.
typedef std::pair<base::FilePath, base::Time> FileAndTime;
typedef std::vector<FileAndTime> FileTimeList;

enum PluginQuirk {
  // No quirks - plugin is outright banned.
  PLUGIN_QUIRK_NONE = 0,
  // Plugin is using SSE2 instructions without checking for SSE2 instruction
  // support. Ban the plugin if the system has no SSE2 support.
  PLUGIN_QUIRK_MISSING_SSE2_CHECK = 1 << 0,
};

// Copied from nsplugindefs.h instead of including the file since it has a bunch
// of dependencies.
enum nsPluginVariable {
  nsPluginVariable_NameString        = 1,
  nsPluginVariable_DescriptionString = 2
};

// Comparator used to sort by descending mtime then ascending filename.
bool CompareTime(const FileAndTime& a, const FileAndTime& b) {
  if (a.second == b.second) {
    // Fall back on filename sorting, just to make the predicate valid.
    return a.first < b.first;
  }

  // Sort by mtime, descending.
  return a.second > b.second;
}

// Checks to see if the current environment meets any of the condtions set in
// |quirks|. Returns true if any of the conditions are met, or if |quirks| is
// PLUGIN_QUIRK_NONE.
bool CheckQuirks(PluginQuirk quirks) {
  if (quirks == PLUGIN_QUIRK_NONE)
    return true;

  if ((quirks & PLUGIN_QUIRK_MISSING_SSE2_CHECK) != 0) {
    base::CPU cpu;
    if (!cpu.has_sse2())
      return true;
  }

  return false;
}

// Return true if |path| matches a known (file size, sha1sum) pair.
// Also check against any PluginQuirks the bad plugin may have.
// The use of the file size is an optimization so we don't have to read in
// the entire file unless we have to.
bool IsBlacklistedBySha1sumAndQuirks(const base::FilePath& path) {
  const struct BadEntry {
    int64 size;
    std::string sha1;
    PluginQuirk quirks;
  } bad_entries[] = {
    // Flash 9 r31 - http://crbug.com/29237
    { 7040080, "fa5803061125ca47846713b34a26a42f1f1e98bb", PLUGIN_QUIRK_NONE },
    // Flash 9 r48 - http://crbug.com/29237
    { 7040036, "0c4b3768a6d4bfba003088e4b9090d381de1af2b", PLUGIN_QUIRK_NONE },
    // Flash 11.2.202.236, 32-bit - http://crbug.com/140086
    { 17406436, "1e07eac912faf9426c52a288c76c3b6238f90b6b",
      PLUGIN_QUIRK_MISSING_SSE2_CHECK },
    // Flash 11.2.202.238, 32-bit - http://crbug.com/140086
    { 17410532, "e9401097e97c8443a7d9156be62184ffe1addd5c",
      PLUGIN_QUIRK_MISSING_SSE2_CHECK },
  };

  int64 size;
  if (!file_util::GetFileSize(path, &size))
    return false;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(bad_entries); i++) {
    if (bad_entries[i].size != size)
      continue;

    std::string file_content;
    if (!file_util::ReadFileToString(path, &file_content))
      continue;
    std::string sha1 = base::SHA1HashString(file_content);
    std::string sha1_readable;
    for (size_t j = 0; j < sha1.size(); j++)
      base::StringAppendF(&sha1_readable, "%02x", sha1[j] & 0xFF);
    if (bad_entries[i].sha1 == sha1_readable)
      return CheckQuirks(bad_entries[i].quirks);
  }
  return false;
}

// Some plugins are shells around other plugins; we prefer to use the
// real plugin directly, if it's available.  This function returns
// true if we should prefer other plugins over this one.  We'll still
// use a "undesirable" plugin if no other option is available.
bool IsUndesirablePlugin(const WebPluginInfo& info) {
  std::string filename = info.path.BaseName().value();
  const char* kUndesiredPlugins[] = {
    "npcxoffice",  // Crossover
    "npwrapper",   // nspluginwrapper
  };
  for (size_t i = 0; i < arraysize(kUndesiredPlugins); i++) {
    if (filename.find(kUndesiredPlugins[i]) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Return true if we shouldn't load a plugin at all.
// This is an ugly hack to blacklist Adobe Acrobat due to not supporting
// its Xt-based mainloop.
// http://code.google.com/p/chromium/issues/detail?id=38229
bool IsBlacklistedPlugin(const base::FilePath& path) {
  const char* kBlackListedPlugins[] = {
    "nppdf.so",           // Adobe PDF
  };
  std::string filename = path.BaseName().value();
  for (size_t i = 0; i < arraysize(kBlackListedPlugins); i++) {
    if (filename.find(kBlackListedPlugins[i]) != std::string::npos) {
      return true;
    }
  }
  return IsBlacklistedBySha1sumAndQuirks(path);
}

// Read the ELF header and return true if it is usable on
// the current architecture (e.g. 32-bit ELF on 32-bit build).
// Returns false on other errors as well.
bool ELFMatchesCurrentArchitecture(const base::FilePath& filename) {
  // First make sure we can open the file and it is in fact, a regular file.
  struct stat stat_buf;
  // Open with O_NONBLOCK so we don't block on pipes.
  int fd = open(filename.value().c_str(), O_RDONLY|O_NONBLOCK);
  if (fd < 0)
    return false;
  bool ret = (fstat(fd, &stat_buf) >= 0 && S_ISREG(stat_buf.st_mode));
  if (HANDLE_EINTR(close(fd)) < 0)
    return false;
  if (!ret)
    return false;

  const size_t kELFBufferSize = 5;
  char buffer[kELFBufferSize];
  if (!file_util::ReadFile(filename, buffer, kELFBufferSize))
    return false;

  if (buffer[0] != ELFMAG0 ||
      buffer[1] != ELFMAG1 ||
      buffer[2] != ELFMAG2 ||
      buffer[3] != ELFMAG3) {
    // Not an ELF file, perhaps?
    return false;
  }

  int elf_class = buffer[EI_CLASS];
#if defined(ARCH_CPU_32_BITS)
  if (elf_class == ELFCLASS32)
    return true;
#elif defined(ARCH_CPU_64_BITS)
  if (elf_class == ELFCLASS64)
    return true;
#endif

  return false;
}

// This structure matches enough of nspluginwrapper's NPW_PluginInfo
// for us to extract the real plugin path.
struct __attribute__((packed)) NSPluginWrapperInfo {
  char ident[32];  // NSPluginWrapper magic identifier (includes version).
  char path[PATH_MAX];  // Path to wrapped plugin.
};

// Test a plugin for whether it's been wrapped by NSPluginWrapper, and
// if so attempt to unwrap it.  Pass in an opened plugin handle; on
// success, |dl| and |unwrapped_path| will be filled in with the newly
// opened plugin.  On failure, params are left unmodified.
void UnwrapNSPluginWrapper(void **dl, base::FilePath* unwrapped_path) {
  NSPluginWrapperInfo* info =
      reinterpret_cast<NSPluginWrapperInfo*>(dlsym(*dl, "NPW_Plugin"));
  if (!info)
    return;  // Not a NSPW plugin.

  // Here we could check the NSPW ident field for the versioning
  // information, but the path field is available in all versions
  // anyway.

  // Grab the path to the wrapped plugin.  Just in case the structure
  // format changes, protect against the path not being null-terminated.
  char* path_end = static_cast<char*>(memchr(info->path, '\0',
                                             sizeof(info->path)));
  if (!path_end)
    path_end = info->path + sizeof(info->path);
  base::FilePath path = base::FilePath(
      std::string(info->path, path_end - info->path));

  if (!ELFMatchesCurrentArchitecture(path)) {
    LOG(WARNING) << path.value() << " is nspluginwrapper wrapping a "
                 << "plugin for a different architecture; it will "
                 << "work better if you instead use a native plugin.";
    return;
  }

  std::string error;
  void* newdl = base::LoadNativeLibrary(path, &error);
  if (!newdl) {
    // We couldn't load the unwrapped plugin for some reason, despite
    // being able to load the wrapped one.  Just use the wrapped one.
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Could not use unwrapped nspluginwrapper plugin "
        << unwrapped_path->value() << " (" << error << "), "
        << "using the wrapped one.";
    return;
  }

  // Unload the wrapped plugin, and use the wrapped plugin instead.
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Using unwrapped version " << unwrapped_path->value()
      << " of nspluginwrapper-wrapped plugin.";
  base::UnloadNativeLibrary(*dl);
  *dl = newdl;
  *unwrapped_path = path;
}

}  // namespace

bool PluginList::ReadWebPluginInfo(const base::FilePath& filename,
                                   WebPluginInfo* info) {
  // The file to reference is:
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUnix.cpp

  // Skip files that aren't appropriate for our architecture.
  if (!ELFMatchesCurrentArchitecture(filename)) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Skipping plugin " << filename.value()
        << " because it doesn't match the current architecture.";
    return false;
  }

  std::string error;
  void* dl = base::LoadNativeLibrary(filename, &error);
  if (!dl) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "While reading plugin info, unable to load library "
        << filename.value() << " (" << error << "), skipping.";
    return false;
  }

  info->path = filename;

  // Attempt to swap in the wrapped plugin if this is nspluginwrapper.
  UnwrapNSPluginWrapper(&dl, &info->path);

  // See comments in plugin_lib_mac regarding this symbol.
  typedef const char* (*NP_GetMimeDescriptionType)();
  NP_GetMimeDescriptionType NP_GetMIMEDescription =
      reinterpret_cast<NP_GetMimeDescriptionType>(
          dlsym(dl, "NP_GetMIMEDescription"));
  const char* mime_description = NULL;
  if (!NP_GetMIMEDescription) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << filename.value() << " doesn't have a "
        << "NP_GetMIMEDescription symbol";
    return false;
  }
  mime_description = NP_GetMIMEDescription();

  if (!mime_description) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "MIME description for " << filename.value() << " is empty";
    return false;
  }
  ParseMIMEDescription(mime_description, &info->mime_types);

  // The plugin name and description live behind NP_GetValue calls.
  typedef NPError (*NP_GetValueType)(void* unused,
                                     nsPluginVariable variable,
                                     void* value_out);
  NP_GetValueType NP_GetValue =
      reinterpret_cast<NP_GetValueType>(dlsym(dl, "NP_GetValue"));
  if (NP_GetValue) {
    const char* name = NULL;
    NP_GetValue(NULL, nsPluginVariable_NameString, &name);
    if (name) {
      info->name = UTF8ToUTF16(name);
      ExtractVersionString(name, info);
    }

    const char* description = NULL;
    NP_GetValue(NULL, nsPluginVariable_DescriptionString, &description);
    if (description) {
      info->desc = UTF8ToUTF16(description);
      if (info->version.empty())
        ExtractVersionString(description, info);
    }

    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Got info for plugin " << filename.value()
        << " Name = \"" << UTF16ToUTF8(info->name)
        << "\", Description = \"" << UTF16ToUTF8(info->desc)
        << "\", Version = \"" << UTF16ToUTF8(info->version)
        << "\".";
  } else {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Plugin " << filename.value()
        << " has no GetValue() and probably won't work.";
  }

  // Intentionally not unloading the plugin here, it can lead to crashes.

  return true;
}

// static
void PluginList::ParseMIMEDescription(
    const std::string& description,
    std::vector<WebPluginMimeType>* mime_types) {
  // We parse the description here into WebPluginMimeType structures.
  // Naively from the NPAPI docs you'd think you could use
  // string-splitting, but the Firefox parser turns out to do something
  // different: find the first colon, then the second, then a semi.
  //
  // See ParsePluginMimeDescription near
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginsDirUtils.h#53

  std::string::size_type ofs = 0;
  for (;;) {
    WebPluginMimeType mime_type;

    std::string::size_type end = description.find(':', ofs);
    if (end == std::string::npos)
      break;
    mime_type.mime_type = description.substr(ofs, end - ofs);
    ofs = end + 1;

    end = description.find(':', ofs);
    if (end == std::string::npos)
      break;
    const std::string extensions = description.substr(ofs, end - ofs);
    base::SplitString(extensions, ',', &mime_type.file_extensions);
    ofs = end + 1;

    end = description.find(';', ofs);
    // It's ok for end to run off the string here.  If there's no
    // trailing semicolon we consume the remainder of the string.
    if (end != std::string::npos) {
      mime_type.description = UTF8ToUTF16(description.substr(ofs, end - ofs));
    } else {
      mime_type.description = UTF8ToUTF16(description.substr(ofs));
    }
    mime_types->push_back(mime_type);
    if (end == std::string::npos)
      break;
    ofs = end + 1;
  }
}

// static
void PluginList::ExtractVersionString(const std::string& desc,
                                      WebPluginInfo* info) {
  // This matching works by extracting a version substring, along the lines of:
  // No postfix: second match in .*<prefix>.*$
  // With postfix: second match .*<prefix>.*<postfix>
  static const struct {
    const char* kPrefix;
    const char* kPostfix;
  } kPrePostFixes[] = {
    { "Shockwave Flash ", 0 },
    { "Java(TM) Plug-in ", 0 },
    { "(using IcedTea-Web ", " " },
    { 0, 0 }
  };
  std::string version;
  for (size_t i = 0; kPrePostFixes[i].kPrefix; ++i) {
    size_t pos;
    if ((pos = desc.find(kPrePostFixes[i].kPrefix)) != std::string::npos) {
      version = desc.substr(pos + strlen(kPrePostFixes[i].kPrefix));
      pos = std::string::npos;
      if (kPrePostFixes[i].kPostfix)
        pos = version.find(kPrePostFixes[i].kPostfix);
      if (pos != std::string::npos)
        version = version.substr(0, pos);
      break;
    }
  }
  if (!version.empty()) {
    info->version = UTF8ToUTF16(version);
  }
}

void PluginList::GetPluginDirectories(std::vector<base::FilePath>* plugin_dirs) {
  // See http://groups.google.com/group/chromium-dev/browse_thread/thread/7a70e5fcbac786a9
  // for discussion.
  // We first consult Chrome-specific dirs, then fall back on the logic
  // Mozilla uses.

  if (PluginList::plugins_discovery_disabled_)
    return;

  // Note: "extra" plugin dirs and paths are examined before these.
  // "Extra" are those added by PluginList::AddExtraPluginDir() and
  // PluginList::AddExtraPluginPath().

  // The Chrome binary dir + "plugins/".
  base::FilePath dir;
  PathService::Get(base::DIR_EXE, &dir);
  plugin_dirs->push_back(dir.Append("plugins"));

  // Chrome OS only loads plugins from /opt/google/chrome/plugins.
#if !defined(OS_CHROMEOS)
  // Mozilla code to reference:
  // http://mxr.mozilla.org/firefox/ident?i=NS_APP_PLUGINS_DIR_LIST
  // and tens of accompanying files (mxr is very helpful).
  // This code carefully matches their behavior for compat reasons.

  // 1) MOZ_PLUGIN_PATH env variable.
  const char* moz_plugin_path = getenv("MOZ_PLUGIN_PATH");
  if (moz_plugin_path) {
    std::vector<std::string> paths;
    base::SplitString(moz_plugin_path, ':', &paths);
    for (size_t i = 0; i < paths.size(); ++i)
      plugin_dirs->push_back(base::FilePath(paths[i]));
  }

  // 2) NS_USER_PLUGINS_DIR: ~/.mozilla/plugins.
  // This is a de-facto standard, so even though we're not Mozilla, let's
  // look in there too.
  base::FilePath home = file_util::GetHomeDir();
  if (!home.empty())
    plugin_dirs->push_back(home.Append(".mozilla/plugins"));

  // 3) NS_SYSTEM_PLUGINS_DIR:
  // This varies across different browsers and versions, so check 'em all.
  plugin_dirs->push_back(base::FilePath("/usr/lib/browser-plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib/mozilla/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib/firefox/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib/xulrunner-addons/plugins"));

#if defined(ARCH_CPU_64_BITS)
  // On my Ubuntu system, /usr/lib64 is a symlink to /usr/lib.
  // But a user reported on their Fedora system they are separate.
  plugin_dirs->push_back(base::FilePath("/usr/lib64/browser-plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib64/mozilla/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib64/firefox/plugins"));
  plugin_dirs->push_back(base::FilePath("/usr/lib64/xulrunner-addons/plugins"));
#endif  // defined(ARCH_CPU_64_BITS)
#endif  // !defined(OS_CHROMEOS)
}

void PluginList::GetPluginsInDir(
    const base::FilePath& dir_path, std::vector<base::FilePath>* plugins) {
  // See ScanPluginsDirectory near
  // http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/nsPluginHostImpl.cpp#5052

  // Construct and stat a list of all filenames under consideration, for
  // later sorting by mtime.
  FileTimeList files;
  base::FileEnumerator enumerator(dir_path,
                                  false,  // not recursive
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    // Skip over Mozilla .xpt files.
    if (path.MatchesExtension(FILE_PATH_LITERAL(".xpt")))
      continue;

    // Java doesn't like being loaded through a symlink, since it uses
    // its path to find dependent data files.
    // MakeAbsoluteFilePath calls through to realpath(), which resolves
    // symlinks.
    base::FilePath orig_path = path;
    path = base::MakeAbsoluteFilePath(path);
    if (path.empty())
      path = orig_path;
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << "Resolved " << orig_path.value() << " -> " << path.value();

    if (std::find(plugins->begin(), plugins->end(), path) != plugins->end()) {
      LOG_IF(ERROR, PluginList::DebugPluginLoading())
          << "Skipping duplicate instance of " << path.value();
      continue;
    }

    if (IsBlacklistedPlugin(path)) {
      LOG_IF(ERROR, PluginList::DebugPluginLoading())
          << "Skipping blacklisted plugin " << path.value();
      continue;
    }

    // Flash stops working if the containing directory involves 'netscape'.
    // No joke.  So use the other path if it's better.
    static const char kFlashPlayerFilename[] = "libflashplayer.so";
    static const char kNetscapeInPath[] = "/netscape/";
    if (path.BaseName().value() == kFlashPlayerFilename &&
        path.value().find(kNetscapeInPath) != std::string::npos) {
      if (orig_path.value().find(kNetscapeInPath) == std::string::npos) {
        // Go back to the old path.
        path = orig_path;
      } else {
        LOG_IF(ERROR, PluginList::DebugPluginLoading())
            << "Flash misbehaves when used from a directory containing "
            << kNetscapeInPath << ", so skipping " << orig_path.value();
        continue;
      }
    }

    // Get mtime.
    base::PlatformFileInfo info;
    if (!file_util::GetFileInfo(path, &info))
      continue;

    files.push_back(std::make_pair(path, info.last_modified));
  }

  // Sort the file list by time (and filename).
  std::sort(files.begin(), files.end(), CompareTime);

  // Load the files in order.
  for (FileTimeList::const_iterator i = files.begin(); i != files.end(); ++i) {
    plugins->push_back(i->first);
  }
}

bool PluginList::ShouldLoadPluginUsingPluginList(
    const WebPluginInfo& info, std::vector<WebPluginInfo>* plugins) {
  LOG_IF(ERROR, PluginList::DebugPluginLoading())
      << "Considering " << info.path.value() << " (" << info.name << ")";

  if (IsUndesirablePlugin(info)) {
    LOG_IF(ERROR, PluginList::DebugPluginLoading())
        << info.path.value() << " is undesirable.";

    // See if we have a better version of this plugin.
    for (size_t j = 0; j < plugins->size(); ++j) {
      if ((*plugins)[j].name == info.name &&
          !IsUndesirablePlugin((*plugins)[j])) {
        // Skip the current undesirable one so we can use the better one
        // we just found.
        LOG_IF(ERROR, PluginList::DebugPluginLoading())
            << "Skipping " << info.path.value() << ", preferring "
            << (*plugins)[j].path.value();
        return false;
      }
    }
  }

  // TODO(evanm): prefer the newest version of flash, etc. here?

  VLOG_IF(1, PluginList::DebugPluginLoading()) << "Using " << info.path.value();

  return true;
}

}  // namespace content
