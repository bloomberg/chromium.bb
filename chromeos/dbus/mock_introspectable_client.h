// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_INTROSPECTABLE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_INTROSPECTABLE_CLIENT_H_

#include "chromeos/dbus/introspectable_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockIntrospectableClient : public IntrospectableClient {
 public:
  MockIntrospectableClient();
  virtual ~MockIntrospectableClient();

  MOCK_METHOD3(Introspect, void(const std::string&,
                                const dbus::ObjectPath&,
                                const IntrospectCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_INTROSPECTABLE_CLIENT_H_
