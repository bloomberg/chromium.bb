// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#define CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Profile;

namespace policy {

namespace em = enterprise_management;

// Fetches the device token that can be used for policy requests with the device
// management server, either from disk if it already has been successfully
// requested, otherwise from the device management server. An instance of the
// fetcher is shared as a singleton by all users of the device management token
// to ensure they all get the same token.
class DeviceTokenFetcher
    : public NotificationObserver,
      public DeviceManagementBackend::DeviceRegisterResponseDelegate,
      public base::RefCountedThreadSafe<DeviceTokenFetcher> {
 public:
  class Observer {
   public:
    virtual void OnTokenSuccess() = 0;
    virtual void OnTokenError() = 0;
    virtual void OnNotManaged() = 0;
    virtual ~Observer() {}
  };

  class ObserverRegistrar {
   public:
    ObserverRegistrar();
    ~ObserverRegistrar();

    void Init(DeviceTokenFetcher* token_fetcher);
    void AddObserver(DeviceTokenFetcher::Observer* observer);
    void RemoveAll();
   private:
    DeviceTokenFetcher* token_fetcher_;
    std::vector<DeviceTokenFetcher::Observer*> observers_;
  };

  // Requests to the device management server are sent through |backend|. It
  // obtains the authentication token from |token_service|. The fetcher stores
  // the device token to |token_path| once it's retrieved from the server.
  DeviceTokenFetcher(DeviceManagementBackend* backend,
                     Profile* profile,
                     const FilePath& token_path);
  virtual ~DeviceTokenFetcher();

  // NotificationObserver method overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // DeviceManagementBackend::DeviceRegisterResponseDelegate method overrides:
  virtual void HandleRegisterResponse(
      const em::DeviceRegisterResponse& response);
  virtual void OnError(DeviceManagementBackend::ErrorCode code);

  // Re-initializes this DeviceTokenFetcher
  void Restart();

  // Called by subscribers of the device management token to indicate that they
  // will need the token in the future. Must be called on the UI thread.
  void StartFetching();

  // Instructs the fetcher to shut down, before the backend and token service
  // references become stale.
  void Shutdown();

  // Returns true if there is a pending token request to the device management
  // server.
  bool IsTokenPending();

  // Returns the device management token for this device, blocking until
  // outstanding requests to the device management server are satisfied.  In the
  // case that the token could not be fetched, an empty string is returned.
  std::string GetDeviceToken();

  // Returns the device ID for this device. If no such ID has been set yet, a
  // new ID is generated and returned.
  std::string GetDeviceID();

  // True if the fetcher has a valid AuthToken for the device management server.
  bool HasAuthToken() const { return !auth_token_.empty(); }

  // True if the device token has been fetched and is valid.
  bool IsTokenValid() const;

 protected:
  // Returns the email address of the currently logged-in user.
  virtual std::string GetCurrentUser();

  // Used to identify GOOGLE_SIGNIN_SUCCESSFUL notifications from the owning
  // profile, and to query for the current username.
  Profile* profile_; // weak

 private:
  friend class DeviceTokenFetcherTest;

  // The different states that the fetcher can be in during the process of
  // getting the device token.
  enum FetcherState {
    kStateNotStarted,
    kStateLoadDeviceTokenFromDisk,
    kStateReadyToRequestDeviceTokenFromServer,
    kStateRequestingDeviceTokenFromServer,
    kStateHasDeviceToken,
    kStateFailure,
    kStateNotManaged,
  };

  // Moves the fetcher into a new state. If the fetcher has the device token
  // or is moving into the failure state, callers waiting on WaitForToken
  // are unblocked.
  void SetState(FetcherState state);

  // Returns the full path to the file that persists the device manager token.
  void GetDeviceTokenPath(FilePath* token_path) const;

  // Tries to load the device token from disk. Must be called on the FILE
  // thread.
  void AttemptTokenLoadFromDisk();

  // Called if it's not possible to load the device token from disk.  Sets the
  // fetcher in a state that's ready to register the device with the device
  // management server and receive the device token in return. If the AuthToken
  // for the device management server is available, initiate the server
  // request.
  void MakeReadyToRequestDeviceToken();

  // Issues a registration request to the server if both the fetcher is in the
  // ready-to-request state and the device management server AuthToken is
  // available.
  void SendServerRequestIfPossible();

  void AddObserver(Observer* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(Observer* obs) {
    observer_list_.RemoveObserver(obs);
  }

  void NotifyTokenSuccess() {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnTokenSuccess());
  }

  void NotifyTokenError() {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnTokenError());
  }

  void NotifyNotManaged() {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnNotManaged());
  }

  // Saves the device management token to disk once it has been retrieved from
  // the server. Must be called on the FILE thread.
  static void WriteDeviceTokenToDisk(const FilePath& path,
                                     const std::string& token,
                                     const std::string& device_id);

  // Generates a new device ID used to register the device with the device
  // management server and generate the device token.
  static std::string GenerateNewDeviceID();

  ObserverList<Observer, true> observer_list_;
  FilePath token_path_;
  DeviceManagementBackend* backend_;  // weak
  FetcherState state_;
  std::string device_token_;
  std::string device_id_;

  // Contains the AuthToken for the device management server. Empty if the
  // AuthToken hasn't been issued yet or that was an error getting the
  // AuthToken.
  std::string auth_token_;

  // An event that is signaled only once the device token has been fetched
  // or it has been determined that there was an error during fetching.
  base::WaitableEvent device_token_load_complete_event_;

  // Registers the fetcher for notification of successful Gaia logins.
  NotificationRegistrar registrar_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_TOKEN_FETCHER_H_
