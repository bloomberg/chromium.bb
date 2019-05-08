// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace plugin_vm {

namespace {

constexpr char kPluginVmDefaultName[] = "PvmDefault";

class PluginVmManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PluginVmManager* GetForProfile(Profile* profile) {
    return static_cast<PluginVmManager*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static PluginVmManagerFactory* GetInstance() {
    static base::NoDestructor<PluginVmManagerFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<PluginVmManagerFactory>;

  PluginVmManagerFactory()
      : BrowserContextKeyedServiceFactory(
            "PluginVmManager",
            BrowserContextDependencyManager::GetInstance()) {}

  ~PluginVmManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new PluginVmManager(profile);
  }
};

}  // namespace

PluginVmManager* PluginVmManager::GetForProfile(Profile* profile) {
  return PluginVmManagerFactory::GetForProfile(profile);
}

PluginVmManager::PluginVmManager(Profile* profile)
    : profile_(profile),
      owner_id_(chromeos::ProfileHelper::GetUserIdHashFromProfile(profile)),
      weak_ptr_factory_(this) {}

PluginVmManager::~PluginVmManager() {}

void PluginVmManager::LaunchPluginVm() {
  if (!IsPluginVmAllowedForProfile(profile_)) {
    LOG(ERROR) << "Attempted to launch PluginVm when it is not allowed";
    return;
  }

  // Launching Plugin Vm goes through the following steps:
  // 1) Start the Plugin Vm Dispatcher
  // 2) Call ListVms to get the state of the VM
  // 3) Start the VM if necessary
  // 4) Show the UI.
  chromeos::DBusThreadManager::Get()
      ->GetDebugDaemonClient()
      ->StartPluginVmDispatcher(
          base::BindOnce(&PluginVmManager::OnStartPluginVmDispatcher,
                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnStartPluginVmDispatcher(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to start Plugin Vm Dispatcher.";
    return;
  }

  vm_tools::plugin_dispatcher::ListVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmDefaultName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ListVms(
      std::move(request), base::BindOnce(&PluginVmManager::OnListVms,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnListVms(
    base::Optional<vm_tools::plugin_dispatcher::ListVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to list VMs.";
    return;
  }
  if (reply->vm_info_size() != 1) {
    LOG(ERROR) << "Default VM is missing.";
    return;
  }

  auto vm_state = reply->vm_info(0).state();
  switch (vm_state) {
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STARTING:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING:
      ShowVm();
      break;
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_PAUSED:
    case vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED: {
      vm_tools::plugin_dispatcher::StartVmRequest request;
      request.set_owner_id(owner_id_);
      request.set_vm_name_uuid(kPluginVmDefaultName);

      chromeos::DBusThreadManager::Get()
          ->GetVmPluginDispatcherClient()
          ->StartVm(std::move(request),
                    base::BindOnce(&PluginVmManager::OnStartVm,
                                   weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    default:
      // TODO(timloh): We need some way to handle states like stopping, perhaps
      // we should retry after a few seconds?
      LOG(ERROR) << "Didn't start VM as it is in state " << vm_state;
      break;
  }
}

void PluginVmManager::OnStartVm(
    base::Optional<vm_tools::plugin_dispatcher::StartVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to start VM.";
    return;
  }

  ShowVm();
}

void PluginVmManager::ShowVm() {
  vm_tools::plugin_dispatcher::ShowVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name_uuid(kPluginVmDefaultName);

  chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient()->ShowVm(
      std::move(request), base::BindOnce(&PluginVmManager::OnShowVm,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmManager::OnShowVm(
    base::Optional<vm_tools::plugin_dispatcher::ShowVmResponse> reply) {
  if (!reply.has_value() || reply->error()) {
    LOG(ERROR) << "Failed to show VM.";
    return;
  }

  VLOG(1) << "ShowVm completed successfully.";
}

}  // namespace plugin_vm
