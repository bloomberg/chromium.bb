// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SAFE_UTIL_WIN_H_
#define CONTENT_COMMON_SAFE_UTIL_WIN_H_

#include <string>
#include <windows.h>

class GURL;

namespace base {
class FilePath;
}

namespace content {

// Open or run a downloaded file via the Windows shell, possibly showing first
// a consent dialog if the the file is deemed dangerous. This function is an
// enhancement over the OpenItemViaShell() function of win_util.h.
//
// The user consent dialog will be shown or not according to the windows
// execution policy defined in the registry which can be overridden per user.
// The mechanics of the policy are explained in the Microsoft Knowledge base
// number 883260: http://support.microsoft.com/kb/883260
//
// The 'hwnd' is the handle to the parent window. In case a dialog is displayed
// the parent window will be disabled since the dialog is meant to be modal.
// The 'window_title' is the text displayed on the title bar of the dialog. If
// you pass an empty string the dialog will have a generic 'windows security'
// name on the title bar.
//
// You must provide a valid 'full_path' to the file to be opened and a well
// formed url in 'source_url'. The url should identify the source of the file
// but does not have to be network-reachable. If the url is malformed a
// dialog will be shown telling the user that the file will be blocked.
//
// In the event that there is no default application registered for the file
// specified by 'full_path' it ask the user, via the Windows "Open With"
// dialog.
// Returns 'true' on successful open, 'false' otherwise.
bool SaferOpenItemViaShell(HWND hwnd, const std::wstring& window_title,
                           const base::FilePath& full_path,
                           const std::wstring& source_url);

// Invokes IAttachmentExecute::Save to validate the downloaded file. The call
// may scan the file for viruses and if necessary, annotate it with evidence. As
// a result of the validation, the file may be deleted.  See:
// http://msdn.microsoft.com/en-us/bb776299
//
// If Attachment Execution Services is unavailable, then this function will
// attempt to manually annotate the file with security zone information. A
// failure code will be returned in this case even if the file is sucessfully
// annotated.
//
// IAE::Save() will delete the file if it was found to be blocked by local
// security policy or if it was found to be infected. The call may also delete
// the file due to other failures (http://crbug.com/153212). A failure code will
// be returned in these cases.
//
// Typical return values:
//   S_OK   : The file was okay. If any viruses were found, they were cleaned.
//   E_FAIL : Virus infected.
//   INET_E_SECURITY_PROBLEM : The file was blocked due to security policy.
//
// Any other return value indicates an unexpected error during the scan.
//
// |full_path| : is the path to the downloaded file. This should be the final
//               path of the download.
// |source_url|: the source URL for the download.
HRESULT ScanAndSaveDownloadedFile(const base::FilePath& full_path,
                                  const GURL& source_url);
}  // namespace content

#endif  // CONTENT_COMMON_SAFE_UTIL_WIN_H_
