// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shlobj.h>
#include <shobjidl.h>

#include "content/browser/safe_util_win.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "googleurl/src/gurl.h"
#include "ui/base/win/shell.h"

namespace content {
namespace {

// This GUID is associated with any 'don't ask me again' settings that the
// user can select for different file types.
// {2676A9A2-D919-4fee-9187-152100393AB2}
static const GUID kClientID = { 0x2676a9a2, 0xd919, 0x4fee,
  { 0x91, 0x87, 0x15, 0x21, 0x0, 0x39, 0x3a, 0xb2 } };

// Sets the Zone Identifier on the file to "Internet" (3). Returns true if the
// function succeeds, false otherwise. A failure is expected on system where
// the Zone Identifier is not supported, like a machine with a FAT32 filesystem.
// This function does not invoke Windows Attachment Execution Services.
//
// |full_path| is the path to the downloaded file.
bool SetInternetZoneIdentifierDirectly(const base::FilePath& full_path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  std::wstring path = full_path.value() + L":Zone.Identifier";
  HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, kShare, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return false;

  static const char kIdentifier[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
  // Don't include trailing null in data written.
  static const DWORD kIdentifierSize = arraysize(kIdentifier) - 1;
  DWORD written = 0;
  BOOL result = WriteFile(file, kIdentifier, kIdentifierSize, &written, NULL);
  BOOL flush_result = FlushFileBuffers(file);
  CloseHandle(file);

  if (!result || !flush_result || written != kIdentifierSize) {
    NOTREACHED();
    return false;
  }

  return true;
}

}

// This function implementation is based on the attachment execution
// services functionally deployed with IE6 or Service pack 2. This
// functionality is exposed in the IAttachmentExecute COM interface.
// more information at:
// http://msdn2.microsoft.com/en-us/library/ms647048.aspx
bool SaferOpenItemViaShell(HWND hwnd, const std::wstring& window_title,
                           const base::FilePath& full_path,
                           const std::wstring& source_url) {
  base::win::ScopedComPtr<IAttachmentExecute> attachment_services;
  HRESULT hr = attachment_services.CreateInstance(CLSID_AttachmentServices);
  if (FAILED(hr)) {
    // We don't have Attachment Execution Services, it must be a pre-XP.SP2
    // Windows installation, or the thread does not have COM initialized.
    if (hr == CO_E_NOTINITIALIZED) {
      NOTREACHED();
      return false;
    }
    return ui::win::OpenItemViaShell(full_path);
  }

  attachment_services->SetClientGuid(kClientID);

  if (!window_title.empty())
    attachment_services->SetClientTitle(window_title.c_str());

  // To help windows decide if the downloaded file is dangerous we can provide
  // what the documentation calls evidence. Which we provide now:
  //
  // Set the file itself as evidence.
  hr = attachment_services->SetLocalPath(full_path.value().c_str());
  if (FAILED(hr))
    return false;
  // Set the origin URL as evidence.
  hr = attachment_services->SetSource(source_url.c_str());
  if (FAILED(hr))
    return false;

  // Now check the windows policy.
  if (attachment_services->CheckPolicy() != S_OK) {
    // It is possible that the above call returns an undocumented result
    // equal to 0x800c000e which seems to indicate that the URL failed the
    // the security check. If you proceed with the Prompt() call the
    // Shell might show a dialog that says:
    // "windows found that this file is potentially harmful. To help protect
    // your computer, Windows has blocked access to this file."
    // Upon dismissal of the dialog windows will delete the file (!!).
    // So, we can 'return' in that case but maybe is best to let it happen to
    // fail on the safe side.

    ATTACHMENT_ACTION action;
    // We cannot control what the prompt says or does directly but it
    // is a pretty decent dialog; for example, if an executable is signed it can
    // decode and show the publisher and the certificate.
    hr = attachment_services->Prompt(hwnd, ATTACHMENT_PROMPT_EXEC, &action);
    if (FAILED(hr) || (ATTACHMENT_ACTION_CANCEL == action)) {
      // The user has declined opening the item.
      return false;
    }
  }
  return ui::win::OpenItemViaShellNoZoneCheck(full_path);
}

HRESULT ScanAndSaveDownloadedFile(const base::FilePath& full_path,
                                  const GURL& source_url) {
  base::win::ScopedComPtr<IAttachmentExecute> attachment_services;
  HRESULT hr = attachment_services.CreateInstance(CLSID_AttachmentServices);

  if (FAILED(hr)) {
    // The thread must have COM initialized.
    DCHECK_NE(CO_E_NOTINITIALIZED, hr);

    // We don't have Attachment Execution Services, it must be a pre-XP.SP2
    // Windows installation, or the thread does not have COM initialized. Try to
    // set the zone information directly. Failure is not considered an error.
    SetInternetZoneIdentifierDirectly(full_path);
    return hr;
  }

  hr = attachment_services->SetClientGuid(kClientID);
  if (FAILED(hr))
    return hr;

  hr = attachment_services->SetLocalPath(full_path.value().c_str());
  if (FAILED(hr))
    return hr;

  hr = attachment_services->SetSource(UTF8ToWide(source_url.spec()).c_str());
  if (FAILED(hr))
    return hr;

  // A failure in the Save() call below could result in the downloaded file
  // being deleted.
  return attachment_services->Save();
}

}  // namespace content
