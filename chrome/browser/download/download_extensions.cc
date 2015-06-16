// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/browser/download/download_extensions.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"

namespace download_util {

// For file extensions taken from mozilla:

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Brodie Thiesfield <brofield@jellycan.com>
 *   Jungshik Shin <jshin@i18nl10n.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

namespace {

enum DownloadAutoOpenHint {
  ALLOW_AUTO_OPEN,

  // The file type should not be allowed to open automatically.
  //
  // Criteria for disallowing a file type from opening automatically:
  //
  // Includes file types that upon opening may either:
  //   * ... execute arbitrary or harmful code with user privileges.
  //   * ... change configuration of the system to cause harmful behavior
  //     immediately or at some time in the future.
  //
  // Doesn't include file types that upon opening:
  //   * ... sufficiently warn the user about the fact that:
  //     - This file was downloaded from the internet.
  //     - Opening it can make specified changes to the system.
  //     (Note that any such warnings need to be displayed prior to the harmful
  //     logic being executed).
  //   * ... does nothing particularly dangerous, despite the act of downloading
  //     itself being dangerous (E.g. .local and .manifest files).
  DISALLOW_AUTO_OPEN,
};

// Guidelines for adding a new dangerous file type:
//
// * Include a comment above the file type that:
//   - Describes the file type.
//   - Justifies why it is considered dangerous if this isn't obvious from the
//     description.
//   - Justifies why the file type is disallowed from auto opening, if
//     necessary.
// * Add the file extension to the kDangerousFileTypes array in
//   download_stats.cc.
//
// TODO(asanka): All file types listed below should have descriptions.
const struct FileType {
  const char* extension;  // Extension sans leading extension separator.
  DownloadDangerLevel danger_level;
  DownloadAutoOpenHint auto_open_hint;
} kDownloadFileTypes[] = {
    // Some files are dangerous on all platforms.

    // Flash files downloaded locally can sometimes access the local filesystem.
    {"swf", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"spl", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Chrome extensions should be obtained through the web store. Allowed to
    // open automatically because Chrome displays a prompt prior to
    // installation.
    {"crx", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

  // Windows, all file categories.
#if defined(OS_WIN)
    {"ad", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"ade", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"adp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"app", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft ClickOnce depolyment manifest. By default, opens with
    // dfshim.dll which should prompt the user before running untrusted code.
    {"application", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Active Server Pages source file.
    {"asp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Advanced Stream Redirector. Contains a playlist of media files.
    {"asx", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Microsoft Visual Basic source file. Opens by default in an editor.
    {"bas", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Command script.
    {"bat", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"cfg", DANGEROUS, ALLOW_AUTO_OPEN},

    // Windows Compiled HTML Help files.
    {"chi", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"chm", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Command script.
    {"cmd", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Windows legacy executable.
    {"com", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Control panel tool. Executable.
    {"cpl", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Signed certificate file.
    {"crt", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows executables.
    {"dll", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"drv", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"exe", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"fxp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"grp", DANGEROUS, ALLOW_AUTO_OPEN},

    // Windows legacy help file format.
    {"hlp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // HTML Application. Executes as a fully trusted application.
    {"hta", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Hypertext Template File. See https://support.microsoft.com/kb/181689.
    {"htt", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Device installation information.
    {"inf", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Generic configuration file.
    {"ini", DANGEROUS, ALLOW_AUTO_OPEN},

    {"ins", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"isp", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // JavaScript file. May open using Windows Script Host with user level
    // privileges.
    {"js", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"jse", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // Shortcuts. May open anything.
    {"lnk", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // .local files affect DLL search path for .exe file with same base name.
    {"local", DANGEROUS, ALLOW_AUTO_OPEN},

    {"mad", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"maf", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mag", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mam", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // While being a generic name, having a .manifest file with the same
    // basename as .exe file (foo.exe + foo.exe.manifest) changes the dll search
    // order for the .exe file. Downloading this kind of file to the users'
    // download directory is almost always the wrong thing to do.
    {"manifest", DANGEROUS, ALLOW_AUTO_OPEN},

    {"maq", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mar", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mas", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mat", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mau", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mav", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"maw", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mda", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mdb", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mde", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mdt", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mdw", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mdz", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Multipart HTML.
    {"mht", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mhtml", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    {"mmc", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mof", DANGEROUS, ALLOW_AUTO_OPEN},

    // Microsoft Management Console Snap-in. Contains executable code.
    {"msc", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"msh", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"mshxml", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Windows Installer
    {"msi", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"msp", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"mst", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // ActiveX Control
    {"ocx", DANGEROUS, ALLOW_AUTO_OPEN},

    {"ops", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"pcd", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Program Information File. Originally intended to configure execution
    // environment for legacy DOS files. They aren't meant to contain executable
    // code. But Windows may execute a PIF file that is sniffed as a PE file.
    {"pif", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"plg", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"prf", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"prg", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"pst", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Registry file. Opening may cause registry settings to change. Users still
    // need to click through a prompt. So we could consider relaxing the
    // DISALLOW_AUTO_OPEN restriction.
    {"reg", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"scf", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // These are also executables.
    {"scr", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"sct", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"shb", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"shs", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // System executable. Windows tries hard to prevent you from opening these
    // types of files.
    {"sys", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Internet Shortcut. See description for .website below.
    {"url", DANGEROUS, DISALLOW_AUTO_OPEN},

    {"vb", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // VBScript files. My open with Windows Script Host and execute with user
    // privileges.
    {"vbe", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"vbs", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    {"vsd", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"vsmacros", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"vss", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"vst", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},
    {"vsw", ALLOW_ON_USER_GESTURE, ALLOW_AUTO_OPEN},

    // Internet Shortcut (new since IE9). Both .url and .website are .ini files
    // that describe a shortcut that points to a URL. They can point at
    // anything. Dropping a download of this type and opening it automatically
    // can in effect sidestep origin restrictions etc.
    {"website", DANGEROUS, DISALLOW_AUTO_OPEN},

    // Windows Script Host related.
    {"ws", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"wsc", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"wsf", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"wsh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},

    // XAML Browser Application.
    {"xbap", DANGEROUS, DISALLOW_AUTO_OPEN},
#endif  // OS_WIN

  // Java.
#if !defined(OS_CHROMEOS)
    {"class", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"jar", DANGEROUS, DISALLOW_AUTO_OPEN},
    {"jnlp", DANGEROUS, DISALLOW_AUTO_OPEN},
#endif

  // Scripting languages. (Shells are handled below.)
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    {"pl", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"py", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"pyc", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"pyw", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"rb", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif

  // Shell languages. (OS_ANDROID is OS_POSIX.) OS_WIN shells are handled above.
#if defined(OS_POSIX)
    {"bash", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"csh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"ksh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"sh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"shar", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"tcsh", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
#if defined(OS_MACOSX)
    {"command", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif

  // Package management formats. OS_WIN package formats are handled above.
#if defined(OS_MACOSX) || defined(OS_LINUX)
    {"pkg", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
#if defined(OS_LINUX)
    {"deb", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
    {"rpm", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
#if defined(OS_ANDROID)
    {"dex", ALLOW_ON_USER_GESTURE, DISALLOW_AUTO_OPEN},
#endif
};

// FileType for files with an empty extension.
const FileType kEmptyFileType = {nullptr, NOT_DANGEROUS, DISALLOW_AUTO_OPEN};

// Default FileType for non-empty extensions that aren't in the list above.
const FileType kUnknownFileType = {nullptr, NOT_DANGEROUS, ALLOW_AUTO_OPEN};

const FileType& GetFileType(const base::FilePath& path) {
  base::FilePath::StringType extension(path.FinalExtension());
  if (extension.empty())
    return kEmptyFileType;
  if (!base::IsStringASCII(extension))
    return kUnknownFileType;
#if defined(OS_WIN)
  std::string ascii_extension = base::UTF16ToASCII(extension);
#elif defined(OS_POSIX)
  std::string ascii_extension = extension;
#endif

  // Strip out leading dot if it's still there
  if (ascii_extension[0] == base::FilePath::kExtensionSeparator)
    ascii_extension.erase(0, 1);

  for (const auto& file_type : kDownloadFileTypes) {
    if (base::LowerCaseEqualsASCII(ascii_extension, file_type.extension))
      return file_type;
  }

  return kUnknownFileType;
}

}  // namespace

DownloadDangerLevel GetFileDangerLevel(const base::FilePath& path) {
  return GetFileType(path).danger_level;
}

bool IsAllowedToOpenAutomatically(const base::FilePath& path) {
  return GetFileType(path).auto_open_hint == ALLOW_AUTO_OPEN;
}

}  // namespace download_util
