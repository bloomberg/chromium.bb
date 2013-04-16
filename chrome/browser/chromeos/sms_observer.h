// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMS_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_SMS_OBSERVER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/cros/network_library.h"

class Profile;

namespace chromeos {

class CrosNetworkWatcher;
struct SMS;

// Performs monitoring of incoming SMS and shows system notifications.
class SmsObserver : public NetworkLibrary::NetworkManagerObserver {
 public:
  explicit SmsObserver(Profile* profile);
  virtual ~SmsObserver();

 private:
  typedef std::map<std::string, CrosNetworkWatcher*> ObserversMap;

  // NetworkLibrary:NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;

  void OnNewMessage(const std::string& modem_device_path, const SMS& message);

  void UpdateObservers(NetworkLibrary* library);
  void DisconnectAll();

  Profile* profile_;
  ObserversMap observers_;

  DISALLOW_COPY_AND_ASSIGN(SmsObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMS_OBSERVER_H_
