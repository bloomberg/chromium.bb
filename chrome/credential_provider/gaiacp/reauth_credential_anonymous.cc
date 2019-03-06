// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential_anonymous.h"

#include "base/files/file_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

CReauthCredentialAnonymous::CReauthCredentialAnonymous() = default;

CReauthCredentialAnonymous::~CReauthCredentialAnonymous() = default;

HRESULT CReauthCredentialAnonymous::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CReauthCredentialAnonymous::FinalRelease() {
  LOGFN(INFO);
}

HRESULT CReauthCredentialAnonymous::GetStringValueImpl(DWORD field_id,
                                                       wchar_t** value) {
  if (field_id != FID_PROVIDER_LABEL)
    return CReauthCredentialBase::GetStringValueImpl(field_id, value);

  base::string16 fullname;
  HRESULT hr = OSUserManager::Get()->GetUserFullname(
      get_os_user_domain(), get_os_username(), &fullname);

  if (FAILED(hr))
    fullname = OLE2W(get_os_username());

  return ::SHStrDupW(fullname.c_str(), value);
}

HRESULT CReauthCredentialAnonymous::GetBitmapValueImpl(DWORD field_id,
                                                       HBITMAP* phbmp) {
  if (field_id == FID_PROVIDER_LOGO) {
    base::FilePath account_picture_path;
    HRESULT hr = GetUserAccountPicturePath(OLE2W(get_os_user_sid()),
                                           &account_picture_path);
    if (SUCCEEDED(hr)) {
      base::FilePath target_picture_path = GetUserSizedAccountPictureFilePath(
          account_picture_path, kLargestProfilePictureSize,
          kCredentialLogoPictureFileExtension);

      if (base::PathExists(target_picture_path)) {
        *phbmp = static_cast<HBITMAP>(
            ::LoadImage(nullptr, target_picture_path.value().c_str(),
                        IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
        if (*phbmp) {
          return S_OK;
        } else {
          hr = HRESULT_FROM_WIN32(::GetLastError());
          LOGFN(ERROR) << "Failed load image at='"
                       << target_picture_path.value() << "' hr=" << putHR(hr);
        }
      } else {
        LOGFN(ERROR) << "Required picture at='" << target_picture_path.value()
                     << "' does not exist.";
      }
    } else {
      LOGFN(ERROR) << "Failed to get account picture known folder hr="
                   << putHR(hr);
    }
  }
  return CReauthCredentialBase::GetBitmapValueImpl(field_id, phbmp);
}
}  // namespace credential_provider
