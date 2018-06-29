// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_IMPL_H_

#include <atomic>
#include <deque>
#include <map>
#include <queue>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/device/bluetooth/le/remote_device.h"

namespace chromecast {
namespace bluetooth {

class GattClientManagerImpl;
class RemoteCharacteristicImpl;
class RemoteDescriptorImpl;

class RemoteDeviceImpl : public RemoteDevice {
 public:
  // RemoteDevice implementation
  void Connect(StatusCallback cb) override;
  bool ConnectSync() override;
  void Disconnect(StatusCallback cb) override;
  bool DisconnectSync() override;
  void ReadRemoteRssi(RssiCallback cb) override;
  void RequestMtu(int mtu, StatusCallback cb) override;
  void ConnectionParameterUpdate(int min_interval,
                                 int max_interval,
                                 int latency,
                                 int timeout,
                                 StatusCallback cb) override;
  bool IsConnected() override;
  int GetMtu() override;
  void GetServices(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)> cb)
      override;
  std::vector<scoped_refptr<RemoteService>> GetServicesSync() override;
  void GetServiceByUuid(
      const bluetooth_v2_shlib::Uuid& uuid,
      base::OnceCallback<void(scoped_refptr<RemoteService>)> cb) override;
  scoped_refptr<RemoteService> GetServiceByUuidSync(
      const bluetooth_v2_shlib::Uuid& uuid) override;
  const bluetooth_v2_shlib::Addr& addr() const override;

  void ReadCharacteristic(
      scoped_refptr<RemoteCharacteristicImpl> characteristic,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
      RemoteCharacteristic::ReadCallback cb);
  void WriteCharacteristic(
      scoped_refptr<RemoteCharacteristicImpl> characteristic,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
      bluetooth_v2_shlib::Gatt::WriteType write_type,
      std::vector<uint8_t> value,
      RemoteCharacteristic::StatusCallback cb);
  void ReadDescriptor(scoped_refptr<RemoteDescriptorImpl> descriptor,
                      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                      RemoteDescriptor::ReadCallback cb);
  void WriteDescriptor(scoped_refptr<RemoteDescriptorImpl> descriptor,
                       bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                       std::vector<uint8_t> value,
                       RemoteDescriptor::StatusCallback cb);

 private:
  friend class GattClientManagerImpl;

  RemoteDeviceImpl(const bluetooth_v2_shlib::Addr& addr,
                   base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteDeviceImpl() override;

  // Friend methods for GattClientManagerImpl
  void SetConnected(bool connected);
  void SetServicesDiscovered(bool discovered);
  bool GetServicesDiscovered();
  void SetMtu(int mtu);

  scoped_refptr<RemoteCharacteristic> CharacteristicFromHandle(uint16_t handle);

  void OnCharacteristicRead(bool status,
                            uint16_t handle,
                            const std::vector<uint8_t>& value);
  void OnCharacteristicWrite(bool status, uint16_t handle);
  void OnDescriptorRead(bool status,
                        uint16_t handle,
                        const std::vector<uint8_t>& value);
  void OnDescriptorWrite(bool status, uint16_t handle);
  void OnGetServices(
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services);
  void OnServicesRemoved(uint16_t start_handle, uint16_t end_handle);
  void OnServicesAdded(
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services);
  void OnReadRemoteRssiComplete(bool status, int rssi);
  // end Friend methods for GattClientManagerImpl

  void ConnectComplete(bool success);

  // Add an operation to the queue. Certain operations can only be executed
  // serially.
  void EnqueueOperation(base::OnceClosure op);

  // Notify that the currently queued operation has completed.
  void NotifyQueueOperationComplete();

  void RequestMtuImpl(int mtu);
  void ReadCharacteristicImpl(
      scoped_refptr<RemoteCharacteristicImpl> descriptor,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req);
  void WriteCharacteristicImpl(
      scoped_refptr<RemoteCharacteristicImpl> descriptor,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
      bluetooth_v2_shlib::Gatt::WriteType write_type,
      std::vector<uint8_t> value);
  void ReadDescriptorImpl(scoped_refptr<RemoteDescriptorImpl> descriptor,
                          bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req);
  void WriteDescriptorImpl(scoped_refptr<RemoteDescriptorImpl> descriptor,
                           bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                           std::vector<uint8_t> value);
  void ClearServices();

  const base::WeakPtr<GattClientManagerImpl> gatt_client_manager_;
  const bluetooth_v2_shlib::Addr addr_;

  // All bluetooth_v2_shlib calls are run on this task_runner. Below members
  // should only be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  bool services_discovered_ = false;

  bool connect_pending_ = false;
  StatusCallback connect_cb_;

  bool disconnect_pending_ = false;
  StatusCallback disconnect_cb_;

  bool rssi_pending_ = false;
  RssiCallback rssi_cb_;

  std::atomic<bool> connected_{false};
  std::atomic<int> mtu_{kDefaultMtu};
  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteService>>
      uuid_to_service_;
  std::map<uint16_t, scoped_refptr<RemoteCharacteristicImpl>>
      handle_to_characteristic_;

  std::deque<base::OnceClosure> command_queue_;
  std::queue<StatusCallback> mtu_callbacks_;
  std::map<uint16_t, std::queue<RemoteCharacteristic::ReadCallback>>
      handle_to_characteristic_read_cbs_;
  std::map<uint16_t, std::queue<RemoteCharacteristic::StatusCallback>>
      handle_to_characteristic_write_cbs_;
  std::map<uint16_t, std::queue<RemoteDescriptor::ReadCallback>>
      handle_to_descriptor_read_cbs_;
  std::map<uint16_t, std::queue<RemoteDescriptor::StatusCallback>>
      handle_to_descriptor_write_cbs_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceImpl);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_IMPL_H_
