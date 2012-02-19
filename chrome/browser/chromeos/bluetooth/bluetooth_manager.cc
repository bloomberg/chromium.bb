// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_manager.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/dbus/bluetooth_manager_client.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "dbus/object_path.h"

namespace chromeos {

static BluetoothManager* g_bluetooth_manager = NULL;

class BluetoothManagerImpl : public BluetoothManager,
                             public BluetoothManagerClient::Observer {
 public:
  BluetoothManagerImpl() : weak_ptr_factory_(this) {
    DBusThreadManager* dbus_thread_manager = DBusThreadManager::Get();
    DCHECK(dbus_thread_manager);
    bluetooth_manager_client_ =
        dbus_thread_manager->GetBluetoothManagerClient();
    DCHECK(bluetooth_manager_client_);

    bluetooth_manager_client_->AddObserver(this);

    bluetooth_manager_client_->DefaultAdapter(
        base::Bind(&BluetoothManagerImpl::OnDefaultAdapter,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // BluetoothManager override.
  virtual void AddObserver(BluetoothManager::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  // BluetoothManager override.
  virtual void RemoveObserver(BluetoothManager::Observer* observer) OVERRIDE {
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  // BluetoothManager override.
  virtual BluetoothAdapter* DefaultAdapter() OVERRIDE {
    return default_adapter_.get();
  }

  // BluetoothManagerClient::Observer override.
  virtual void AdapterRemoved(const dbus::ObjectPath& adapter) OVERRIDE {
    if (default_adapter_.get() == NULL
        || default_adapter_->Id() != adapter.value()) {
      return;
    }
    // The default adapter was removed.
    default_adapter_.reset();
    FOR_EACH_OBSERVER(BluetoothManager::Observer, observers_,
                      DefaultAdapterChanged(default_adapter_.get()));
  }

  // BluetoothManagerClient::Observer override.
  virtual void DefaultAdapterChanged(const dbus::ObjectPath& adapter) OVERRIDE {
    OnNewDefaultAdapter(adapter);
  }

 private:
  virtual ~BluetoothManagerImpl() {
    bluetooth_manager_client_->RemoveObserver(this);
  }

  // We have updated info about the default adapter.
  void OnNewDefaultAdapter(const dbus::ObjectPath& adapter) {
    if (default_adapter_.get() != NULL
        && default_adapter_->Id() == adapter.value()) {
      return;
    }
    default_adapter_.reset(BluetoothAdapter::Create(adapter.value()));
    DCHECK(default_adapter_.get());
    FOR_EACH_OBSERVER(BluetoothManager::Observer, observers_,
                      DefaultAdapterChanged(default_adapter_.get()));
  }

  // Called by bluetooth_manager_client when our DefaultAdapter request is
  // complete
  void OnDefaultAdapter(const dbus::ObjectPath& adapter, bool success) {
    if (!success) {
      LOG(ERROR) << "OnDefaultAdapter: failed.";
      return;
    }
    OnNewDefaultAdapter(adapter);
  }

  base::WeakPtrFactory<BluetoothManagerImpl> weak_ptr_factory_;

  // Owned by the dbus thread manager. Storing this is ok only because our
  // lifetime is a subset of the thread manager's lifetime.
  BluetoothManagerClient* bluetooth_manager_client_;

  ObserverList<BluetoothManager::Observer> observers_;

  scoped_ptr<BluetoothAdapter> default_adapter_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothManagerImpl);
};

BluetoothManager::BluetoothManager() {
}

BluetoothManager::~BluetoothManager() {
}

// static
// TODO(vlaviano): allow creation of stub impl for ui testing
void BluetoothManager::Initialize() {
  VLOG(1) << "BluetoothManager::Initialize";
  DCHECK(!g_bluetooth_manager);
  g_bluetooth_manager = new BluetoothManagerImpl();
  DCHECK(g_bluetooth_manager);
}

// static
void BluetoothManager::Shutdown() {
  VLOG(1) << "BluetoothManager::Shutdown";
  if (g_bluetooth_manager) {
    delete g_bluetooth_manager;
    g_bluetooth_manager = NULL;
  }
}

// static
BluetoothManager* BluetoothManager::GetInstance() {
  return g_bluetooth_manager;
}

}  // namespace chromeos
