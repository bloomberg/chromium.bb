// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_manager.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/dbus/bluetooth_manager_client.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

static BluetoothManager* g_bluetooth_manager = NULL;

class BluetoothManagerImpl : public BluetoothManager,
                             public BluetoothManagerClient::Observer {
 public:
  BluetoothManagerImpl() : weak_ptr_factory_(this) {
    DBusThreadManager* dbus_thread_manager = DBusThreadManager::Get();
    DCHECK(dbus_thread_manager);
    bluetooth_manager_client_ = dbus_thread_manager->bluetooth_manager_client();
    DCHECK(bluetooth_manager_client_);

    bluetooth_manager_client_->AddObserver(this);

    bluetooth_manager_client_->DefaultAdapter(
        base::Bind(&BluetoothManagerImpl::OnDefaultAdapter,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual void AddObserver(BluetoothManager::Observer* observer) {
    VLOG(1) << "BluetoothManager::AddObserver";
    DCHECK(observer);
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(BluetoothManager::Observer* observer) {
    VLOG(1) << "BluetoothManager::RemoveObserver";
    DCHECK(observer);
    observers_.RemoveObserver(observer);
  }

  virtual BluetoothAdapter* DefaultAdapter() {
    VLOG(1) << "BluetoothManager::DefaultAdapter";
    return default_adapter_.get();
  }

  // BluetoothManagerClient::Observer override.
  virtual void AdapterRemoved(const std::string& adapter) {
    VLOG(1) << "AdapterRemoved: " << adapter;
    if (default_adapter_.get() == NULL || default_adapter_->Id() != adapter) {
      return;
    }
    // The default adapter was removed.
    default_adapter_.reset();
    FOR_EACH_OBSERVER(BluetoothManager::Observer, observers_,
                      DefaultAdapterChanged(default_adapter_.get()));
  }

  // BluetoothManagerClient::Observer override.
  virtual void DefaultAdapterChanged(const std::string& adapter) {
    VLOG(1) << "DefaultAdapterChanged: " << adapter;
    OnNewDefaultAdapter(adapter);
  }

 private:
  virtual ~BluetoothManagerImpl() {
    bluetooth_manager_client_->RemoveObserver(this);
  }

  // We have updated info about the default adapter.
  void OnNewDefaultAdapter(const std::string& adapter) {
    VLOG(1) << "OnNewDefaultAdapter: " << adapter;
    if (default_adapter_.get() != NULL && default_adapter_->Id() == adapter) {
      return;
    }
    default_adapter_.reset(BluetoothAdapter::Create(adapter));
    DCHECK(default_adapter_.get());
    FOR_EACH_OBSERVER(BluetoothManager::Observer, observers_,
                      DefaultAdapterChanged(default_adapter_.get()));
  }

  // Called by bluetooth_manager_client when our DefaultAdapter request is
  // complete
  void OnDefaultAdapter(const std::string& adapter, bool success) {
    if (!success) {
      LOG(ERROR) << "OnDefaultAdapter: failed.";
      return;
    }
    VLOG(1) << "OnDefaultAdapter: " << adapter;
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
