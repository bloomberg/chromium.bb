// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMS_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SMS_OBSERVER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/cros/network_library.h"

class Profile;

namespace chromeos {

struct SMS;
class SMSHandler;

// Performs monitoring of incoming SMS and shows system notifications.
class SmsObserver : public NetworkLibrary::NetworkManagerObserver {
 public:
  explicit SmsObserver(Profile* profile);
  virtual ~SmsObserver();

 private:
  typedef std::map<std::string, SMSHandler*> ObserversMap;

  // NetworkLibrary:NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj);

  static void StaticCallback(void* object,
                             const char* modem_device_path,
                             const SMS* message);

  void OnNewMessage(const char* modem_device_path, const SMS* message);

  void UpdateObservers(NetworkLibrary* library);
  void DisconnectAll();

  Profile* profile_;
  ObserversMap observers_;

  DISALLOW_COPY_AND_ASSIGN(SmsObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMS_OBSERVER_H_
