// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/browser/download/download_extensions.h"

#include "base/string_util.h"
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
  { "class", Dangerous },
  { "htm", AllowOnUserGesture },
  { "html", AllowOnUserGesture },
  { "jar", Dangerous },
  { "jnlp", Dangerous },
  { "pdf", AllowOnUserGesture },
  { "pdfxml", AllowOnUserGesture },
  { "mars", AllowOnUserGesture },
  { "fdf", AllowOnUserGesture },
  { "xfdf", AllowOnUserGesture },
  { "xdp", AllowOnUserGesture },
  { "xfd", AllowOnUserGesture },
  { "pl", AllowOnUserGesture },
  { "py", AllowOnUserGesture },
  { "rb", AllowOnUserGesture },
  { "shtm", AllowOnUserGesture },
  { "shtml", AllowOnUserGesture },
  { "svg", AllowOnUserGesture },
  { "swf", AllowOnUserGesture },
  { "xht", AllowOnUserGesture },
  { "xhtm", AllowOnUserGesture },
  { "xhtml", AllowOnUserGesture },
  { "xml", AllowOnUserGesture },
  { "xsl", AllowOnUserGesture },
  { "xslt", AllowOnUserGesture },
#if defined(OS_WIN)
  { "ad", AllowOnUserGesture },
  { "ade", AllowOnUserGesture },
  { "adp", AllowOnUserGesture },
  { "app", AllowOnUserGesture },
  { "application", AllowOnUserGesture },
  { "asp", AllowOnUserGesture },
  { "asx", AllowOnUserGesture },
  { "bas", AllowOnUserGesture },
  { "bat", AllowOnUserGesture },
  { "chi", AllowOnUserGesture },
  { "chm", AllowOnUserGesture },
  { "cmd", AllowOnUserGesture },
  { "com", AllowOnUserGesture },
  { "cpl", AllowOnUserGesture },
  { "crt", AllowOnUserGesture },
  { "dll", Dangerous },
  { "drv", Dangerous },
  { "exe", AllowOnUserGesture },
  { "fxp", AllowOnUserGesture },
  { "hlp", AllowOnUserGesture },
  { "hta", AllowOnUserGesture },
  { "htt", AllowOnUserGesture },
  { "inf", AllowOnUserGesture },
  { "ins", AllowOnUserGesture },
  { "isp", AllowOnUserGesture },
  { "js", AllowOnUserGesture },
  { "jse", AllowOnUserGesture },
  { "lnk", AllowOnUserGesture },
  { "mad", AllowOnUserGesture },
  { "maf", AllowOnUserGesture },
  { "mag", AllowOnUserGesture },
  { "mam", AllowOnUserGesture },
  { "maq", AllowOnUserGesture },
  { "mar", AllowOnUserGesture },
  { "mas", AllowOnUserGesture },
  { "mat", AllowOnUserGesture },
  { "mau", AllowOnUserGesture },
  { "mav", AllowOnUserGesture },
  { "maw", AllowOnUserGesture },
  { "mda", AllowOnUserGesture },
  { "mdb", AllowOnUserGesture },
  { "mde", AllowOnUserGesture },
  { "mdt", AllowOnUserGesture },
  { "mdw", AllowOnUserGesture },
  { "mdz", AllowOnUserGesture },
  { "mht", AllowOnUserGesture },
  { "mhtml", AllowOnUserGesture },
  { "mmc", AllowOnUserGesture },
  { "msc", AllowOnUserGesture },
  { "msh", AllowOnUserGesture },
  { "mshxml", AllowOnUserGesture },
  { "msi", AllowOnUserGesture },
  { "msp", AllowOnUserGesture },
  { "mst", AllowOnUserGesture },
  { "ocx", AllowOnUserGesture },
  { "ops", AllowOnUserGesture },
  { "pcd", AllowOnUserGesture },
  { "pif", AllowOnUserGesture },
  { "plg", AllowOnUserGesture },
  { "prf", AllowOnUserGesture },
  { "prg", AllowOnUserGesture },
  { "pst", AllowOnUserGesture },
  { "reg", AllowOnUserGesture },
  { "scf", AllowOnUserGesture },
  { "scr", AllowOnUserGesture },
  { "sct", AllowOnUserGesture },
  { "shb", AllowOnUserGesture },
  { "shs", AllowOnUserGesture },
  { "sys", Dangerous },
  { "url", AllowOnUserGesture },
  { "vb", AllowOnUserGesture },
  { "vbe", AllowOnUserGesture },
  { "vbs", AllowOnUserGesture },
  { "vsd", AllowOnUserGesture },
  { "vsmacros", AllowOnUserGesture },
  { "vss", AllowOnUserGesture },
  { "vst", AllowOnUserGesture },
  { "vsw", AllowOnUserGesture },
  { "ws", AllowOnUserGesture },
  { "wsc", AllowOnUserGesture },
  { "wsf", AllowOnUserGesture },
  { "wsh", AllowOnUserGesture },
  { "xbap", Dangerous },
#elif defined(OS_MACOSX)
  // TODO(thakis): Figure out what makes sense here -- crbug.com/19096
  { "app", AllowOnUserGesture },
  { "dmg", AllowOnUserGesture },
#elif defined(OS_POSIX)
  // TODO(estade): lengthen this list.
  { "bash", AllowOnUserGesture },
  { "csh", AllowOnUserGesture },
  { "deb", AllowOnUserGesture },
  { "exe", AllowOnUserGesture },
  { "ksh", AllowOnUserGesture },
  { "rpm", AllowOnUserGesture },
  { "sh", AllowOnUserGesture },
  { "tcsh", AllowOnUserGesture },
#endif
};

DownloadDangerLevel GetFileDangerLevel(const FilePath& path) {
  return GetFileExtensionDangerLevel(path.Extension());
}

DownloadDangerLevel GetFileExtensionDangerLevel(
    const FilePath::StringType& extension) {
  if (extension.empty())
    return NotDangerous;
  if (!IsStringASCII(extension))
    return NotDangerous;
#if defined(OS_WIN)
  std::string ascii_extension = WideToASCII(extension);
#elif defined(OS_POSIX)
  std::string ascii_extension = extension;
#endif

  // Strip out leading dot if it's still there
  if (ascii_extension[0] == FilePath::kExtensionSeparator)
    ascii_extension.erase(0, 1);

  for (size_t i = 0; i < arraysize(g_executables); ++i) {
    if (LowerCaseEqualsASCII(ascii_extension, g_executables[i].extension))
      return g_executables[i].level;
  }
  return NotDangerous;
}

bool IsFileSafe(const FilePath& path) {
  return GetFileDangerLevel(path) == NotDangerous;
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
