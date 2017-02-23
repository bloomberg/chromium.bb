// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_CONNECTION_MANAGER_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_CONNECTION_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/components/tether/ble_advertisement_device_queue.h"
#include "chromeos/components/tether/ble_advertiser.h"
#include "chromeos/components/tether/ble_scanner.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/secure_channel.h"

namespace cryptauth {
class BluetoothThrottler;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

// Manages connections to remote devices. When a device is registered,
// BleConnectionManager intiates a connection to that device. If the connection
// succeeds and authenticates successfully, messages can be sent/received
// to/from the device. If the connection does not succeed, BleConnectionManager
// continues attempting connections until a connection is established or the
// device is unregistered.
//
// To use this class, construct an instance and observe it via AddObserver().
// Then, register device(s) to connect to via RegisterRemoteDevice() and wait
// for the OnSecureChannelStatusChanged() callback to be invoked. If the
// status for a device changes from |DISCONNECTED| to |CONNECTING| then back to
// |DISCONNECTED|, a connection attempt has failed. Clients should set a retry
// limit and unregister a device via |UnregsterRemoteDevice()| if multiple
// connection attempts have failed. If, instead, a connection succeeds the
// status changes to |AUTHENTICATED|, the device can safely send and receive
// messages. To send a message, call SendMessage(), and to listen for received
// messages, implement the OnMessageReceived() callback.
//
// Note that a single device can be registered for multiple connection reasons.
// If a device is registered for more than one reason, its connections (and
// connection attempts) will remain active until all connection reasons have
// been unregistered for the device.
class BleConnectionManager : public BleScanner::Observer {
 public:
  static std::string MessageTypeToString(const MessageType& reason);

  class Observer {
   public:
    virtual void OnSecureChannelStatusChanged(
        const cryptauth::RemoteDevice& remote_device,
        const cryptauth::SecureChannel::Status& old_status,
        const cryptauth::SecureChannel::Status& new_status) = 0;

    virtual void OnMessageReceived(const cryptauth::RemoteDevice& remote_device,
                                   const std::string& payload) = 0;
  };

  class Delegate {
   public:
    virtual std::unique_ptr<cryptauth::SecureChannel::Delegate>
    CreateSecureChannelDelegate() = 0;
  };

  BleConnectionManager(
      std::unique_ptr<Delegate> delegate,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const LocalDeviceDataProvider* local_device_data_provider,
      const cryptauth::RemoteBeaconSeedFetcher* remote_beacon_seed_fetcher,
      cryptauth::BluetoothThrottler* bluetooth_throttler);
  virtual ~BleConnectionManager();

  // Registers |remote_device| for |connection_reason|. Once registered, this
  // instance will continue to attempt to connect and authenticate to that
  // device until the device is unregistered.
  virtual void RegisterRemoteDevice(
      const cryptauth::RemoteDevice& remote_device,
      const MessageType& connection_reason);

  // Unregisters |remote_device| for |connection_reason|. Once registered, a
  // device will continue trying to connect until *ALL* of its
  // MessageTypes have been unregistered.
  virtual void UnregisterRemoteDevice(
      const cryptauth::RemoteDevice& remote_device,
      const MessageType& connection_reason);

  // Sends |message| to |remote_device|. This function can only be called if the
  // given device is authenticated.
  virtual void SendMessage(const cryptauth::RemoteDevice& remote_device,
                           const std::string& message);

  // Gets |remote_device|'s status and stores it to |status|, returning whether
  // |remote_device| is registered. If this function returns |false|, no value
  // is saved to |status|.
  virtual bool GetStatusForDevice(
      const cryptauth::RemoteDevice& remote_device,
      cryptauth::SecureChannel::Status* status) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // BleScanner::Observer:
  void OnReceivedAdvertisementFromDevice(
      const std::string& device_address,
      cryptauth::RemoteDevice remote_device) override;

 protected:
  void SendMessageReceivedEvent(const cryptauth::RemoteDevice& remote_device,
                                const std::string& payload);
  void SendSecureChannelStatusChangeEvent(
      const cryptauth::RemoteDevice& remote_device,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status);

 private:
  friend class BleConnectionManagerTest;

  static const int64_t kAdvertisingTimeoutMillis;
  static const int64_t kFailImmediatelyTimeoutMillis;

  // Data associated with a registered device. Each registered device has an
  // associated |ConnectionMetadata| stored in |device_to_metadata_map_|, and
  // the |ConnectionMetadata| is removed when the device is unregistered. A
  // |ConnectionMetadata| stores the associated |SecureChannel| for registered
  // devices which have an active connection.
  class ConnectionMetadata : public cryptauth::SecureChannel::Observer {
   public:
    ConnectionMetadata(const cryptauth::RemoteDevice remote_device,
                       std::shared_ptr<base::Timer> timer,
                       base::WeakPtr<BleConnectionManager> manager);
    ~ConnectionMetadata();

    void RegisterConnectionReason(const MessageType& connection_reason);
    void UnregisterConnectionReason(const MessageType& connection_reason);
    bool HasReasonForConnection() const;

    bool HasEstablishedConnection() const;
    cryptauth::SecureChannel::Status GetStatus() const;

    void StartConnectionAttemptTimer(bool use_short_error_timeout);
    void SetSecureChannel(
        std::unique_ptr<cryptauth::SecureChannel> secure_channel);
    void SendMessage(const std::string& payload);

    // cryptauth::SecureChannel::Observer:
    void OnSecureChannelStatusChanged(
        cryptauth::SecureChannel* secure_channel,
        const cryptauth::SecureChannel::Status& old_status,
        const cryptauth::SecureChannel::Status& new_status) override;
    void OnMessageReceived(cryptauth::SecureChannel* secure_channel,
                           const std::string& feature,
                           const std::string& payload) override;

   private:
    friend class BleConnectionManagerTest;

    void OnConnectionAttemptTimeout();

    cryptauth::RemoteDevice remote_device_;
    std::set<MessageType> active_connection_reasons_;
    std::shared_ptr<cryptauth::SecureChannel> secure_channel_;
    std::shared_ptr<base::Timer> connection_attempt_timeout_timer_;
    base::WeakPtr<BleConnectionManager> manager_;

    base::WeakPtrFactory<ConnectionMetadata> weak_ptr_factory_;
  };

  class TimerFactory {
   public:
    virtual std::unique_ptr<base::Timer> CreateTimer();
  };

  BleConnectionManager(
      std::unique_ptr<Delegate> delegate,
      scoped_refptr<device::BluetoothAdapter> adapter,
      std::unique_ptr<BleScanner> ble_scanner,
      std::unique_ptr<BleAdvertiser> ble_advertiser,
      std::unique_ptr<BleAdvertisementDeviceQueue> device_queue,
      std::unique_ptr<TimerFactory> timer_factory,
      cryptauth::BluetoothThrottler* bluetooth_throttler);

  std::shared_ptr<ConnectionMetadata> GetConnectionMetadata(
      const cryptauth::RemoteDevice& remote_device) const;
  std::shared_ptr<ConnectionMetadata> AddMetadataForDevice(
      const cryptauth::RemoteDevice& remote_device);

  void UpdateConnectionAttempts();
  void UpdateAdvertisementQueue();

  void StartConnectionAttempt(const cryptauth::RemoteDevice& remote_device);
  void StopConnectionAttemptAndMoveToEndOfQueue(
      const cryptauth::RemoteDevice& remote_device);

  void OnConnectionAttemptTimeout(const cryptauth::RemoteDevice& remote_device);
  void OnSecureChannelStatusChanged(
      const cryptauth::RemoteDevice& remote_device,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status);

  std::unique_ptr<Delegate> delegate_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<BleScanner> ble_scanner_;
  std::unique_ptr<BleAdvertiser> ble_advertiser_;
  std::unique_ptr<BleAdvertisementDeviceQueue> device_queue_;
  std::unique_ptr<TimerFactory> timer_factory_;
  cryptauth::BluetoothThrottler* bluetooth_throttler_;

  bool has_registered_observer_;
  std::map<cryptauth::RemoteDevice, std::shared_ptr<ConnectionMetadata>>
      device_to_metadata_map_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<BleConnectionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleConnectionManager);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_CONNECTION_MANAGER_H_
