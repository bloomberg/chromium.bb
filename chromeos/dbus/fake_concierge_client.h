// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_

#include "chromeos/dbus/concierge_client.h"

namespace chromeos {

// FakeConciergeClient is a light mock of ConciergeClient used for testing.
class CHROMEOS_EXPORT FakeConciergeClient : public ConciergeClient {
 public:
  FakeConciergeClient();
  ~FakeConciergeClient() override;

  // Fake version of the method that creates a disk image for the Termina VM.
  // Sets fake_create_disk_image_called. |callback| is called after the method
  // call finishes.
  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override;

  // Fake version of the method that starts a Termina VM. Sets
  // fake_start_termina_vm_called. |callback| is called after the method call
  // finishes.
  void StartTerminaVm(const vm_tools::concierge::StartVmRequest& request,
                      DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                          callback) override;

  // Fake version of the method that stops the named Termina VM if it is
  // running. Sets fake_stop_vm_called. |callback| is called after the method
  // call finishes.
  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback)
      override;

  // Fake version of the method that starts a Container inside an existing
  // Termina VM. |callback| is called after the method call finishes.
  void StartContainer(
      const vm_tools::concierge::StartContainerRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartContainerResponse> callback)
      override;

  // Fake version of the method that waits for the Concierge service to be
  // availble.  |callback| is called after the method call finishes.
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Indicates whether CreateDiskImage has been called
  bool create_disk_image_called() const { return create_disk_image_called_; }
  // Indicates whether StartTerminaVm has been called
  bool start_termina_vm_called() const { return start_termina_vm_called_; }
  // Indicates whether StopVm has been called
  bool stop_vm_called() const { return stop_vm_called_; }

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  bool create_disk_image_called_ = false;
  bool start_termina_vm_called_ = false;
  bool stop_vm_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(FakeConciergeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
