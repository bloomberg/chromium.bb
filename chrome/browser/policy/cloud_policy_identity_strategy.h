// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_IDENTITY_STRATEGY_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_IDENTITY_STRATEGY_H_
#pragma once

#include <string>

#include "base/observer_list.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace em = enterprise_management;

// Manages a device management token, i.e. an identifier that represents a
// registration with the device management service, and the associated
// credentials. Responsibilities include storing and loading the token from
// disk, observing and triggering relevant notifications.
class CloudPolicyIdentityStrategy {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Notifies observers that the effective token for fetching policy has
    // changed. The token can be queried by calling GetDeviceToken().
    virtual void OnDeviceTokenChanged() = 0;

    // Authentication credentials for talking to the device management service
    // changed. New auth data is available through GetCredentials().
    virtual void OnCredentialsChanged() = 0;
  };

  CloudPolicyIdentityStrategy();
  virtual ~CloudPolicyIdentityStrategy();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Returns the device management token, if available. Returns the empty string
  // if the device token is currently unavailable.
  virtual std::string GetDeviceToken() = 0;

  // Returns the device ID for this device. This is a unique identifier that is
  // randomly generated at registration time on the client side. It always has
  // to be sent along with the device token to the server.
  virtual std::string GetDeviceID() = 0;

  // Returns physical machine ID for this device.
  virtual std::string GetMachineID() = 0;

  // Returns physical machine model for this device.
  virtual std::string GetMachineModel() = 0;

  // Returns the policy type to be used for registering at the device management
  // server.
  virtual em::DeviceRegisterRequest_Type GetPolicyRegisterType() = 0;

  // Returns the policy type to be used for requesting policies from the device
  // management server.
  virtual std::string GetPolicyType() = 0;

  // Retrieves authentication credentials to use when talking to the device
  // management service. Returns true if the data is available and writes the
  // values to the provided pointers.
  virtual bool GetCredentials(std::string* username,
                              std::string* auth_token) = 0;

  // Notifies the identity strategy that a new token has been fetched. It is up
  // to the identity strategy to store the token, decide whether it is going
  // to be used, send out an appropriate OnDeviceTokenChanged() notification
  // and return the new token in GetDeviceToken() calls.
  virtual void OnDeviceTokenAvailable(const std::string& token) = 0;

 protected:
  // Notify observers that the effective token has changed.
  void NotifyDeviceTokenChanged();

  // Notify observers about authentication data change.
  void NotifyAuthChanged();

 private:
  ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyIdentityStrategy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_IDENTITY_STRATEGY_H_
