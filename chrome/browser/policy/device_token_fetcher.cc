// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_token_fetcher.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/policy/mock_device_management_backend.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

namespace {

static const char kPlaceholderDeviceID[] = "placeholder_device_id";

}  // namespace

namespace policy {

bool UserDirDeviceTokenPathProvider::GetPath(FilePath* path) const {
  FilePath dir_path;
  if ( PathService::Get(chrome::DIR_USER_DATA, &dir_path))
    return false;
  *path = dir_path.Append(FILE_PATH_LITERAL("DeviceManagementToken"));
  return true;
}

DeviceTokenFetcher::DeviceTokenFetcher(
    DeviceManagementBackend* backend,
    StoredDeviceTokenPathProvider* path_provider)
    : backend_(backend),
      path_provider_(path_provider),
      state_(kStateLoadDeviceTokenFromDisk),
      device_token_load_complete_event_(true, false) {
}

void DeviceTokenFetcher::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  if (type == NotificationType::TOKEN_AVAILABLE) {
    const Source<TokenService> token_service(source);
    const TokenService::TokenAvailableDetails* token_details =
        Details<const TokenService::TokenAvailableDetails>(details).ptr();
    if (token_details->service() == GaiaConstants::kDeviceManagementService &&
        state_ < kStateHasAuthToken) {
      DCHECK_EQ(kStateFetchingAuthToken, state_);
      SetState(kStateHasAuthToken);
      em::DeviceRegisterRequest register_request;
      backend_->ProcessRegisterRequest(token_details->token(),
                                       GetDeviceID(),
                                       register_request,
                                       this);
    }
  } else {
    NOTREACHED();
  }
}

void DeviceTokenFetcher::HandleRegisterResponse(
    const em::DeviceRegisterResponse& response) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(kStateHasAuthToken, state_);
  if (response.has_device_management_token()) {
    device_token_ = response.device_management_token();
    FilePath device_token_path;
    if (path_provider_->GetPath(&device_token_path)) {
      BrowserThread::PostTask(
          BrowserThread::FILE,
          FROM_HERE,
          NewRunnableFunction(&WriteDeviceTokenToDisk,
                              device_token_path,
                              device_token_));
    }
    SetState(kStateHasDeviceToken);
  } else {
    NOTREACHED();
    SetState(kStateFailure);
  }
}

void DeviceTokenFetcher::OnError(DeviceManagementBackend::ErrorCode code) {
  DCHECK(CalledOnValidThread());
  SetState(kStateFailure);
}

void DeviceTokenFetcher::StartFetching() {
  DCHECK(CalledOnValidThread());
  if (state_ < kStateHasDeviceToken) {
    FilePath device_token_path;
    FetcherState new_state = kStateFailure;
    if (path_provider_->GetPath(&device_token_path)) {
      if (file_util::PathExists(device_token_path)) {
        std::string device_token;
        if (file_util::ReadFileToString(device_token_path, &device_token_)) {
          new_state = kStateHasDeviceToken;
        }
      }
      if (new_state != kStateHasDeviceToken) {
        new_state = kStateFetchingAuthToken;
        // The policy provider gets initialized with the PrefService and Profile
        // before ServiceTokens are available. Install a notification observer
        // to ensure that the device management token gets fetched after the
        // AuthTokens are available if it's needed.
        registrar_.Add(this,
                       NotificationType::TOKEN_AVAILABLE,
                       NotificationService::AllSources());
      }
    }
    SetState(new_state);
  }
}

bool DeviceTokenFetcher::IsTokenPending() {
  DCHECK(CalledOnValidThread());
  return !device_token_load_complete_event_.IsSignaled();
}

std::string DeviceTokenFetcher::GetDeviceToken() {
  DCHECK(CalledOnValidThread());
  device_token_load_complete_event_.Wait();
  return device_token_;
}

void DeviceTokenFetcher::SetState(FetcherState state) {
  DCHECK(CalledOnValidThread());
  state_ = state;
  if (state == kStateFailure) {
    device_token_load_complete_event_.Signal();
  } else if (state == kStateHasDeviceToken) {
    device_token_load_complete_event_.Signal();
    NotificationService::current()->Notify(
        NotificationType::DEVICE_TOKEN_AVAILABLE,
        Source<DeviceTokenFetcher>(this),
        NotificationService::NoDetails());
  }
}

bool DeviceTokenFetcher::IsTokenValid() const {
  return state_ == kStateHasDeviceToken;
}

// static
void DeviceTokenFetcher::WriteDeviceTokenToDisk(
    const FilePath& path,
    const std::string& device_token) {
  file_util::WriteFile(path,
                       device_token.c_str(),
                       device_token.length());
}

// static
std::string DeviceTokenFetcher::GetDeviceID() {
  // TODO(danno): fetch a real device_id
  return kPlaceholderDeviceID;
}

}
