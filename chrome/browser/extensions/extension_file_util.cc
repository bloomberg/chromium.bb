// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_util.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_io.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/json_value_serializer.h"
#include "net/base/file_stream.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extension_file_util {

// Validates locale info. Doesn't check if messages.json files are valid.
static bool ValidateLocaleInfo(const Extension& extension, std::string* error);

const char kInstallDirectoryName[] = "Extensions";
// TODO(mpcomplete): obsolete. remove after migration period.
// http://code.google.com/p/chromium/issues/detail?id=19733
const char kCurrentVersionFileName[] = "Current Version";

bool MoveDirSafely(const FilePath& source_dir, const FilePath& dest_dir) {
  if (file_util::PathExists(dest_dir)) {
    if (!file_util::Delete(dest_dir, true))
      return false;
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent))
        return false;
    }
  }

  if (!file_util::Move(source_dir, dest_dir))
    return false;

  return true;
}

Extension::InstallType CompareToInstalledVersion(
    const FilePath& extensions_dir,
    const std::string& extension_id,
    const std::string& current_version_str,
    const std::string& new_version_str,
    FilePath* version_dir) {
  FilePath dest_dir = extensions_dir.AppendASCII(extension_id);
  FilePath current_version_dir = dest_dir.AppendASCII(current_version_str);
  *version_dir = dest_dir.AppendASCII(new_version_str);

  if (current_version_str.empty())
    return Extension::NEW_INSTALL;

  scoped_ptr<Version> current_version(
    Version::GetVersionFromString(current_version_str));
  scoped_ptr<Version> new_version(
    Version::GetVersionFromString(new_version_str));
  int comp = new_version->CompareTo(*current_version);
  if (comp > 0)
    return Extension::UPGRADE;
  if (comp < 0)
    return Extension::DOWNGRADE;

  // Same version. Treat corrupted existing installation as new install case.
  if (!SanityCheckExtension(current_version_dir))
    return Extension::NEW_INSTALL;

  return Extension::REINSTALL;
}

bool SanityCheckExtension(const FilePath& dir) {
  // Verify that the directory actually exists.
  // TODO(erikkay): A further step would be to verify that the extension
  // has actually loaded successfully.
  FilePath manifest_file(dir.AppendASCII(Extension::kManifestFilename));
  return file_util::PathExists(dir) && file_util::PathExists(manifest_file);
}

bool InstallExtension(const FilePath& src_dir,
                      const FilePath& version_dir,
                      std::string* error) {
  // If anything fails after this, we want to delete the extension dir.
  ScopedTempDir scoped_version_dir;
  scoped_version_dir.Set(version_dir);

  if (!MoveDirSafely(src_dir, version_dir)) {
    *error = "Could not move extension directory into profile.";
    return false;
  }

  scoped_version_dir.Take();
  return true;
}

Extension* LoadExtension(const FilePath& extension_path,
                         bool require_key,
                         std::string* error) {
  FilePath manifest_path =
      extension_path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    *error = extension_manifest_errors::kManifestUnreadable;
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<Value> root(serializer.Deserialize(error));
  if (!root.get()) {
    if (error->empty()) {
      // If |error| is empty, than the file could not be read.
      // It would be cleaner to have the JSON reader give a specific error
      // in this case, but other code tests for a file error with
      // error->empty().  For now, be consistent.
      *error = extension_manifest_errors::kManifestUnreadable;
    } else {
      *error = StringPrintf("%s  %s",
                            extension_manifest_errors::kManifestParseError,
                            error->c_str());
    }
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    *error = extension_manifest_errors::kInvalidManifest;
    return NULL;
  }

  DictionaryValue* manifest = static_cast<DictionaryValue*>(root.get());

  scoped_ptr<Extension> extension(new Extension(extension_path));

  if (!extension_l10n_util::LocalizeExtension(extension.get(), manifest, error))
    return NULL;

  if (!extension->InitFromValue(*manifest, require_key, error))
    return NULL;

  if (!ValidateExtension(extension.get(), error))
    return NULL;

  return extension.release();
}

bool ValidateExtension(Extension* extension, std::string* error) {
  // Validate icons exist.
  for (std::map<int, std::string>::const_iterator iter =
       extension->icons().begin(); iter != extension->icons().end(); ++iter) {
    const FilePath path = extension->GetResource(iter->second).GetFilePath();
    if (!file_util::PathExists(path)) {
      *error = StringPrintf("Could not load extension icon '%s'.",
                            iter->second.c_str());
      return false;
    }
  }

  // Theme resource validation.
  if (extension->IsTheme()) {
    DictionaryValue* images_value = extension->GetThemeImages();
    if (images_value) {
      for (DictionaryValue::key_iterator iter = images_value->begin_keys();
           iter != images_value->end_keys(); ++iter) {
        std::string val;
        if (images_value->GetStringWithoutPathExpansion(*iter, &val)) {
          FilePath image_path = extension->path().AppendASCII(val);
          if (!file_util::PathExists(image_path)) {
            *error = StringPrintf(
                "Could not load '%s' for theme.",
                WideToUTF8(image_path.ToWStringHack()).c_str());
            return false;
          }
        }
      }
    }

    // Themes cannot contain other extension types.
    return true;
  }

  // Validate that claimed script resources actually exist.
  for (size_t i = 0; i < extension->content_scripts().size(); ++i) {
    const UserScript& script = extension->content_scripts()[i];

    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      const UserScript::File& js_script = script.js_scripts()[j];
      const FilePath& path = ExtensionResource::GetFilePath(
          js_script.extension_root(), js_script.relative_path());
      if (!file_util::PathExists(path)) {
        *error = StringPrintf(
            "Could not load javascript '%s' for content script.",
            WideToUTF8(js_script.relative_path().ToWStringHack()).c_str());
        return false;
      }
    }

    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      const UserScript::File& css_script = script.css_scripts()[j];
      const FilePath& path = ExtensionResource::GetFilePath(
          css_script.extension_root(), css_script.relative_path());
      if (!file_util::PathExists(path)) {
        *error = StringPrintf(
            "Could not load css '%s' for content script.",
            WideToUTF8(css_script.relative_path().ToWStringHack()).c_str());
        return false;
      }
    }
  }

  // Validate claimed plugin paths.
  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    if (!file_util::PathExists(plugin.path)) {
      *error = StringPrintf("Could not load '%s' for plugin.",
                            WideToUTF8(plugin.path.ToWStringHack()).c_str());
      return false;
    }
  }

  // Validate claimed privacy blacklists.
  for (size_t i = 0; i < extension->privacy_blacklists().size(); ++i) {
    const Extension::PrivacyBlacklistInfo& blacklist_info =
        extension->privacy_blacklists()[i];
    std::string path_utf8(WideToUTF8(blacklist_info.path.ToWStringHack()));
    if (!file_util::PathExists(blacklist_info.path)) {
      *error = StringPrintf("Could not load '%s' for privacy blacklist: "
                            "file does not exist.",
                            path_utf8.c_str());
      return false;
    }
    Blacklist blacklist;
    std::string parsing_error;
    if (!BlacklistIO::ReadText(&blacklist, blacklist_info.path,
                               &parsing_error)) {
      *error = StringPrintf("Could not load '%s' for privacy blacklist: %s",
                            path_utf8.c_str(), parsing_error.c_str());
      return false;
    }
  }

  // Validate icon location for page actions.
  ExtensionAction* page_action = extension->page_action();
  if (page_action) {
    std::vector<std::string> icon_paths(*page_action->icon_paths());
    if (!page_action->default_icon_path().empty())
      icon_paths.push_back(page_action->default_icon_path());
    for (std::vector<std::string>::iterator iter = icon_paths.begin();
         iter != icon_paths.end(); ++iter) {
      if (!file_util::PathExists(extension->GetResource(*iter).GetFilePath())) {
        *error = StringPrintf("Could not load icon '%s' for page action.",
                              iter->c_str());
        return false;
      }
    }
  }

  // Validate icon location for browser actions.
  // Note: browser actions don't use the icon_paths().
  ExtensionAction* browser_action = extension->browser_action();
  if (browser_action) {
    std::string path = browser_action->default_icon_path();
    if (!path.empty() &&
        !file_util::PathExists(extension->GetResource(path).GetFilePath())) {
        *error = StringPrintf("Could not load icon '%s' for browser action.",
                              path.c_str());
        return false;
    }
  }

  // Validate background page location.
  if (!extension->background_url().is_empty()) {
    const std::string page_path = extension->background_url().path();
    const FilePath path = extension->GetResource(page_path).GetFilePath();
    if (!file_util::PathExists(path)) {
      *error = StringPrintf("Could not load background page '%s'.",
                            WideToUTF8(path.ToWStringHack()).c_str());
      return false;
    }
  }

  // Validate locale info.
  if (!ValidateLocaleInfo(*extension, error))
    return false;

  // Check children of extension root to see if any of them start with _ and is
  // not on the reserved list.
  if (!CheckForIllegalFilenames(extension->path(), error)) {
    return false;
  }

  return true;
}

void UninstallExtension(const std::string& id, const FilePath& extensions_dir) {
  // First, delete the Current Version file. If the directory delete fails, then
  // at least the extension won't be loaded again.
  FilePath extension_root = extensions_dir.AppendASCII(id);

  if (!file_util::PathExists(extension_root)) {
    LOG(WARNING) << "Asked to remove a non-existent extension " << id;
    return;
  }

  FilePath current_version_file = extension_root.AppendASCII(
      kCurrentVersionFileName);
  if (!file_util::PathExists(current_version_file)) {
    // This is OK, since we're phasing out the current version file.
  } else {
    if (!file_util::Delete(current_version_file, false)) {
      LOG(WARNING) << "Could not delete Current Version file for extension "
                   << id;
      return;
    }
  }

  // OK, now try and delete the entire rest of the directory. One major place
  // this can fail is if the extension contains a plugin (stupid plugins). It's
  // not a big deal though, because we'll notice next time we startup that the
  // Current Version file is gone and finish the delete then.
  if (!file_util::Delete(extension_root, true))
    LOG(WARNING) << "Could not delete directory for extension " << id;
}

void GarbageCollectExtensions(
    const FilePath& install_directory,
    const std::set<std::string>& installed_ids,
    const std::map<std::string, std::string>& installed_versions) {
  // Nothing to clean up if it doesn't exist.
  if (!file_util::DirectoryExists(install_directory))
    return;

  LOG(INFO) << "Loading installed extensions...";
  file_util::FileEnumerator enumerator(install_directory,
                                       false,  // Not recursive.
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath extension_path;
  for (extension_path = enumerator.Next(); !extension_path.value().empty();
       extension_path = enumerator.Next()) {
    std::string extension_id = WideToASCII(
        extension_path.BaseName().ToWStringHack());

    // If there is no entry in the prefs file, just delete the directory and
    // move on. This can legitimately happen when an uninstall does not
    // complete, for example, when a plugin is in use at uninstall time.
    if (installed_ids.count(extension_id) == 0) {
      LOG(INFO) << "Deleting unreferenced install for directory "
                << WideToASCII(extension_path.ToWStringHack()) << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }

    // Delete directories that aren't valid IDs.
    if (!Extension::IdIsValid(extension_id)) {
      LOG(WARNING) << "Invalid extension ID encountered in extensions "
                      "directory: " << extension_id;
      LOG(INFO) << "Deleting invalid extension directory "
                << WideToASCII(extension_path.ToWStringHack()) << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }

    // Clean up old version directories.
    file_util::FileEnumerator versions_enumerator(
        extension_path,
        false,  // Not recursive.
        file_util::FileEnumerator::DIRECTORIES);
    for (FilePath version_dir = versions_enumerator.Next();
         !version_dir.value().empty();
         version_dir = versions_enumerator.Next()) {
      std::map<std::string, std::string>::const_iterator installed_version =
          installed_versions.find(extension_id);
      if (installed_version == installed_versions.end()) {
        NOTREACHED() << "No installed version found for " << extension_id;
        continue;
      }

      std::string version = WideToASCII(version_dir.BaseName().ToWStringHack());
      if (version != installed_version->second) {
        LOG(INFO) << "Deleting old version for directory "
                  << WideToASCII(version_dir.ToWStringHack()) << ".";
        file_util::Delete(version_dir, true);  // Recursive.
      }
    }
  }
}

ExtensionMessageBundle* LoadExtensionMessageBundle(
    const FilePath& extension_path,
    const DictionaryValue& manifest,
    std::string* error) {
  error->clear();
  // Load locale information if available.
  FilePath locale_path = extension_path.AppendASCII(Extension::kLocaleFolder);
  if (!file_util::PathExists(locale_path))
    return NULL;

  std::set<std::string> locales;
  if (!extension_l10n_util::GetValidLocales(locale_path, &locales, error))
    return NULL;

  std::string default_locale =
    extension_l10n_util::GetDefaultLocaleFromManifest(manifest, error);
  if (default_locale.empty() ||
      locales.find(default_locale) == locales.end()) {
    *error = extension_manifest_errors::kLocalesNoDefaultLocaleSpecified;
    return NULL;
  }

  ExtensionMessageBundle* message_bundle =
      extension_l10n_util::LoadMessageCatalogs(
          locale_path,
          default_locale,
          extension_l10n_util::CurrentLocaleOrDefault(),
          locales,
          error);

  return message_bundle;
}

static bool ValidateLocaleInfo(const Extension& extension, std::string* error) {
  // default_locale and _locales have to be both present or both missing.
  const FilePath path = extension.path().AppendASCII(Extension::kLocaleFolder);
  bool path_exists = file_util::PathExists(path);
  std::string default_locale = extension.default_locale();

  // If both default locale and _locales folder are empty, skip verification.
  if (!default_locale.empty() || path_exists) {
    if (default_locale.empty() && path_exists) {
      *error = errors::kLocalesNoDefaultLocaleSpecified;
      return false;
    } else if (!default_locale.empty() && !path_exists) {
      *error = errors::kLocalesTreeMissing;
      return false;
    }

    // Treat all folders under _locales as valid locales.
    file_util::FileEnumerator locales(path,
                                      false,
                                      file_util::FileEnumerator::DIRECTORIES);

    FilePath locale_path = locales.Next();
    if (locale_path.empty()) {
      *error = errors::kLocalesTreeMissing;
      return false;
    }

    const FilePath default_locale_path = path.AppendASCII(default_locale);
    bool has_default_locale_message_file = false;
    do {
      // Skip any strings with '.'. This happens sometimes, for example with
      // '.svn' directories.
      FilePath relative_path;
      if (!extension.path().AppendRelativePath(locale_path, &relative_path))
        NOTREACHED();
      std::wstring subdir(relative_path.ToWStringHack());
      if (std::find(subdir.begin(), subdir.end(), L'.') != subdir.end())
        continue;

      FilePath messages_path =
        locale_path.AppendASCII(Extension::kMessagesFilename);

      if (!file_util::PathExists(messages_path)) {
        *error = StringPrintf(
            "%s %s", errors::kLocalesMessagesFileMissing,
            WideToUTF8(messages_path.ToWStringHack()).c_str());
        return false;
      }

      if (locale_path == default_locale_path)
        has_default_locale_message_file = true;
    } while (!(locale_path = locales.Next()).empty());

    // Only message file for default locale has to exist.
    if (!has_default_locale_message_file) {
      *error = errors::kLocalesNoDefaultMessages;
      return false;
    }
  }

  return true;
}

bool CheckForIllegalFilenames(const FilePath& extension_path,
                              std::string* error) {
  // Reserved underscore names.
  static const char* reserved_names[] = {
    Extension::kLocaleFolder,
    "__MACOSX"
  };
  static std::set<std::string> reserved_underscore_names(
    reserved_names, reserved_names + arraysize(reserved_names));

  // Enumerate all files and directories in the extension root.
  // There is a problem when using pattern "_*" with FileEnumerator, so we have
  // to cheat with find_first_of and match all.
  file_util::FileEnumerator all_files(
    extension_path,
    false,
    static_cast<file_util::FileEnumerator::FILE_TYPE>(
        file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::FILES));

  FilePath files;
  while (!(files = all_files.Next()).empty()) {
    std::string filename =
        WideToASCII(files.BaseName().ToWStringHack());
    // Skip all that don't start with "_".
    if (filename.find_first_of("_") != 0) continue;
    if (reserved_underscore_names.find(filename) ==
          reserved_underscore_names.end()) {
      *error = StringPrintf(
        "Cannot load extension with file or directory name %s. "
        "Filenames starting with \"_\" are reserved for use by the system.",
        filename.c_str());
      return false;
    }
  }

  return true;
}

}  // namespace extension_file_util
