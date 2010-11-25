// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "chrome/browser/guid.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#else
#include "chrome/browser/browser_signin.h"
#endif

namespace {

// Domain names that are known not to be managed.
// We don't register the device when such a user logs in.
const char* kNonManagedDomains[] = {
  "@googlemail.com",
  "@gmail.com"
};

// Checks the domain part of the given username against the list of known
// non-managed domain names. Returns false if |username| is empty or its
// in a domain known not to be managed.
bool CanBeInManagedDomain(const std::string& username) {
  if (username.empty()) {
    // This means incognito user in case of ChromiumOS and
    // no logged-in user in case of Chromium (SigninService).
    return false;
  }
  for (size_t i = 0; i < arraysize(kNonManagedDomains); i++) {
    if (EndsWith(username, kNonManagedDomains[i], true)) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace policy {

namespace em = enterprise_management;

DeviceTokenFetcher::DeviceTokenFetcher(
    DeviceManagementBackend* backend,
    Profile* profile,
    const FilePath& token_path)
    : profile_(profile),
      token_path_(token_path),
      backend_(backend),
      state_(kStateNotStarted),
      device_token_load_complete_event_(true, false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TokenService* token_service = profile_->GetTokenService();
  auth_token_ = token_service->GetTokenForService(
      GaiaConstants::kDeviceManagementService);

  registrar_.Add(this,
                 NotificationType::TOKEN_AVAILABLE,
                 Source<TokenService>(token_service));
  // Register for the event of user login. The device management token won't
  // be fetched until we know the domain of the currently logged in user.
#if defined(OS_CHROMEOS)
  registrar_.Add(this,
                 NotificationType::LOGIN_USER_CHANGED,
                 NotificationService::AllSources());
#else
  registrar_.Add(this,
                 NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
                 Source<Profile>(profile_));
#endif
}

void DeviceTokenFetcher::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == NotificationType::TOKEN_AVAILABLE) {
    if (Source<TokenService>(source).ptr() == profile_->GetTokenService()) {
      const TokenService::TokenAvailableDetails* token_details =
          Details<const TokenService::TokenAvailableDetails>(details).ptr();
      if (token_details->service() == GaiaConstants::kDeviceManagementService) {
        if (!HasAuthToken()) {
          auth_token_ = token_details->token();
          SendServerRequestIfPossible();
        }
      }
    }
#if defined(OS_CHROMEOS)
  } else if (type == NotificationType::LOGIN_USER_CHANGED) {
    SendServerRequestIfPossible();
#else
  } else if (type == NotificationType::GOOGLE_SIGNIN_SUCCESSFUL) {
    if (profile_ == Source<Profile>(source).ptr()) {
      SendServerRequestIfPossible();
    }
#endif
  } else {
    NOTREACHED();
  }
}

std::string DeviceTokenFetcher::GetCurrentUser() {
#if defined(OS_CHROMEOS)
  return chromeos::UserManager::Get()->logged_in_user().email();
#else
  return profile_->GetBrowserSignin()->GetSignedInUsername();
#endif
}

void DeviceTokenFetcher::HandleRegisterResponse(
    const em::DeviceRegisterResponse& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(kStateRequestingDeviceTokenFromServer, state_);
  if (response.has_device_management_token()) {
    device_token_ = response.device_management_token();
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableFunction(&WriteDeviceTokenToDisk,
                            token_path_,
                            device_token_,
                            device_id_));
    SetState(kStateHasDeviceToken);
  } else {
    NOTREACHED();
    SetState(kStateFailure);
  }
}

void DeviceTokenFetcher::OnError(DeviceManagementBackend::ErrorCode code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // For privacy reasons, delete all identifying data when this device is not
  // managed.
  if (code == DeviceManagementBackend::kErrorServiceManagementNotSupported) {
    device_token_ = std::string();
    device_id_ = std::string();
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        // The Windows compiler needs explicit template instantiation.
        NewRunnableFunction<bool(*)(const FilePath&, bool), FilePath, bool>(
            &file_util::Delete, token_path_, false));
    SetState(kStateNotManaged);
    return;
  }
  SetState(kStateFailure);
}

void DeviceTokenFetcher::Restart() {
  DCHECK(!IsTokenPending());
  device_token_.clear();
  device_token_load_complete_event_.Reset();
  MakeReadyToRequestDeviceToken();
}

void DeviceTokenFetcher::StartFetching() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == kStateNotStarted) {
    SetState(kStateLoadDeviceTokenFromDisk);
    // The file calls for loading the persisted token must be deferred to the
    // FILE thread.
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        NewRunnableMethod(this,
                          &DeviceTokenFetcher::AttemptTokenLoadFromDisk));
  }
}

void DeviceTokenFetcher::Shutdown() {
  profile_ = NULL;
  backend_ = NULL;
}

void DeviceTokenFetcher::AttemptTokenLoadFromDisk() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (file_util::PathExists(token_path_)) {
    std::string data;
    em::DeviceCredentials device_credentials;
    if (file_util::ReadFileToString(token_path_, &data) &&
        device_credentials.ParseFromArray(data.c_str(), data.size())) {
      device_token_ = device_credentials.device_token();
      device_id_ = device_credentials.device_id();
      if (!device_token_.empty() && !device_id_.empty()) {
        BrowserThread::PostTask(
            BrowserThread::UI,
            FROM_HERE,
            NewRunnableMethod(this,
                              &DeviceTokenFetcher::SetState,
                              kStateHasDeviceToken));
        return;
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      NewRunnableMethod(this,
                        &DeviceTokenFetcher::MakeReadyToRequestDeviceToken));
}

void DeviceTokenFetcher::MakeReadyToRequestDeviceToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SetState(kStateReadyToRequestDeviceTokenFromServer);
  SendServerRequestIfPossible();
}

void DeviceTokenFetcher::SendServerRequestIfPossible() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string username = GetCurrentUser();
  if (state_ == kStateReadyToRequestDeviceTokenFromServer
      && HasAuthToken()
      && backend_
      && !username.empty()) {
    if (CanBeInManagedDomain(username)) {
      em::DeviceRegisterRequest register_request;
      SetState(kStateRequestingDeviceTokenFromServer);
      backend_->ProcessRegisterRequest(auth_token_,
                                       GetDeviceID(),
                                       register_request,
                                       this);
    } else {
      SetState(kStateNotManaged);
    }
  }
}

bool DeviceTokenFetcher::IsTokenPending() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return !device_token_load_complete_event_.IsSignaled();
}

std::string DeviceTokenFetcher::GetDeviceToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_token_load_complete_event_.Wait();
  return device_token_;
}

std::string DeviceTokenFetcher::GetDeviceID() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // As long as access to this is only allowed from the UI thread, no explicit
  // locking is necessary to prevent the ID from being generated twice.
  if (device_id_.empty())
    device_id_ = GenerateNewDeviceID();
  return device_id_;
}

void DeviceTokenFetcher::SetState(FetcherState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == state)
    return;
  state_ = state;
  if (state == kStateFailure) {
    device_token_load_complete_event_.Signal();
    NotifyTokenError();
  } else if (state == kStateNotManaged) {
    device_token_load_complete_event_.Signal();
    NotifyNotManaged();
  } else if (state == kStateHasDeviceToken) {
    device_token_load_complete_event_.Signal();
    NotifyTokenSuccess();
  }
}

void DeviceTokenFetcher::GetDeviceTokenPath(FilePath* token_path) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  *token_path = token_path_;
}

bool DeviceTokenFetcher::IsTokenValid() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return state_ == kStateHasDeviceToken;
}

// static
void DeviceTokenFetcher::WriteDeviceTokenToDisk(
    const FilePath& path,
    const std::string& device_token,
    const std::string& device_id) {
  em::DeviceCredentials device_credentials;
  device_credentials.set_device_token(device_token);
  device_credentials.set_device_id(device_id);
  std::string data;
  bool no_error = device_credentials.SerializeToString(&data);
  DCHECK(no_error);
  file_util::WriteFile(path, data.c_str(), data.length());
}

// static
std::string DeviceTokenFetcher::GenerateNewDeviceID() {
  return guid::GenerateGUID();
}

}  // namespace policy
