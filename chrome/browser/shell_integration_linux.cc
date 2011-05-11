// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <fcntl.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// Helper to launch xdg scripts. We don't want them to ask any questions on the
// terminal etc.
bool LaunchXdgUtility(const std::vector<std::string>& argv) {
  // xdg-settings internally runs xdg-mime, which uses mv to move newly-created
  // files on top of originals after making changes to them. In the event that
  // the original files are owned by another user (e.g. root, which can happen
  // if they are updated within sudo), mv will prompt the user to confirm if
  // standard input is a terminal (otherwise it just does it). So make sure it's
  // not, to avoid locking everything up waiting for mv.
  int devnull = open("/dev/null", O_RDONLY);
  if (devnull < 0)
    return false;
  base::file_handle_mapping_vector no_stdin;
  no_stdin.push_back(std::make_pair(devnull, STDIN_FILENO));

  base::ProcessHandle handle;
  if (!base::LaunchApp(argv, no_stdin, false, &handle)) {
    close(devnull);
    return false;
  }
  close(devnull);

  int success_code;
  base::WaitForExitCode(handle, &success_code);
  return success_code == EXIT_SUCCESS;
}

std::string CreateShortcutIcon(
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const FilePath& shortcut_filename) {
  if (shortcut_info.favicon.isNull())
    return std::string();

  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
  ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return std::string();

  FilePath temp_file_path = temp_dir.path().Append(
      shortcut_filename.ReplaceExtension("png"));

  std::vector<unsigned char> png_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(shortcut_info.favicon, false, &png_data);
  int bytes_written = file_util::WriteFile(temp_file_path,
      reinterpret_cast<char*>(png_data.data()), png_data.size());

  if (bytes_written != static_cast<int>(png_data.size()))
    return std::string();

  std::vector<std::string> argv;
  argv.push_back("xdg-icon-resource");
  argv.push_back("install");

  // Always install in user mode, even if someone runs the browser as root
  // (people do that).
  argv.push_back("--mode");
  argv.push_back("user");

  argv.push_back("--size");
  argv.push_back(base::IntToString(shortcut_info.favicon.width()));

  argv.push_back(temp_file_path.value());
  std::string icon_name = temp_file_path.BaseName().RemoveExtension().value();
  argv.push_back(icon_name);
  LaunchXdgUtility(argv);
  return icon_name;
}

void CreateShortcutOnDesktop(const FilePath& shortcut_filename,
                             const std::string& contents) {
  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.

  // Make sure that we will later call openat in a secure way.
  DCHECK_EQ(shortcut_filename.BaseName().value(), shortcut_filename.value());

  FilePath desktop_path;
  if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_path))
    return;

  int desktop_fd = open(desktop_path.value().c_str(), O_RDONLY | O_DIRECTORY);
  if (desktop_fd < 0)
    return;

  int fd = openat(desktop_fd, shortcut_filename.value().c_str(),
                  O_CREAT | O_EXCL | O_WRONLY,
                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  if (fd < 0) {
    if (HANDLE_EINTR(close(desktop_fd)) < 0)
      PLOG(ERROR) << "close";
    return;
  }

  ssize_t bytes_written = file_util::WriteFileDescriptor(fd, contents.data(),
                                                         contents.length());
  if (HANDLE_EINTR(close(fd)) < 0)
    PLOG(ERROR) << "close";

  if (bytes_written != static_cast<ssize_t>(contents.length())) {
    // Delete the file. No shortuct is better than corrupted one. Use unlinkat
    // to make sure we're deleting the file in the directory we think we are.
    // Even if an attacker manager to put something other at
    // |shortcut_filename| we'll just undo his action.
    unlinkat(desktop_fd, shortcut_filename.value().c_str(), 0);
  }

  if (HANDLE_EINTR(close(desktop_fd)) < 0)
    PLOG(ERROR) << "close";
}

void CreateShortcutInApplicationsMenu(const FilePath& shortcut_filename,
                                      const std::string& contents) {
  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
  ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return;

  FilePath temp_file_path = temp_dir.path().Append(shortcut_filename);

  int bytes_written = file_util::WriteFile(temp_file_path, contents.data(),
                                           contents.length());

  if (bytes_written != static_cast<int>(contents.length()))
    return;

  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("install");

  // Always install in user mode, even if someone runs the browser as root
  // (people do that).
  argv.push_back("--mode");
  argv.push_back("user");

  argv.push_back(temp_file_path.value());
  LaunchXdgUtility(argv);
}

// Quote a string such that it appears as one verbatim argument for the Exec
// key in a desktop file.
std::string QuoteArgForDesktopFileExec(const std::string& arg) {
  // http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html

  // Quoting is only necessary if the argument has a reserved character.
  if (arg.find_first_of(" \t\n\"'\\><~|&;$*?#()`") == std::string::npos)
    return arg;  // No quoting necessary.

  std::string quoted = "\"";
  for (size_t i = 0; i < arg.size(); ++i) {
    // Note that the set of backslashed characters is smaller than the
    // set of reserved characters.
    switch (arg[i]) {
      case '"':
      case '`':
      case '$':
      case '\\':
        quoted += '\\';
        break;
    }
    quoted += arg[i];
  }
  quoted += '"';

  return quoted;
}

// Remove keys from the [Desktop Entry] that would be wrong if copied verbatim
// into the new .desktop file.
const char* kDesktopKeysToDelete[] = {
  "GenericName",
  "Comment",
  "MimeType",
  "X-Ayatana-Desktop-Shortcuts",
  "StartupWMClass",
  NULL
};

const char* kDesktopEntry = "Desktop Entry";

const char* kXdgOpenShebang = "#!/usr/bin/env xdg-open";

}  // namespace

// static
std::string ShellIntegration::GetDesktopName(base::Environment* env) {
#if defined(GOOGLE_CHROME_BUILD)
  return "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  // Allow $CHROME_DESKTOP to override the built-in value, so that development
  // versions can set themselves as the default without interfering with
  // non-official, packaged versions using the built-in value.
  std::string name;
  if (env->GetVar("CHROME_DESKTOP", &name) && !name.empty())
    return name;
  return "chromium-browser.desktop";
#endif
}

// We delegate the difficulty of setting the default browser in Linux desktop
// environments to a new xdg utility, xdg-settings. We have to include a copy of
// it for this to work, obviously, but that's actually the suggested approach
// for xdg utilities anyway.

// static
bool ShellIntegration::SetAsDefaultBrowser() {
  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("set");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName(env.get()));
  return LaunchXdgUtility(argv);
}

// static
ShellIntegration::DefaultBrowserState ShellIntegration::IsDefaultBrowser() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("check");
  argv.push_back("default-web-browser");
  argv.push_back(GetDesktopName(env.get()));

  std::string reply;
  if (!base::GetAppOutput(CommandLine(argv), &reply)) {
    // xdg-settings failed: we can't determine or set the default browser.
    return UNKNOWN_DEFAULT_BROWSER;
  }

  // Allow any reply that starts with "yes".
  return (reply.find("yes") == 0) ? IS_DEFAULT_BROWSER : NOT_DEFAULT_BROWSER;
}

// static
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back("xdg-settings");
  argv.push_back("get");
  argv.push_back("default-web-browser");

  std::string browser;
  // We don't care about the return value here.
  base::GetAppOutput(CommandLine(argv), &browser);
  return browser.find("irefox") != std::string::npos;
}

// static
bool ShellIntegration::GetDesktopShortcutTemplate(
    base::Environment* env, std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<FilePath> search_paths;

  std::string xdg_data_home;
  if (env->GetVar("XDG_DATA_HOME", &xdg_data_home) &&
      !xdg_data_home.empty()) {
    search_paths.push_back(FilePath(xdg_data_home));
  }

  std::string xdg_data_dirs;
  if (env->GetVar("XDG_DATA_DIRS", &xdg_data_dirs) &&
      !xdg_data_dirs.empty()) {
    StringTokenizer tokenizer(xdg_data_dirs, ":");
    while (tokenizer.GetNext()) {
      FilePath data_dir(tokenizer.token());
      search_paths.push_back(data_dir);
      search_paths.push_back(data_dir.Append("applications"));
    }
  }

  // Add some fallback paths for systems which don't have XDG_DATA_DIRS or have
  // it incomplete.
  search_paths.push_back(FilePath("/usr/share/applications"));
  search_paths.push_back(FilePath("/usr/local/share/applications"));

  std::string template_filename(GetDesktopName(env));
  for (std::vector<FilePath>::const_iterator i = search_paths.begin();
       i != search_paths.end(); ++i) {
    FilePath path = (*i).Append(template_filename);
    VLOG(1) << "Looking for desktop file template in " << path.value();
    if (file_util::PathExists(path)) {
      VLOG(1) << "Found desktop file template at " << path.value();
      return file_util::ReadFileToString(path, output);
    }
  }

  LOG(ERROR) << "Could not find desktop file template.";
  return false;
}

// static
FilePath ShellIntegration::GetDesktopShortcutFilename(const GURL& url) {
  // Use a prefix, because xdg-desktop-menu requires it.
  std::string filename =
      std::string(chrome::kBrowserProcessExecutableName) + "-" + url.spec();
  file_util::ReplaceIllegalCharactersInPath(&filename, '_');

  FilePath desktop_path;
  if (!PathService::Get(chrome::DIR_USER_DESKTOP, &desktop_path))
    return FilePath();

  FilePath filepath = desktop_path.Append(filename);
  FilePath alternative_filepath(filepath.value() + ".desktop");
  for (size_t i = 1; i < 100; ++i) {
    if (file_util::PathExists(FilePath(alternative_filepath))) {
      alternative_filepath = FilePath(
          filepath.value() + "_" + base::IntToString(i) + ".desktop");
    } else {
      return FilePath(alternative_filepath).BaseName();
    }
  }

  return FilePath();
}

// static
std::string ShellIntegration::GetDesktopFileContents(
    const std::string& template_contents,
    const std::string& app_name,
    const GURL& url,
    const std::string& extension_id,
    const string16& title,
    const std::string& icon_name) {
  if (template_contents.empty())
    return std::string(kXdgOpenShebang) + "\n";

  // See http://standards.freedesktop.org/desktop-entry-spec/latest/
  // http://developer.gnome.org/glib/unstable/glib-Key-value-file-parser.html
  GKeyFile* key_file = g_key_file_new();
  GError* err = NULL;
  // Loading the data will strip translations and comments from the desktop
  // file (which we want to do!)
  if (!g_key_file_load_from_data(
          key_file,
          template_contents.c_str(),
          template_contents.size(),
          G_KEY_FILE_NONE,
          &err)) {
    NOTREACHED() << "Unable to read desktop file template:" << err->message;
    g_error_free(err);
    return std::string(kXdgOpenShebang) + "\n";
  }

  // Remove all sections except for the Desktop Entry
  gsize length = 0;
  gchar** groups = g_key_file_get_groups(key_file, &length);
  for (gsize i = 0; i < length; ++i) {
    if (strcmp(groups[i], kDesktopEntry) != 0) {
      g_key_file_remove_group(key_file, groups[i], NULL);
    }
  }
  g_strfreev(groups);

  // Remove keys that we won't need.
  for (const char** current_key = kDesktopKeysToDelete; *current_key;
       ++current_key) {
    g_key_file_remove_key(key_file, kDesktopEntry, *current_key, NULL);
  }

  // Set the "Name" key.
  std::string final_title = UTF16ToUTF8(title);
  // Make sure no endline characters can slip in and possibly introduce
  // additional lines (like Exec, which makes it a security risk). Also
  // use the URL as a default when the title is empty.
  if (final_title.empty() ||
      final_title.find("\n") != std::string::npos ||
      final_title.find("\r") != std::string::npos) {
    final_title = url.spec();
  }
  g_key_file_set_string(key_file, kDesktopEntry, "Name", final_title.c_str());

  // Set the "Exec" key.
  char* exec_c_string = g_key_file_get_string(key_file, kDesktopEntry, "Exec",
                                              NULL);
  if (exec_c_string) {
    std::string exec_string(exec_c_string);
    g_free(exec_c_string);
    StringTokenizer exec_tokenizer(exec_string, " ");

    std::string final_path;
    while (exec_tokenizer.GetNext() && exec_tokenizer.token() != "%U") {
      if (!final_path.empty())
        final_path += " ";
      final_path += exec_tokenizer.token();
    }
    CommandLine cmd_line =
        ShellIntegration::CommandLineArgsForLauncher(url, extension_id);
    const CommandLine::SwitchMap& switch_map = cmd_line.GetSwitches();
    for (CommandLine::SwitchMap::const_iterator i = switch_map.begin();
         i != switch_map.end(); ++i) {
      if (i->second.empty()) {
        final_path += " --" + i->first;
      } else {
        final_path += " " + QuoteArgForDesktopFileExec("--" + i->first +
                                                       "=" + i->second);
      }
    }

    g_key_file_set_string(key_file, kDesktopEntry, "Exec", final_path.c_str());
  }

  // Set the "Icon" key.
  if (!icon_name.empty())
    g_key_file_set_string(key_file, kDesktopEntry, "Icon", icon_name.c_str());

#if defined(TOOLKIT_USES_GTK)
  std::string wmclass = web_app::GetWMClassFromAppName(app_name);
  g_key_file_set_string(key_file, kDesktopEntry, "StartupWMClass",
                        wmclass.c_str());
#endif

  // Although not required by the spec, Nautilus on Ubuntu Karmic creates its
  // launchers with an xdg-open shebang. Follow that convention.
  std::string output_buffer = kXdgOpenShebang;
  length = 0;
  gchar* data_dump = g_key_file_to_data(key_file, &length, NULL);
  if (data_dump) {
    output_buffer += data_dump;
    g_free(data_dump);
  }

  g_key_file_free(key_file);
  return output_buffer;
}

// static
void ShellIntegration::CreateDesktopShortcut(
    const ShortcutInfo& shortcut_info, const std::string& shortcut_template) {
  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath shortcut_filename = GetDesktopShortcutFilename(shortcut_info.url);
  if (shortcut_filename.empty())
    return;

  std::string icon_name = CreateShortcutIcon(shortcut_info, shortcut_filename);

  std::string app_name =
      web_app::GenerateApplicationNameFromInfo(shortcut_info);
  std::string contents = GetDesktopFileContents(
      shortcut_template,
      app_name,
      shortcut_info.url,
      shortcut_info.extension_id,
      shortcut_info.title,
      icon_name);

  if (shortcut_info.create_on_desktop)
    CreateShortcutOnDesktop(shortcut_filename, contents);

  if (shortcut_info.create_in_applications_menu)
    CreateShortcutInApplicationsMenu(shortcut_filename, contents);
}
