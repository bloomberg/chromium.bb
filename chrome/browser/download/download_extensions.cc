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

static const struct Executables {
    const char* extension;
    DownloadDangerLevel level;
} g_executables[] = {
  // Some files are dangerous on all platforms.
  //
  // Flash files downloaded locally can sometimes access the local filesystem.
  { "swf", DANGEROUS },
  { "spl", DANGEROUS },
  // Chrome extensions should be obtained through the web store.
  { "crx", ALLOW_ON_USER_GESTURE },

  // Windows, all file categories.
#if defined(OS_WIN)
  { "ad", ALLOW_ON_USER_GESTURE },
  { "ade", ALLOW_ON_USER_GESTURE },
  { "adp", ALLOW_ON_USER_GESTURE },
  { "app", ALLOW_ON_USER_GESTURE },
  { "application", ALLOW_ON_USER_GESTURE },
  { "asp", ALLOW_ON_USER_GESTURE },
  { "asx", ALLOW_ON_USER_GESTURE },
  { "bas", ALLOW_ON_USER_GESTURE },
  { "bat", ALLOW_ON_USER_GESTURE },
  { "cfg", DANGEROUS },
  { "chi", ALLOW_ON_USER_GESTURE },
  { "chm", ALLOW_ON_USER_GESTURE },
  { "cmd", ALLOW_ON_USER_GESTURE },
  { "com", ALLOW_ON_USER_GESTURE },
  { "cpl", ALLOW_ON_USER_GESTURE },
  { "crt", ALLOW_ON_USER_GESTURE },
  { "dll", DANGEROUS },
  { "drv", DANGEROUS },
  { "exe", ALLOW_ON_USER_GESTURE },
  { "fxp", ALLOW_ON_USER_GESTURE },
  { "grp", DANGEROUS },
  { "hlp", ALLOW_ON_USER_GESTURE },
  { "hta", ALLOW_ON_USER_GESTURE },
  { "htt", ALLOW_ON_USER_GESTURE },
  { "inf", ALLOW_ON_USER_GESTURE },
  { "ini", DANGEROUS },
  { "ins", ALLOW_ON_USER_GESTURE },
  { "isp", ALLOW_ON_USER_GESTURE },
  { "js", ALLOW_ON_USER_GESTURE },
  { "jse", ALLOW_ON_USER_GESTURE },
  { "lnk", ALLOW_ON_USER_GESTURE },
  { "local", DANGEROUS },
  { "mad", ALLOW_ON_USER_GESTURE },
  { "maf", ALLOW_ON_USER_GESTURE },
  { "mag", ALLOW_ON_USER_GESTURE },
  { "mam", ALLOW_ON_USER_GESTURE },
  { "manifest", DANGEROUS },
  { "maq", ALLOW_ON_USER_GESTURE },
  { "mar", ALLOW_ON_USER_GESTURE },
  { "mas", ALLOW_ON_USER_GESTURE },
  { "mat", ALLOW_ON_USER_GESTURE },
  { "mau", ALLOW_ON_USER_GESTURE },
  { "mav", ALLOW_ON_USER_GESTURE },
  { "maw", ALLOW_ON_USER_GESTURE },
  { "mda", ALLOW_ON_USER_GESTURE },
  { "mdb", ALLOW_ON_USER_GESTURE },
  { "mde", ALLOW_ON_USER_GESTURE },
  { "mdt", ALLOW_ON_USER_GESTURE },
  { "mdw", ALLOW_ON_USER_GESTURE },
  { "mdz", ALLOW_ON_USER_GESTURE },
  { "mht", ALLOW_ON_USER_GESTURE },
  { "mhtml", ALLOW_ON_USER_GESTURE },
  { "mmc", ALLOW_ON_USER_GESTURE },
  { "mof", DANGEROUS },
  { "msc", ALLOW_ON_USER_GESTURE },
  { "msh", ALLOW_ON_USER_GESTURE },
  { "mshxml", ALLOW_ON_USER_GESTURE },
  { "msi", ALLOW_ON_USER_GESTURE },
  { "msp", ALLOW_ON_USER_GESTURE },
  { "mst", ALLOW_ON_USER_GESTURE },
  { "ocx", DANGEROUS },
  { "ops", ALLOW_ON_USER_GESTURE },
  { "pcd", ALLOW_ON_USER_GESTURE },
  { "pif", ALLOW_ON_USER_GESTURE },
  { "plg", ALLOW_ON_USER_GESTURE },
  { "prf", ALLOW_ON_USER_GESTURE },
  { "prg", ALLOW_ON_USER_GESTURE },
  { "pst", ALLOW_ON_USER_GESTURE },
  { "reg", ALLOW_ON_USER_GESTURE },
  { "scf", ALLOW_ON_USER_GESTURE },
  { "scr", ALLOW_ON_USER_GESTURE },
  { "sct", ALLOW_ON_USER_GESTURE },
  { "shb", ALLOW_ON_USER_GESTURE },
  { "shs", ALLOW_ON_USER_GESTURE },
  { "sys", DANGEROUS },
  { "url", DANGEROUS },
  { "vb", ALLOW_ON_USER_GESTURE },
  { "vbe", ALLOW_ON_USER_GESTURE },
  { "vbs", ALLOW_ON_USER_GESTURE },
  { "vsd", ALLOW_ON_USER_GESTURE },
  { "vsmacros", ALLOW_ON_USER_GESTURE },
  { "vss", ALLOW_ON_USER_GESTURE },
  { "vst", ALLOW_ON_USER_GESTURE },
  { "vsw", ALLOW_ON_USER_GESTURE },
  { "ws", ALLOW_ON_USER_GESTURE },
  { "wsc", ALLOW_ON_USER_GESTURE },
  { "wsf", ALLOW_ON_USER_GESTURE },
  { "wsh", ALLOW_ON_USER_GESTURE },
  { "xbap", DANGEROUS },
#endif  // OS_WIN

  // Java.
#if !defined(OS_CHROMEOS)
  { "class", DANGEROUS },
  { "jar", DANGEROUS },
  { "jnlp", DANGEROUS },
#endif

  // Scripting languages. (Shells are handled below.)
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  { "pl", ALLOW_ON_USER_GESTURE },
  { "py", ALLOW_ON_USER_GESTURE },
  { "pyc", ALLOW_ON_USER_GESTURE },
  { "pyw", ALLOW_ON_USER_GESTURE },
  { "rb", ALLOW_ON_USER_GESTURE },
#endif

  // Shell languages. (OS_ANDROID is OS_POSIX.) OS_WIN shells are handled above.
#if defined(OS_POSIX)
  { "bash", ALLOW_ON_USER_GESTURE },
  { "csh", ALLOW_ON_USER_GESTURE },
  { "ksh", ALLOW_ON_USER_GESTURE },
  { "sh", ALLOW_ON_USER_GESTURE },
  { "shar", ALLOW_ON_USER_GESTURE },
  { "tcsh", ALLOW_ON_USER_GESTURE },
#endif
#if defined(OS_MACOSX)
  { "command", ALLOW_ON_USER_GESTURE },
#endif

  // Package management formats. OS_WIN package formats are handled above.
#if defined(OS_MACOSX) || defined(OS_LINUX)
  { "pkg", ALLOW_ON_USER_GESTURE },
#endif
#if defined(OS_LINUX)
  { "deb", ALLOW_ON_USER_GESTURE },
  { "rpm", ALLOW_ON_USER_GESTURE },
#endif
#if defined(OS_ANDROID)
  { "apk", ALLOW_ON_USER_GESTURE },
  { "dex", ALLOW_ON_USER_GESTURE },  // Really an executable format.
#endif
};

DownloadDangerLevel GetFileDangerLevel(const base::FilePath& path) {
  base::FilePath::StringType extension(path.FinalExtension());
  if (extension.empty())
    return NOT_DANGEROUS;
  if (!base::IsStringASCII(extension))
    return NOT_DANGEROUS;
#if defined(OS_WIN)
  std::string ascii_extension = base::UTF16ToASCII(extension);
#elif defined(OS_POSIX)
  std::string ascii_extension = extension;
#endif

  // Strip out leading dot if it's still there
  if (ascii_extension[0] == base::FilePath::kExtensionSeparator)
    ascii_extension.erase(0, 1);

  for (size_t i = 0; i < arraysize(g_executables); ++i) {
    if (base::LowerCaseEqualsASCII(ascii_extension,
                                   g_executables[i].extension))
      return g_executables[i].level;
  }
  return NOT_DANGEROUS;
}

static const char* kExecutableWhiteList[] = {
  // JavaScript is just as powerful as EXE.
  "text/javascript",
  "text/javascript;version=*",
  "text/html",
  // Registry files can cause critical changes to the MS OS behavior.
  // Addition of this mimetype also addresses bug 7337.
  "text/x-registry",
  "text/x-sh",
  // Some sites use binary/octet-stream to mean application/octet-stream.
  // See http://code.google.com/p/chromium/issues/detail?id=1573
  "binary/octet-stream"
};

static const char* kExecutableBlackList[] = {
  // These application types are not executable.
  "application/*+xml",
  "application/xml"
};

bool IsExecutableMimeType(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kExecutableWhiteList); ++i) {
    if (net::MatchesMimeType(kExecutableWhiteList[i], mime_type))
      return true;
  }
  for (size_t i = 0; i < arraysize(kExecutableBlackList); ++i) {
    if (net::MatchesMimeType(kExecutableBlackList[i], mime_type))
      return false;
  }
  // We consider only other application types to be executable.
  return net::MatchesMimeType("application/*", mime_type);
}


}  // namespace download_util
