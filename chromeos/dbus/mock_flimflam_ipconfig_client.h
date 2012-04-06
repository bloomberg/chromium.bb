// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_FLIMFLAM_IPCONFIG_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_FLIMFLAM_IPCONFIG_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/flimflam_ipconfig_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockFlimflamIPConfigClient : public FlimflamIPConfigClient {
 public:
  MockFlimflamIPConfigClient();
  virtual ~MockFlimflamIPConfigClient();

  MOCK_METHOD1(SetPropertyChangedHandler,
               void(const PropertyChangedHandler& handler));
  MOCK_METHOD0(ResetPropertyChangedHandler, void());
  MOCK_METHOD1(GetProperties, void(const DictionaryValueCallback& callback));
  MOCK_METHOD3(SetProperty, void(const std::string& name,
                                 const base::Value& value,
                                 const VoidCallback& callback));
  MOCK_METHOD2(ClearProperty, void(const std::string& name,
                                   const VoidCallback& callback));
  MOCK_METHOD1(Remove, void(const VoidCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_FLIMFLAM_IPCONFIG_CLIENT_H_
