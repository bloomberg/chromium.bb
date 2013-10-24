// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"

namespace chromeos {

class DeviceOAuth2TokenService;

class DeviceOAuth2TokenServiceFactory {
 public:
  // Callback type used for Get() function.
  typedef base::Callback<void(DeviceOAuth2TokenService*)> GetCallback;

  // Returns the instance of the DeviceOAuth2TokenService singleton via the
  // given callback. This function is asynchronous as initializing
  // DeviceOAuth2TokenService involves asynchronous D-Bus method calls.
  //
  // May return NULL during browser startup and shutdown. May also return
  // NULL if Initialize() is not called beforehand, which can happen in unit
  // tests.
  //
  // Do not hold the pointer returned by this method; call this method every
  // time and check for NULL to handle the case where this instance is
  // destroyed during shutdown.
  static void Get(const GetCallback& callback);

  // Called by ChromeBrowserMainPartsChromeOS in order to bootstrap the
  // DeviceOAuth2TokenService instance after the required global data is
  // available (local state and request context getter).
  static void Initialize();

  // Called by ChromeBrowserMainPartsChromeOS in order to shutdown the
  // DeviceOAuth2TokenService instance and cancel all in-flight requests
  // before the required global data is destroyed (local state and request
  // context getter).
  static void Shutdown();

 private:
  DeviceOAuth2TokenServiceFactory();
  ~DeviceOAuth2TokenServiceFactory();

  DeviceOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_FACTORY_H_
