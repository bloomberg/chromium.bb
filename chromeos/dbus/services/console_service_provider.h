// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICES_CONSOLE_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_SERVICES_CONSOLE_SERVICE_PROVIDER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

namespace chromeos {

// This class provides an API for external apps to notify
// chrome that it should release control of the display server.
// The main client is the console application.  This can
// also be used by crouton to take over the display.
class CHROMEOS_EXPORT ConsoleServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  class Delegate {
   public:
    typedef base::Callback<void(bool)> UpdateOwnershipCallback;

    virtual ~Delegate() {}

    // Performs the actual work needed by the provider methods with the same
    // names.
    virtual void TakeDisplayOwnership(
        const UpdateOwnershipCallback& callback) = 0;
    virtual void ReleaseDisplayOwnership(
        const UpdateOwnershipCallback& callback) = 0;
  };

  explicit ConsoleServiceProvider(scoped_ptr<Delegate> delegate);
  ~ConsoleServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // This method will get called when a external process no longer needs
  // control of the display and Chrome can take ownership.
  void TakeDisplayOwnership(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // This method will get called when a external process needs control of
  // the display and needs Chrome to release ownership.
  void ReleaseDisplayOwnership(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // This method is called when a dbus method is exported.  If the export of the
  // method is successful, |success| will be true.  It will be false
  // otherwise.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  scoped_ptr<Delegate> delegate_;
  base::WeakPtrFactory<ConsoleServiceProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SERVICES_CONSOLE_SERVICE_PROVIDER_H_
