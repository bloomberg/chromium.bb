// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"

#include <Windows.h>

#include <atlconv.h>
#include <dpapi.h>
#include <security.h>
#include <shlobj.h>
#include <userenv.h>

#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

namespace {

// Retry count when attempting to determine if the user's OS profile has
// been created.  In slow envrionments, like VMs used for testing, it may
// take some time to create the OS profile so checks are done periodically.
// Ideally the OS would send out a notification when a profile is created and
// retrying would not be needed, but this notification does not exist.
const int kWaitForProfileCreationRetryCount = 30;

constexpr wchar_t kDefaultProfilePictureFileExtension[] = L".jpg";

constexpr int kProfilePictureSizes[] = {32, 40, 48, 96, 192, 240, 448};

std::string GetEncryptedRefreshToken(
    base::win::ScopedHandle::Handle logon_handle,
    const base::DictionaryValue& properties) {
  std::string refresh_token = GetDictStringUTF8(&properties, kKeyRefreshToken);
  if (refresh_token.empty()) {
    LOGFN(ERROR) << "Refresh token is empty";
    return std::string();
  }

  if (!::ImpersonateLoggedOnUser(logon_handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ImpersonateLoggedOnUser hr=" << putHR(hr);
    return std::string();
  }

  // Don't include null character in ciphertext.
  DATA_BLOB plaintext;
  plaintext.pbData =
      reinterpret_cast<BYTE*>(const_cast<char*>(refresh_token.c_str()));
  plaintext.cbData = static_cast<DWORD>(refresh_token.length());

  DATA_BLOB ciphertext;
  BOOL success =
      ::CryptProtectData(&plaintext, L"Gaia refresh token", nullptr, nullptr,
                         nullptr, CRYPTPROTECT_UI_FORBIDDEN, &ciphertext);
  HRESULT hr = success ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
  ::RevertToSelf();
  if (!success) {
    LOGFN(ERROR) << "CryptProtectData hr=" << putHR(hr);
    return std::string();
  }

  // NOTE: return value is binary data, not null-terminate string.
  std::string encrypted_data(reinterpret_cast<char*>(ciphertext.pbData),
                             ciphertext.cbData);
  ::LocalFree(ciphertext.pbData);
  return encrypted_data;
}

HRESULT GetBaseAccountPicturePath(base::FilePath* base_path) {
  DCHECK(base_path);
  base_path->clear();
  LPWSTR path;
  HRESULT hr = ::SHGetKnownFolderPath(FOLDERID_PublicUserTiles, 0, NULL, &path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SHGetKnownFolderPath=" << putHR(hr);
    return hr;
  }
  *base_path = base::FilePath(path);
  ::CoTaskMemFree(path);
  return S_OK;
}

HRESULT UpdateProfilePicturesForWindows8AndNewer(
    const base::string16& sid,
    const base::string16& picture_url,
    bool force_update) {
  DCHECK(!sid.empty());
  DCHECK(!picture_url.empty());
  DCHECK(base::win::GetVersion() >= base::win::VERSION_WIN8);

  // Try to download profile pictures of all required sizes for windows.
  // Needed profile picture sizes are in |kProfilePictureSizes|.
  // The way Windows8+ stores sets profile pictures is the following:
  // In |reg_utils.cc:kAccountPicturesRootRegKey| there is a registry key
  // for each resolution of profile picture needed. The keys are names
  // "Image[x]" where [x] is the resolution of the picture.
  // Each key points to a profile picture of the correct resolution on disk.
  // Generally the profile pictures are stored under:
  // FOLDERID_PublicUserTiles\\{user sid}

  base::string16 picture_url_path = base::UTF8ToUTF16(GURL(picture_url).path());
  if (picture_url_path.size() <= 1) {
    LOGFN(ERROR) << "Invalid picture url=" << picture_url;
    return E_FAIL;
  }

  base::FilePath account_picture_base_path;
  HRESULT hr = GetBaseAccountPicturePath(&account_picture_base_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed to get account picture known folder=" << putHR(hr);
    return E_FAIL;
  }

  base::FilePath account_picture_path =
      account_picture_base_path.Append(sid.c_str());

  if (!base::PathExists(account_picture_path) &&
      !base::CreateDirectory(account_picture_path)) {
    LOGFN(ERROR) << "Failed to create profile picture directory="
                 << account_picture_path;
    return E_FAIL;
  }

  constexpr wchar_t kBasePictureFilename[] = L"GoogleAccountPicture";

  base::string16 base_picture_extension = kDefaultProfilePictureFileExtension;
  base::string16 base_picture_filename = kBasePictureFilename;
  base_picture_filename += L"_";

  size_t last_period = picture_url_path.find_last_of('.');
  if (last_period != std::string::npos) {
    base_picture_extension = picture_url_path.substr(last_period);
  }

  for (auto image_size : kProfilePictureSizes) {
    base::string16 image_size_postfix = base::StringPrintf(L"%i", image_size);
    base::FilePath target_picture_path = account_picture_path.Append(
        base_picture_filename + image_size_postfix + base_picture_extension);

    // Skip if the file already exists and an update is not forced.
    if (!force_update && base::PathExists(target_picture_path)) {
      // Update the reg string for the image if it is not up to date.
      wchar_t old_picture_path[MAX_PATH];
      ULONG path_size = base::size(old_picture_path);
      HRESULT hr = GetAccountPictureRegString(sid, image_size, old_picture_path,
                                              &path_size);
      if (FAILED(hr) || target_picture_path.value() != old_picture_path) {
        HRESULT hr = SetAccountPictureRegString(sid, image_size,
                                                target_picture_path.value());
        if (FAILED(hr))
          LOGFN(ERROR) << "SetAccountPictureRegString(pic) hr=" << putHR(hr);
      }
      continue;
    }

    std::string current_picture_url = base::UTF16ToUTF8(picture_url) +
                                      base::StringPrintf("?sz=%i", image_size);

    auto fetcher = WinHttpUrlFetcher::Create(GURL(current_picture_url));
    if (!fetcher) {
      LOGFN(ERROR) << "Failed to create fetcher for=" << current_picture_url;
      continue;
    }

    std::vector<char> response;
    HRESULT hr = fetcher->Fetch(&response);
    if (FAILED(hr)) {
      LOGFN(INFO) << "fetcher.Fetch hr=" << putHR(hr);
      continue;
    }

    // Make the file visible in case it is hidden or else WriteFile will fail
    // to overwrite the existing file.
    DWORD file_attributes =
        ::GetFileAttributes(target_picture_path.value().c_str());
    if (file_attributes != INVALID_FILE_ATTRIBUTES) {
      ::SetFileAttributes(target_picture_path.value().c_str(),
                          file_attributes & ~FILE_ATTRIBUTE_HIDDEN);
    }
    if (base::WriteFile(target_picture_path, response.data(),
                        response.size()) != static_cast<int>(response.size())) {
      LOGFN(ERROR) << "Failed to write profile picture to file="
                   << target_picture_path;
      continue;
    }

    // Make the picture file hidden just like the system would normally.
    file_attributes = ::GetFileAttributes(target_picture_path.value().c_str());
    if (file_attributes != INVALID_FILE_ATTRIBUTES) {
      ::SetFileAttributes(target_picture_path.value().c_str(),
                          file_attributes | FILE_ATTRIBUTE_HIDDEN);
    }

    // Finally update the registry to point to this profile picture.
    hr = SetAccountPictureRegString(sid, image_size,
                                    target_picture_path.value());
    if (FAILED(hr))
      LOGFN(ERROR) << "SetAccountPictureRegString(pic) hr=" << putHR(hr);
  }

  return S_OK;
}

}  // namespace

// static
ScopedUserProfile::CreatorCallback*
ScopedUserProfile::GetCreatorFunctionStorage() {
  static CreatorCallback creator_for_testing;
  return &creator_for_testing;
}

// static
std::unique_ptr<ScopedUserProfile> ScopedUserProfile::Create(
    const base::string16& sid,
    const base::string16& username,
    const base::string16& password) {
  if (!GetCreatorFunctionStorage()->is_null())
    return GetCreatorFunctionStorage()->Run(sid, username, password);

  std::unique_ptr<ScopedUserProfile> scoped(
      new ScopedUserProfile(sid, username, password));
  return scoped->IsValid() ? std::move(scoped) : nullptr;
}

ScopedUserProfile::ScopedUserProfile(const base::string16& sid,
                                     const base::string16& username,
                                     const base::string16& password) {
  LOGFN(INFO);
  // Load the user's profile so that their regsitry hive is available.
  base::win::ScopedHandle::Handle handle;
  if (!::LogonUserW(username.c_str(), L".", password.c_str(),
                    LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT,
                    &handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "LogonUserW hr=" << putHR(hr);
    return;
  }
  token_.Set(handle);

  if (!WaitForProfileCreation(sid))
    token_.Close();
}

ScopedUserProfile::~ScopedUserProfile() {}

bool ScopedUserProfile::IsValid() {
  return token_.IsValid();
}

HRESULT ScopedUserProfile::ExtractAssociationInformation(
    const base::DictionaryValue& properties,
    base::string16* sid,
    base::string16* id,
    base::string16* email,
    base::string16* token_handle) {
  DCHECK(sid);
  DCHECK(id);
  DCHECK(email);
  DCHECK(token_handle);

  *sid = GetDictString(&properties, kKeySID);
  if (sid->empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  *id = GetDictString(&properties, kKeyId);
  if (id->empty()) {
    LOGFN(ERROR) << "Id is empty";
    return E_INVALIDARG;
  }

  *email = GetDictString(&properties, kKeyEmail);
  if (email->empty()) {
    LOGFN(ERROR) << "Email is empty";
    return E_INVALIDARG;
  }

  *token_handle = GetDictString(&properties, kKeyTokenHandle);
  if (token_handle->empty()) {
    LOGFN(ERROR) << "Token handle is empty";
    return E_INVALIDARG;
  }

  return S_OK;
}

HRESULT ScopedUserProfile::RegisterAssociation(
    const base::string16& sid,
    const base::string16& id,
    const base::string16& email,
    const base::string16& token_handle) {
  // Save token handle.  This handle will be used later to determine if the
  // the user has changed their password since the account was created.
  HRESULT hr = SetUserProperty(sid, kUserTokenHandle, token_handle);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(th) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, kUserId, id);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(id) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, kUserEmail, email);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(email) hr=" << putHR(hr);
    return hr;
  }

  return S_OK;
}

HRESULT ScopedUserProfile::SaveAccountInfo(
    const base::DictionaryValue& properties) {
  LOGFN(INFO);

  base::string16 sid;
  base::string16 id;
  base::string16 email;
  base::string16 token_handle;

  HRESULT hr = ExtractAssociationInformation(properties, &sid, &id, &email,
                                             &token_handle);
  if (FAILED(hr))
    return hr;

  hr = RegisterAssociation(sid, id, email, token_handle);

  if (FAILED(hr))
    return hr;

  // Write account information to the user's hive.
  // NOTE: regular users cannot access the registry entry of other users,
  // but administrators and SYSTEM can.
  {
    wchar_t key_name[128];
    swprintf_s(key_name, base::size(key_name), L"%s\\%s\\%s", sid.c_str(),
               kRegHkcuAccountsPath, id.c_str());
    LOGFN(INFO) << "HKU\\" << key_name;

    base::win::RegKey key;
    LONG sts = key.Create(HKEY_USERS, key_name, KEY_READ | KEY_WRITE);
    if (sts != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.Create(" << id << ") hr=" << putHR(hr);
      return hr;
    }

    sts = key.WriteValue(base::ASCIIToUTF16(kKeyEmail).c_str(), email.c_str());
    if (sts != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.WriteValue(" << sid << ", email) hr=" << putHR(hr);
      return hr;
    }

    // NOTE: |encrypted_data| is binary data, not null-terminate string.
    std::string encrypted_data =
        GetEncryptedRefreshToken(token_.Get(), properties);
    if (encrypted_data.empty()) {
      LOGFN(ERROR) << "GetEncryptedRefreshToken returned empty string";
      return E_UNEXPECTED;
    }

    sts = key.WriteValue(
        base::ASCIIToUTF16(kKeyRefreshToken).c_str(), encrypted_data.c_str(),
        static_cast<ULONG>(encrypted_data.length()), REG_BINARY);
    if (sts != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.WriteValue(" << sid << ", RT) hr=" << putHR(hr);
      return hr;
    }
  }

  // This code for setting profile pictures is specific for windows 8+.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    base::string16 picture_url = GetDictString(&properties, kKeyPicture);
    if (!picture_url.empty() && !sid.empty()) {
      wchar_t old_picture_url[512];
      ULONG url_size = base::size(old_picture_url);
      hr = GetUserProperty(sid, kUserPictureUrl, old_picture_url, &url_size);

      UpdateProfilePicturesForWindows8AndNewer(
          sid, picture_url, FAILED(hr) || old_picture_url != picture_url);
      hr = SetUserProperty(sid.c_str(), kUserPictureUrl, picture_url.c_str());
      if (FAILED(hr)) {
        LOGFN(ERROR) << "SetUserProperty(pic) hr=" << putHR(hr);
        return hr;
      }
    }
  }

  return S_OK;
}

ScopedUserProfile::ScopedUserProfile() {}

bool ScopedUserProfile::WaitForProfileCreation(const base::string16& sid) {
  LOGFN(INFO);
  wchar_t profile_dir[MAX_PATH];
  bool created = false;

  for (int i = 0; i < kWaitForProfileCreationRetryCount; ++i) {
    ::Sleep(1000);
    DWORD length = base::size(profile_dir);
    if (::GetUserProfileDirectoryW(token_.Get(), profile_dir, &length)) {
      LOGFN(INFO) << "GetUserProfileDirectoryW " << i << " " << profile_dir;
      created = true;
      break;
    } else {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(INFO) << "GetUserProfileDirectoryW hr=" << putHR(hr);
    }
  }

  if (!created)
    LOGFN(INFO) << "Profile not created yet???";

  created = false;

  // Write account information to the user's hive.
  // NOTE: regular users cannot access the registry entry of other users,
  // but administrators and SYSTEM can.
  base::win::RegKey key;
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\%s", sid.c_str(),
             kRegHkcuAccountsPath);
  LOGFN(INFO) << "HKU\\" << key_name;

  for (int i = 0; i < kWaitForProfileCreationRetryCount; ++i) {
    ::Sleep(1000);
    LONG sts = key.Create(HKEY_USERS, key_name, KEY_READ | KEY_WRITE);
    if (sts == ERROR_SUCCESS) {
      LOGFN(INFO) << "Registry hive created " << i;
      created = true;
      break;
    }
  }

  if (!created)
    LOGFN(ERROR) << "Profile not created really???";

  return created;
}

}  // namespace credential_provider
