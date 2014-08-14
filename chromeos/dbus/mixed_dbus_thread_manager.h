// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MIXED_DBUS_THREAD_MANAGER_H_
#define CHROMEOS_DBUS_MIXED_DBUS_THREAD_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

// This class permits mixing DBusThreadManagerImpl and FakeDBusThreadManager
// implementation and allow us to selectively activate some dbus clients while
// others remain stubbed out.
class CHROMEOS_EXPORT MixedDBusThreadManager : public DBusThreadManager {
 public:
  explicit MixedDBusThreadManager(
      DBusThreadManager* real_thread_manager,
      DBusThreadManager* fake_thread_manager);

  virtual ~MixedDBusThreadManager();

  // DBusThreadManager overrides.
  virtual dbus::Bus* GetSystemBus() OVERRIDE;
  virtual BluetoothAdapterClient* GetBluetoothAdapterClient() OVERRIDE;
  virtual BluetoothAgentManagerClient*
      GetBluetoothAgentManagerClient() OVERRIDE;
  virtual BluetoothDeviceClient* GetBluetoothDeviceClient() OVERRIDE;
  virtual BluetoothGattCharacteristicClient*
      GetBluetoothGattCharacteristicClient() OVERRIDE;
  virtual BluetoothGattDescriptorClient*
      GetBluetoothGattDescriptorClient() OVERRIDE;
  virtual BluetoothGattManagerClient* GetBluetoothGattManagerClient() OVERRIDE;
  virtual BluetoothGattServiceClient* GetBluetoothGattServiceClient() OVERRIDE;
  virtual BluetoothInputClient* GetBluetoothInputClient() OVERRIDE;
  virtual BluetoothProfileManagerClient*
      GetBluetoothProfileManagerClient() OVERRIDE;
  virtual CrasAudioClient* GetCrasAudioClient() OVERRIDE;
  virtual CrosDisksClient* GetCrosDisksClient() OVERRIDE;
  virtual CryptohomeClient* GetCryptohomeClient() OVERRIDE;
  virtual DebugDaemonClient* GetDebugDaemonClient() OVERRIDE;
  virtual EasyUnlockClient* GetEasyUnlockClient() OVERRIDE;
  virtual LorgnetteManagerClient* GetLorgnetteManagerClient() OVERRIDE;
  virtual ShillDeviceClient* GetShillDeviceClient() OVERRIDE;
  virtual ShillIPConfigClient* GetShillIPConfigClient() OVERRIDE;
  virtual ShillManagerClient* GetShillManagerClient() OVERRIDE;
  virtual ShillProfileClient* GetShillProfileClient() OVERRIDE;
  virtual ShillServiceClient* GetShillServiceClient() OVERRIDE;
  virtual GsmSMSClient* GetGsmSMSClient() OVERRIDE;
  virtual ImageBurnerClient* GetImageBurnerClient() OVERRIDE;
  virtual IntrospectableClient* GetIntrospectableClient() OVERRIDE;
  virtual ModemMessagingClient* GetModemMessagingClient() OVERRIDE;
  virtual NfcAdapterClient* GetNfcAdapterClient() OVERRIDE;
  virtual NfcDeviceClient* GetNfcDeviceClient() OVERRIDE;
  virtual NfcManagerClient* GetNfcManagerClient() OVERRIDE;
  virtual NfcRecordClient* GetNfcRecordClient() OVERRIDE;
  virtual NfcTagClient* GetNfcTagClient() OVERRIDE;
  virtual PermissionBrokerClient* GetPermissionBrokerClient() OVERRIDE;
  virtual PowerManagerClient* GetPowerManagerClient() OVERRIDE;
  virtual PowerPolicyController* GetPowerPolicyController() OVERRIDE;
  virtual SessionManagerClient* GetSessionManagerClient() OVERRIDE;
  virtual SMSClient* GetSMSClient() OVERRIDE;
  virtual SystemClockClient* GetSystemClockClient() OVERRIDE;
  virtual UpdateEngineClient* GetUpdateEngineClient() OVERRIDE;

 private:
  // Returns either the real or fake DBusThreadManager implementation based on
  // |client| and |unstub_mask_|.
  DBusThreadManager* GetThreadManager(DBusClientBundle::DBusClientType client);

  scoped_ptr<DBusThreadManager> real_thread_manager_;
  scoped_ptr<DBusThreadManager> fake_thread_manager_;

  DISALLOW_COPY_AND_ASSIGN(MixedDBusThreadManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MIXED_DBUS_THREAD_MANAGER_H_
