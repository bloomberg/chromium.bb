// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration_linux.h"

#include <fcntl.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/file_util_icu.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using content::BrowserThread;

namespace {

// Helper to launch xdg scripts. We don't want them to ask any questions on the
// terminal etc. The function returns true if the utility launches and exits
// cleanly, in which case |exit_code| returns the utility's exit code.
bool LaunchXdgUtility(const std::vector<std::string>& argv, int* exit_code) {
  // xdg-settings internally runs xdg-mime, which uses mv to move newly-created
  // files on top of originals after making changes to them. In the event that
  // the original files are owned by another user (e.g. root, which can happen
  // if they are updated within sudo), mv will prompt the user to confirm if
  // standard input is a terminal (otherwise it just does it). So make sure it's
  // not, to avoid locking everything up waiting for mv.
  *exit_code = EXIT_FAILURE;
  int devnull = open("/dev/null", O_RDONLY);
  if (devnull < 0)
    return false;
  base::FileHandleMappingVector no_stdin;
  no_stdin.push_back(std::make_pair(devnull, STDIN_FILENO));

  base::ProcessHandle handle;
  base::LaunchOptions options;
  options.fds_to_remap = &no_stdin;
  if (!base::LaunchProcess(argv, options, &handle)) {
    close(devnull);
    return false;
  }
  close(devnull);

  return base::WaitForExitCode(handle, exit_code);
}

std::string CreateShortcutIcon(
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const base::FilePath& shortcut_filename) {
  if (shortcut_info.favicon.IsEmpty())
    return std::string();

  // TODO(phajdan.jr): Report errors from this function, possibly as infobars.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return std::string();

  base::FilePath temp_file_path = temp_dir.path().Append(
      shortcut_filename.ReplaceExtension("png"));
  std::string icon_name = temp_file_path.BaseName().RemoveExtension().value();

  std::vector<gfx::ImageSkiaRep> image_reps =
      shortcut_info.favicon.ToImageSkia()->image_reps();
  for (std::vector<gfx::ImageSkiaRep>::const_iterator it = image_reps.begin();
       it != image_reps.end(); ++it) {
    std::vector<unsigned char> png_data;
    const SkBitmap& bitmap = it->sk_bitmap();
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_data)) {
      // If the bitmap could not be encoded to PNG format, skip it.
      LOG(WARNING) << "Could not encode icon " << icon_name << ".png at size "
                   << bitmap.width() << ".";
      continue;
    }
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
    argv.push_back(base::IntToString(bitmap.width()));

    argv.push_back(temp_file_path.value());
    argv.push_back(icon_name);
    int exit_code;
    if (!LaunchXdgUtility(argv, &exit_code) || exit_code) {
      LOG(WARNING) << "Could not install icon " << icon_name << ".png at size "
                   << bitmap.width() << ".";
    }
  }
  return icon_name;
}

bool CreateShortcutOnDesktop(const base::FilePath& shortcut_filename,
                             const std::string& contents) {
  // Make sure that we will later call openat in a secure way.
  DCHECK_EQ(shortcut_filename.BaseName().value(), shortcut_filename.value());

  base::FilePath desktop_path;
  if (!PathService::Get(base::DIR_USER_DESKTOP, &desktop_path))
    return false;

  int desktop_fd = open(desktop_path.value().c_str(), O_RDONLY | O_DIRECTORY);
  if (desktop_fd < 0)
    return false;

  int fd = openat(desktop_fd, shortcut_filename.value().c_str(),
                  O_CREAT | O_EXCL | O_WRONLY,
                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  if (fd < 0) {
    if (HANDLE_EINTR(close(desktop_fd)) < 0)
      PLOG(ERROR) << "close";
    return false;
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

  return true;
}

void DeleteShortcutOnDesktop(const base::FilePath& shortcut_filename) {
  base::FilePath desktop_path;
  if (PathService::Get(base::DIR_USER_DESKTOP, &desktop_path))
    file_util::Delete(desktop_path.Append(shortcut_filename), false);
}

bool CreateShortcutInApplicationsMenu(const base::FilePath& shortcut_filename,
                                      const std::string& contents) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  base::FilePath temp_file_path = temp_dir.path().Append(shortcut_filename);

  int bytes_written = file_util::WriteFile(temp_file_path, contents.data(),
                                           contents.length());

  if (bytes_written != static_cast<int>(contents.length()))
    return false;

  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("install");

  // Always install in user mode, even if someone runs the browser as root
  // (people do that).
  argv.push_back("--mode");
  argv.push_back("user");

  argv.push_back(temp_file_path.value());
  int exit_code;
  LaunchXdgUtility(argv, &exit_code);
  return exit_code == 0;
}

void DeleteShortcutInApplicationsMenu(const base::FilePath& shortcut_filename) {
  std::vector<std::string> argv;
  argv.push_back("xdg-desktop-menu");
  argv.push_back("uninstall");

  // Uninstall in user mode, to match the install.
  argv.push_back("--mode");
  argv.push_back("user");

  // The file does not need to exist anywhere - xdg-desktop-menu will uninstall
  // items from the menu with a matching name.
  argv.push_back(shortcut_filename.value());
  int exit_code;
  LaunchXdgUtility(argv, &exit_code);
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

const char kDesktopEntry[] = "Desktop Entry";

const char kXdgOpenShebang[] = "#!/usr/bin/env xdg-open";

const char kXdgSettings[] = "xdg-settings";
const char kXdgSettingsDefaultBrowser[] = "default-web-browser";
const char kXdgSettingsDefaultSchemeHandler[] = "default-url-scheme-handler";

// Regex to match a localized key name such as "Name[en_AU]".
const char kLocalizedKeyRegex[] = "^[A-Za-z0-9\\-]+\\[[^\\]]*\\]$";

}  // namespace

namespace {

// Utility function to get the path to the version of a script shipped with
// Chrome. |script| gives the name of the script. |chrome_version| returns the
// path to the Chrome version of the script, and the return value of the
// function is true if the function is successful and the Chrome version is
// not the script found on the PATH.
bool GetChromeVersionOfScript(const std::string& script,
                               std::string* chrome_version) {
  // Get the path to the Chrome version.
  base::FilePath chrome_dir;
  if (!PathService::Get(base::DIR_EXE, &chrome_dir))
    return false;

  base::FilePath chrome_version_path = chrome_dir.Append(script);
  *chrome_version = chrome_version_path.value();

  // Check if this is different to the one on path.
  std::vector<std::string> argv;
  argv.push_back("which");
  argv.push_back(script);
  std::string path_version;
  if (base::GetAppOutput(CommandLine(argv), &path_version)) {
    // Remove trailing newline
    path_version.erase(path_version.length() - 1, 1);
    base::FilePath path_version_path(path_version);
    return (chrome_version_path != path_version_path);
  }
  return false;
}

// Value returned by xdg-settings if it can't understand our request.
const int EXIT_XDG_SETTINGS_SYNTAX_ERROR = 1;

// We delegate the difficulty of setting the default browser and default url
// scheme handler in Linux desktop environments to an xdg utility, xdg-settings.

// When calling this script we first try to use the script on PATH. If that
// fails we then try to use the script that we have included. This gives
// scripts on the system priority over ours, as distribution vendors may have
// tweaked the script, but still allows our copy to be used if the script on the
// system fails, as the system copy may be missing capabilities of the Chrome
// copy.

// If |protocol| is empty this function sets Chrome as the default browser,
// otherwise it sets Chrome as the default handler application for |protocol|.
bool SetDefaultWebClient(const std::string& protocol) {
#if defined(OS_CHROMEOS)
  return true;
#else
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::vector<std::string> argv;
  argv.push_back(kXdgSettings);
  argv.push_back("set");
  if (protocol.empty()) {
    argv.push_back(kXdgSettingsDefaultBrowser);
  } else {
    argv.push_back(kXdgSettingsDefaultSchemeHandler);
    argv.push_back(protocol);
  }
  argv.push_back(ShellIntegrationLinux::GetDesktopName(env.get()));

  int exit_code;
  bool ran_ok = LaunchXdgUtility(argv, &exit_code);
  if (ran_ok && exit_code == EXIT_XDG_SETTINGS_SYNTAX_ERROR) {
    if (GetChromeVersionOfScript(kXdgSettings, &argv[0])) {
      ran_ok = LaunchXdgUtility(argv, &exit_code);
    }
  }

  return ran_ok && exit_code == EXIT_SUCCESS;
#endif
}

// If |protocol| is empty this function checks if Chrome is the default browser,
// otherwise it checks if Chrome is the default handler application for
// |protocol|.
ShellIntegration::DefaultWebClientState GetIsDefaultWebClient(
    const std::string& protocol) {
#if defined(OS_CHROMEOS)
  return ShellIntegration::IS_DEFAULT;
#else
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<base::Environment> env(base::Environment::Create());

  std::vector<std::string> argv;
  argv.push_back(kXdgSettings);
  argv.push_back("check");
  if (protocol.empty()) {
    argv.push_back(kXdgSettingsDefaultBrowser);
  } else {
    argv.push_back(kXdgSettingsDefaultSchemeHandler);
    argv.push_back(protocol);
  }
  argv.push_back(ShellIntegrationLinux::GetDesktopName(env.get()));

  std::string reply;
  int success_code;
  bool ran_ok = base::GetAppOutputWithExitCode(CommandLine(argv), &reply,
                                               &success_code);
  if (ran_ok && success_code == EXIT_XDG_SETTINGS_SYNTAX_ERROR) {
    if (GetChromeVersionOfScript(kXdgSettings, &argv[0])) {
      ran_ok = base::GetAppOutputWithExitCode(CommandLine(argv), &reply,
                                              &success_code);
    }
  }

  if (!ran_ok || success_code != EXIT_SUCCESS) {
    // xdg-settings failed: we can't determine or set the default browser.
    return ShellIntegration::UNKNOWN_DEFAULT;
  }

  // Allow any reply that starts with "yes".
  return (reply.find("yes") == 0) ? ShellIntegration::IS_DEFAULT :
                                    ShellIntegration::NOT_DEFAULT;
#endif
}

} // namespace

// static
ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultBrowser() {
  return SET_DEFAULT_UNATTENDED;
}

// static
bool ShellIntegration::SetAsDefaultBrowser() {
  return SetDefaultWebClient("");
}

// static
bool ShellIntegration::SetAsDefaultProtocolClient(const std::string& protocol) {
  return SetDefaultWebClient(protocol);
}

// static
ShellIntegration::DefaultWebClientState ShellIntegration::GetDefaultBrowser() {
  return GetIsDefaultWebClient("");
}

// static
std::string ShellIntegration::GetApplicationForProtocol(const GURL& url) {
  return std::string("xdg-open");
}

// static
ShellIntegration::DefaultWebClientState
ShellIntegration::IsDefaultProtocolClient(const std::string& protocol) {
  return GetIsDefaultWebClient(protocol);
}

// static
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  std::vector<std::string> argv;
  argv.push_back(kXdgSettings);
  argv.push_back("get");
  argv.push_back(kXdgSettingsDefaultBrowser);

  std::string browser;
  // We don't care about the return value here.
  base::GetAppOutput(CommandLine(argv), &browser);
  return browser.find("irefox") != std::string::npos;
}

namespace ShellIntegrationLinux {

std::string GetDesktopName(base::Environment* env) {
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

bool GetDesktopShortcutTemplate(base::Environment* env,
                                std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<base::FilePath> search_paths;

  std::string xdg_data_home;
  if (env->GetVar("XDG_DATA_HOME", &xdg_data_home) &&
      !xdg_data_home.empty()) {
    search_paths.push_back(base::FilePath(xdg_data_home));
  }

  std::string xdg_data_dirs;
  if (env->GetVar("XDG_DATA_DIRS", &xdg_data_dirs) &&
      !xdg_data_dirs.empty()) {
    base::StringTokenizer tokenizer(xdg_data_dirs, ":");
    while (tokenizer.GetNext()) {
      base::FilePath data_dir(tokenizer.token());
      search_paths.push_back(data_dir);
      search_paths.push_back(data_dir.Append("applications"));
    }
  }

  // Add some fallback paths for systems which don't have XDG_DATA_DIRS or have
  // it incomplete.
  search_paths.push_back(base::FilePath("/usr/share/applications"));
  search_paths.push_back(base::FilePath("/usr/local/share/applications"));

  std::string template_filename(GetDesktopName(env));
  for (std::vector<base::FilePath>::const_iterator i = search_paths.begin();
       i != search_paths.end(); ++i) {
    base::FilePath path = i->Append(template_filename);
    VLOG(1) << "Looking for desktop file template in " << path.value();
    if (file_util::PathExists(path)) {
      VLOG(1) << "Found desktop file template at " << path.value();
      return file_util::ReadFileToString(path, output);
    }
  }

  LOG(ERROR) << "Could not find desktop file template.";
  return false;
}

base::FilePath GetWebShortcutFilename(const GURL& url) {
  // Use a prefix, because xdg-desktop-menu requires it.
  std::string filename =
      std::string(chrome::kBrowserProcessExecutableName) + "-" + url.spec();
  file_util::ReplaceIllegalCharactersInPath(&filename, '_');

  base::FilePath desktop_path;
  if (!PathService::Get(base::DIR_USER_DESKTOP, &desktop_path))
    return base::FilePath();

  base::FilePath filepath = desktop_path.Append(filename);
  base::FilePath alternative_filepath(filepath.value() + ".desktop");
  for (size_t i = 1; i < 100; ++i) {
    if (file_util::PathExists(base::FilePath(alternative_filepath))) {
      alternative_filepath = base::FilePath(
          filepath.value() + "_" + base::IntToString(i) + ".desktop");
    } else {
      return base::FilePath(alternative_filepath).BaseName();
    }
  }

  return base::FilePath();
}

base::FilePath GetExtensionShortcutFilename(const base::FilePath& profile_path,
                                            const std::string& extension_id) {
  DCHECK(!extension_id.empty());

  // Use a prefix, because xdg-desktop-menu requires it.
  std::string filename(chrome::kBrowserProcessExecutableName);
  filename.append("-")
      .append(extension_id)
      .append("-")
      .append(profile_path.BaseName().value());
  file_util::ReplaceIllegalCharactersInPath(&filename, '_');
  return base::FilePath(filename.append(".desktop"));
}

std::string GetDesktopFileContents(
    const std::string& template_contents,
    const std::string& app_name,
    const GURL& url,
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const string16& title,
    const std::string& icon_name,
    const base::FilePath& profile_path) {
  // Although not required by the spec, Nautilus on Ubuntu Karmic creates its
  // launchers with an xdg-open shebang. Follow that convention.
  std::string output_buffer = std::string(kXdgOpenShebang) + "\n";
  if (template_contents.empty())
    return output_buffer;

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
    return output_buffer;
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
  // Remove all localized keys.
  GRegex* localized_key_regex = g_regex_new(kLocalizedKeyRegex,
                                            static_cast<GRegexCompileFlags>(0),
                                            static_cast<GRegexMatchFlags>(0),
                                            NULL);
  gchar** keys = g_key_file_get_keys(key_file, kDesktopEntry, NULL, NULL);
  for (gchar** keys_ptr = keys; *keys_ptr; ++keys_ptr) {
    if (g_regex_match(localized_key_regex, *keys_ptr,
                      static_cast<GRegexMatchFlags>(0), NULL)) {
      g_key_file_remove_key(key_file, kDesktopEntry, *keys_ptr, NULL);
    }
  }
  g_strfreev(keys);
  g_regex_unref(localized_key_regex);

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
    base::StringTokenizer exec_tokenizer(exec_string, " ");

    std::string final_path;
    while (exec_tokenizer.GetNext() && exec_tokenizer.token() != "%U") {
      if (!final_path.empty())
        final_path += " ";
      final_path += exec_tokenizer.token();
    }
    CommandLine cmd_line(CommandLine::NO_PROGRAM);
    cmd_line = ShellIntegration::CommandLineArgsForLauncher(
        url, extension_id, profile_path);
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

#if defined(TOOLKIT_GTK)
  std::string wmclass = web_app::GetWMClassFromAppName(app_name);
  g_key_file_set_string(key_file, kDesktopEntry, "StartupWMClass",
                        wmclass.c_str());
#endif

  length = 0;
  gchar* data_dump = g_key_file_to_data(key_file, &length, NULL);
  if (data_dump) {
    // If strlen(data_dump[0]) == 0, this check will fail.
    if (data_dump[0] == '\n') {
      // Older versions of glib produce a leading newline. If this is the case,
      // remove it to avoid double-newline after the shebang.
      output_buffer += (data_dump + 1);
    } else {
      output_buffer += data_dump;
    }
    g_free(data_dump);
  }

  g_key_file_free(key_file);
  return output_buffer;
}

bool CreateDesktopShortcut(
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const std::string& shortcut_template) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath shortcut_filename;
  if (!shortcut_info.extension_id.empty()) {
    shortcut_filename = GetExtensionShortcutFilename(
        shortcut_info.profile_path, shortcut_info.extension_id);
    // For extensions we do not want duplicate shortcuts. So, delete any that
    // already exist and replace them.
    if (shortcut_info.create_on_desktop)
      DeleteShortcutOnDesktop(shortcut_filename);
    if (shortcut_info.create_in_applications_menu)
      DeleteShortcutInApplicationsMenu(shortcut_filename);
  } else {
    shortcut_filename = GetWebShortcutFilename(shortcut_info.url);
  }
  if (shortcut_filename.empty())
    return false;

  std::string icon_name = CreateShortcutIcon(shortcut_info, shortcut_filename);

  std::string app_name =
      web_app::GenerateApplicationNameFromInfo(shortcut_info);
  std::string contents = ShellIntegrationLinux::GetDesktopFileContents(
      shortcut_template,
      app_name,
      shortcut_info.url,
      shortcut_info.extension_id,
      shortcut_info.extension_path,
      shortcut_info.title,
      icon_name,
      shortcut_info.profile_path);

  bool success = true;

  if (shortcut_info.create_on_desktop)
    success = CreateShortcutOnDesktop(shortcut_filename, contents);

  if (shortcut_info.create_in_applications_menu)
    success = CreateShortcutInApplicationsMenu(shortcut_filename, contents) &&
              success;

  return success;
}

void DeleteDesktopShortcuts(const base::FilePath& profile_path,
                            const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::FilePath shortcut_filename = GetExtensionShortcutFilename(
      profile_path, extension_id);
  DCHECK(!shortcut_filename.empty());

  DeleteShortcutOnDesktop(shortcut_filename);
  DeleteShortcutInApplicationsMenu(shortcut_filename);
}

}  // namespace ShellIntegrationLinux
