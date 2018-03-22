// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_H_

#include <atomic>
#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "chromecast/internal/public/bluetooth_v2_shlib/gatt.h"

namespace chromecast {
namespace bluetooth {

class GattClientManager;
class RemoteCharacteristic;
class RemoteDescriptor;
class RemoteService;

// A proxy to for a remote GATT server device.
class RemoteDevice : public base::RefCountedThreadSafe<RemoteDevice> {
 public:
  static constexpr int kDefaultMtu = 20;
  using StatusCallback = base::OnceCallback<void(bool)>;

  // Initiate a connection to this device. Callback will return |true| if
  // connected successfully, otherwise false. Only one pending call is allowed
  // at a time.
  void Connect(StatusCallback cb);

  // TODO(bcf): Deprecated. Replace usage with async version.
  bool ConnectSync();

  // Disconnect from this device. Callback will return |true| if disconnected
  // successfully, otherwise false. Only one pending call is allowed at a time.
  void Disconnect(StatusCallback cb);

  // TODO(bcf): Deprecated. Replace usage with async version.
  bool DisconnectSync();

  // Read this device's RSSI. The result will be sent in |callback|. Only one
  // pending call is allowed at a time.
  using RssiCallback = base::OnceCallback<void(bool success, int rssi)>;
  void ReadRemoteRssi(RssiCallback cb);

  // Request an MTU update to |mtu|. Callback will return |true| if MTU is
  // updated successfully, otherwise false. Only one pending call is allowed at
  // a time.
  void RequestMtu(int mtu, StatusCallback cb);

  // Request an update to connection parameters.
  void ConnectionParameterUpdate(int min_interval,
                                 int max_interval,
                                 int latency,
                                 int timeout,
                                 StatusCallback cb);

  // Initiate service discovery on this device. If it fails, the result will be
  // empty.
  using DiscoverServicesCb = base::OnceCallback<
      void(bool success, std::vector<scoped_refptr<RemoteService>> services)>;
  void DiscoverServices(DiscoverServicesCb cb);

  // Returns true if this device is connected.
  bool IsConnected();

  // Returns the current MTU of the connection with this device.
  int GetMtu();

  // Returns a list of all discovered services on this device. After
  // GattClientManager::Observer::OnServicesUpdated is called, these may point
  // to old services, so services need to be reobtained.
  void GetServices(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)> cb);

  // TODO(bcf): Deprecated. Replace usage with async version.
  std::vector<scoped_refptr<RemoteService>> GetServicesSync();

  // Returns the service corresponding to |uuid|, or nullptr if none exist.
  void GetServiceByUuid(
      const bluetooth_v2_shlib::Uuid& uuid,
      base::OnceCallback<void(scoped_refptr<RemoteService>)> cb);

  // TODO(bcf): Deprecated. Replace usage with async version.
  scoped_refptr<RemoteService> GetServiceByUuidSync(
      const bluetooth_v2_shlib::Uuid& uuid);

  const bluetooth_v2_shlib::Addr& addr() const { return addr_; }

 private:
  friend class GattClientManager;
  friend base::RefCountedThreadSafe<RemoteDevice>;

  RemoteDevice(const bluetooth_v2_shlib::Addr& addr,
               base::WeakPtr<GattClientManager> gatt_client_manager,
               scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteDevice();

  // Friend methods for GattClientManager
  void SetConnected(bool connected);
  void SetMtu(int mtu);

  scoped_refptr<RemoteCharacteristic> CharacteristicFromHandle(uint16_t handle);
  scoped_refptr<RemoteDescriptor> DescriptorFromHandle(uint16_t handle);

  void OnGetServices(
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services);
  void OnServicesRemoved(uint16_t start_handle, uint16_t end_handle);
  void OnServicesAdded(
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services);
  void OnReadRemoteRssiComplete(bool status, int rssi);
  // end Friend methods for GattClientManager

  const base::WeakPtr<GattClientManager> gatt_client_manager_;
  const bluetooth_v2_shlib::Addr addr_;

  // All bluetooth_v2_shlib calls are run on this task_runner. Below members
  // should only be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  bool connect_pending_ = false;
  StatusCallback connect_cb_;

  bool disconnect_pending_ = false;
  StatusCallback disconnect_cb_;

  bool rssi_pending_ = false;
  RssiCallback rssi_cb_;

  bool mtu_pending_ = false;
  StatusCallback mtu_cb_;

  bool discover_services_pending_ = false;
  DiscoverServicesCb discover_services_cb_;

  std::atomic<bool> connected_{false};
  std::atomic<int> mtu_{kDefaultMtu};
  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteService>>
      uuid_to_service_;
  std::map<uint16_t, scoped_refptr<RemoteCharacteristic>>
      handle_to_characteristic_;
  std::map<uint16_t, scoped_refptr<RemoteDescriptor>> handle_to_descriptor_;
  DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_H_
