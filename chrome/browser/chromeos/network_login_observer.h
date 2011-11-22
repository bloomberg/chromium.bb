// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_LOGIN_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_LOGIN_OBSERVER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace views {
class WidgetDelegate;
}

namespace chromeos {

// The network login observer reshows a login dialog if there was an error.

class NetworkLoginObserver : public NetworkLibrary::NetworkManagerObserver {
 public:
  explicit NetworkLoginObserver(NetworkLibrary* netlib);
  virtual ~NetworkLoginObserver();

  typedef std::map<std::string, bool> NetworkFailureMap;
 private:
  void CreateModalPopup(views::WidgetDelegate* view);

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NetworkLoginObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_LOGIN_OBSERVER_H_
