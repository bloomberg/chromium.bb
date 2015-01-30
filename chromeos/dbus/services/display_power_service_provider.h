// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICES_DISPLAY_POWER_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_SERVICES_DISPLAY_POWER_SERVICE_PROVIDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace dbus {
class MethodCall;
}

namespace chromeos {

// This class exports "SetDisplayPower" and "SetDisplaySoftwareDimming"
// D-Bus methods that the power manager calls to instruct Chrome to turn
// various displays on or off or dim them.
class CHROMEOS_EXPORT DisplayPowerServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  class Delegate {
   public:
    typedef base::Callback<void(bool)> ResponseCallback;

    virtual ~Delegate() {}

    // Sets the display power state. After the display power is set, |callback|
    // is called with the operation status.
    virtual void SetDisplayPower(DisplayPowerState power_state,
                                 const ResponseCallback& callback) = 0;

    // Dims or undims the screen.
    virtual void SetDimming(bool dimmed) = 0;
  };

  explicit DisplayPowerServiceProvider(scoped_ptr<Delegate> delegate);
  ~DisplayPowerServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Called from ExportedObject when a handler is exported as a D-Bus
  // method or failed to be exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called on UI thread in response to D-Bus requests.
  void SetDisplayPower(dbus::MethodCall* method_call,
                       dbus::ExportedObject::ResponseSender response_sender);
  void SetDisplaySoftwareDimming(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  scoped_ptr<Delegate> delegate_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<DisplayPowerServiceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPowerServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SERVICES_DISPLAY_POWER_SERVICE_PROVIDER_H_
