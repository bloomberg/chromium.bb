// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_MOCK_CONNECTIVITY_STATE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_NET_MOCK_CONNECTIVITY_STATE_HELPER_H_

#include "chrome/browser/chromeos/net/connectivity_state_helper.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockConnectivityStateHelper : public ConnectivityStateHelper {
 public:
  MockConnectivityStateHelper();
  virtual ~MockConnectivityStateHelper();
  MOCK_METHOD0(IsConnected, bool(void));
  MOCK_METHOD1(IsConnectedType, bool(const std::string&));
  MOCK_METHOD1(IsConnectingType, bool(const std::string&));
  MOCK_METHOD1(NetworkNameForType, std::string(const std::string&));
  MOCK_METHOD0(DefaultNetworkName, std::string(void));
  MOCK_METHOD0(DefaultNetworkOnline, bool(void));
};



}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_MOCK_CONNECTIVITY_STATE_HELPER_H_
