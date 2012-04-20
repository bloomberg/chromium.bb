// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CASHEW_CLIENT_H_
#define CHROMEOS_DBUS_CASHEW_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace base {
class ListValue;
}

namespace dbus {
class Bus;
}

namespace chromeos {

// CashewClient is used to communicate with the Cashew service.
// All methods should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT CashewClient {
 public:
  // A callback to handle "DataPlansUpdate" signal.
  // |service| is the D-Bus path of the cellular service.
  // (e.g. /service/cellular_0271266ce2ce_310260467781434)
  typedef base::Callback<void(const std::string& service,
                              const base::ListValue& data_plans)>
      DataPlansUpdateHandler;

  virtual ~CashewClient();

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static CashewClient* Create(DBusClientImplementationType type,
                              dbus::Bus* bus);

  // Sets DataPlansUpdate signal handler.
  virtual void SetDataPlansUpdateHandler(
      const DataPlansUpdateHandler& handler) = 0;

  // Resets DataPlansUpdate signal handler.
  virtual void ResetDataPlansUpdateHandler() = 0;

  // Calls RequestDataPlansUpdate method.
  virtual void RequestDataPlansUpdate(const std::string& service) = 0;

 protected:
  // Create() should be used instead.
  CashewClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CashewClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CASHEW_CLIENT_H_
