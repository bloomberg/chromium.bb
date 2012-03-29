// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_FLIMFLAM_NETWORK_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_FLIMFLAM_NETWORK_CLIENT_H_

#include "chrome/browser/chromeos/dbus/flimflam_network_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockFlimflamNetworkClient : public FlimflamNetworkClient {
 public:
  MockFlimflamNetworkClient();
  virtual ~MockFlimflamNetworkClient();

  MOCK_METHOD1(SetPropertyChangedHandler,
               void(const PropertyChangedHandler& handler));
  MOCK_METHOD0(ResetPropertyChangedHandler, void());
  MOCK_METHOD1(GetProperties, void(const DictionaryValueCallback& callback));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_FLIMFLAM_NETWORK_CLIENT_H_
