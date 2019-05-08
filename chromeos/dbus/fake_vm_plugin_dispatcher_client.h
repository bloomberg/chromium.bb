// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_

#include "chromeos/dbus/vm_plugin_dispatcher_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeVmPluginDispatcherClient
    : public VmPluginDispatcherClient {
 public:
  FakeVmPluginDispatcherClient();
  ~FakeVmPluginDispatcherClient() override;

  void StartVm(const vm_tools::plugin_dispatcher::StartVmRequest& request,
               DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse>
                   callback) override;

  void ListVms(const vm_tools::plugin_dispatcher::ListVmRequest& request,
               DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse>
                   callback) override;

  void StopVm(const vm_tools::plugin_dispatcher::StopVmRequest& request,
              DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse>
                  callback) override;

  void SuspendVm(
      const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::SuspendVmResponse>
          callback) override;

  void ShowVm(const vm_tools::plugin_dispatcher::ShowVmRequest& request,
              DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse>
                  callback) override;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeVmPluginDispatcherClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_
