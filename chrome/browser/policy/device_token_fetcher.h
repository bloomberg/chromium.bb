// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#define CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/non_thread_safe.h"
#include "base/waitable_event.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/browser/policy/device_management_backend.h"

namespace policy {

namespace em = enterprise_management;

// Abstracts how the path is determined where the DeviceTokenFetcher stores the
// device token once it has been returned from the server. Tests provide a mock
// implementation to the DeviceTokenFetcher that doesn't write to DIR_USER_DATA.
class StoredDeviceTokenPathProvider {
 public:
  virtual ~StoredDeviceTokenPathProvider() {}

  // Sets |path| to contain the path at which to use to store the device
  // management token file. Returns true if successful, otherwise false.
  virtual bool GetPath(FilePath* path) const = 0;
 protected:
  StoredDeviceTokenPathProvider() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(StoredDeviceTokenPathProvider);
};

// Provides a path to the device token that's inside DIR_USER_DATA.
class UserDirDeviceTokenPathProvider : public StoredDeviceTokenPathProvider {
 public:
  UserDirDeviceTokenPathProvider() {}
  virtual ~UserDirDeviceTokenPathProvider() {}
  virtual bool GetPath(FilePath* path) const;
 private:
  DISALLOW_COPY_AND_ASSIGN(UserDirDeviceTokenPathProvider);
};

// Fetches the device token that can be used for policy requests with the device
// management server, either from disk if it already has been successfully
// requested, otherwise from the device management server. An instance of the
// fetcher is shared as a singleton by all users of the device management token
// to ensure they all get the same token.
class DeviceTokenFetcher
    : public NonThreadSafe,
      public NotificationObserver,
      public DeviceManagementBackend::DeviceRegisterResponseDelegate {
 public:
  // Requests to the device management server are sent through |backend|.  The
  // DeviceTokenFetcher assumes ownership of |backend|, which is passed in
  // explicitly to simplify mocking of the backend for unit testing.  The
  // fetcher uses |path_provider| to determine the directory in which the device
  // token is stored once it's retrieved from the server. The fetcher assumes
  // ownership of |path_provider|.
  DeviceTokenFetcher(DeviceManagementBackend* backend,
                     StoredDeviceTokenPathProvider* path_provider);
  virtual ~DeviceTokenFetcher() {}

  // NotificationObserver method overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // DeviceManagementBackend::DeviceRegisterResponseDelegate method overrides:
  virtual void HandleRegisterResponse(
      const em::DeviceRegisterResponse& response);
  virtual void OnError(DeviceManagementBackend::ErrorCode code);

  // Called by subscribers of the device management token to indicate that they
  // will need the token in the future.
  void StartFetching();

  // Returns true if there is a pending token request to the device management
  // server.
  bool IsTokenPending();

  // Returns the device management token for this device, blocking until
  // outstanding requests to the device management server are satisfied.  In the
  // case that the token could not be fetched, an empty string is returned.
  std::string GetDeviceToken();

  // True if the device token has been fetched and is valid.
  bool IsTokenValid() const;

 private:
  // The different states that the fetcher can be in during the process of
  // getting the device token.
  enum FetcherState {
    kStateLoadDeviceTokenFromDisk,
    kStateFetchingAuthToken,
    kStateHasAuthToken,
    kStateHasDeviceToken,
    kStateFailure
  };

  // Moves the fetcher into a new state. If the fetcher has the device token
  // or is moving into the failure state, callers waiting on WaitForToken
  // are unblocked.
  void SetState(FetcherState state);

  // Saves the device management token to disk once it has been retrieved from
  // the server. Must be called on the FILE thread.
  static void WriteDeviceTokenToDisk(const FilePath& path,
                                     const std::string& token);

  // Returns the device ID used to register the device with the device
  // management server and generate the device token.
  static std::string GetDeviceID();

  scoped_ptr<DeviceManagementBackend> backend_;
  scoped_ptr<StoredDeviceTokenPathProvider> path_provider_;
  FetcherState state_;
  std::string device_token_;

  // An event that is signaled only once the device token has been fetched
  // or it has been determined that there was an error during fetching.
  base::WaitableEvent device_token_load_complete_event_;

  // Registers the fetcher for notification of successful Gaia logins.
  NotificationRegistrar registrar_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
