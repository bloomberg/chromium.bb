// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CONCIERGE_CLIENT_H_
#define CHROMEOS_DBUS_CONCIERGE_CLIENT_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// ConciergeClient is used to communicate with Concierge, which is used to
// start and stop VMs.
class CHROMEOS_EXPORT ConciergeClient : public DBusClient {
 public:
  // Creates a disk image for the Termina VM.
  // |callback| is called after the method call finishes.
  virtual void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) = 0;

  // Starts a Termina VM if there is not alread one running.
  // |callback| is called after the method call finishes.
  virtual void StartTerminaVm(
      const vm_tools::concierge::StartVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) = 0;

  // Stops the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void StopVm(
      const vm_tools::concierge::StopVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback) = 0;

  // Starts a Container inside an existing Termina VM.
  // |callback| is called after the method call finishes.
  virtual void StartContainer(
      const vm_tools::concierge::StartContainerRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartContainerResponse>
          callback) = 0;

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Creates an instance of ConciergeClient.
  static ConciergeClient* Create();

  ~ConciergeClient() override;

 protected:
  // Create() should be used instead.
  ConciergeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ConciergeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CONCIERGE_CLIENT_H_
