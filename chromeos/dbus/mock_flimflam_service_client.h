// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_FLIMFLAM_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_FLIMFLAM_SERVICE_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/flimflam_service_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockFlimflamServiceClient : public FlimflamServiceClient {
 public:
  MockFlimflamServiceClient();
  virtual ~MockFlimflamServiceClient();

  MOCK_METHOD2(SetPropertyChangedHandler,
               void(const dbus::ObjectPath& service_path,
                    const PropertyChangedHandler& handler));
  MOCK_METHOD1(ResetPropertyChangedHandler,
               void(const dbus::ObjectPath& service_path));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& service_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD4(SetProperty, void(const dbus::ObjectPath& service_path,
                                 const std::string& name,
                                 const base::Value& value,
                                 const VoidCallback& callback));
  MOCK_METHOD3(ClearProperty, void(const dbus::ObjectPath& service_path,
                                   const std::string& name,
                                   const VoidCallback& callback));
  MOCK_METHOD3(Connect, void(const dbus::ObjectPath& service_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback));
  MOCK_METHOD2(Disconnect, void(const dbus::ObjectPath& service_path,
                                const VoidCallback& callback));
  MOCK_METHOD2(Remove, void(const dbus::ObjectPath& service_path,
                            const VoidCallback& callback));
  MOCK_METHOD3(ActivateCellularModem, void(const dbus::ObjectPath& service_path,
                                           const std::string& carrier,
                                           const VoidCallback& callback));
  MOCK_METHOD2(CallActivateCellularModemAndBlock,
               bool(const dbus::ObjectPath& service_path,
                    const std::string& carrier));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_FLIMFLAM_SERVICE_CLIENT_H_
