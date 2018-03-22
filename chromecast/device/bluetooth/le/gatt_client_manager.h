// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "chromecast/device/bluetooth/shlib/gatt_client.h"

namespace chromecast {
namespace bluetooth {

class RemoteCharacteristic;
class RemoteDevice;
class RemoteService;

class GattClientManager : public bluetooth_v2_shlib::Gatt::Client::Delegate {
 public:
  class Observer {
   public:
    // Called when the connection state changes for |device|.
    virtual void OnConnectChanged(scoped_refptr<RemoteDevice> device,
                                  bool connected) {}

    // Called when the connection MTU changes for |device|.
    virtual void OnMtuChanged(scoped_refptr<RemoteDevice> device, int mtu) {}

    // Called when the device |device|'s service list changed. |services| is the
    // new list of services, and any old ones should be considered invalidated.
    virtual void OnServicesUpdated(
        scoped_refptr<RemoteDevice> device,
        std::vector<scoped_refptr<RemoteService>> services) {}

    // Called when |device| has a notification on |characteristic| with |value|.
    virtual void OnCharacteristicNotification(
        scoped_refptr<RemoteDevice> device,
        scoped_refptr<RemoteCharacteristic> characteristic,
        std::vector<uint8_t> value) {}

    // Called when a connection is requested.
    virtual void OnConnectInitated(const bluetooth_v2_shlib::Addr& addr) {}

    virtual ~Observer() = default;
  };

  explicit GattClientManager(bluetooth_v2_shlib::GattClient* gatt_client);
  ~GattClientManager() override;

  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  void Finalize();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // TODO(bcf/slan): Add new method:
  // void GetDevices(Callback<void(vector<scoped_refptr<RemoteDevice>>)> cb);

  // Get a RemoteDevice object corresponding to |addr| for performing GATT
  // operations. |cb| will be run on the callers thread. Callbacks passed into
  // methods on RemoteDevice and its subobjects (RemoteService,
  // RemoteCharacteristic, RemoteDescriptor) will also be run on the thread
  // which called the specific method.
  void GetDevice(const bluetooth_v2_shlib::Addr& addr,
                 base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb);

  // TODO(bcf): Deprecated. Replace usage with async version.
  scoped_refptr<RemoteDevice> GetDeviceSync(
      const bluetooth_v2_shlib::Addr& addr);

  // TODO(bcf): Make async.
  // Returns the number of devices which are currently connected.
  size_t GetNumConnected() const;

  void NotifyConnect(const bluetooth_v2_shlib::Addr& addr);

  // TODO(bcf): Deprecated. Should be removed now that this class may be used
  // from any thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return io_task_runner_;
  }

  // TODO(bcf): Should be private and passed into objects which need it (e.g.
  // RemoteDevice, RemoteCharacteristic).
  bluetooth_v2_shlib::GattClient* gatt_client() const { return gatt_client_; }

 private:
  // bluetooth_v2_shlib::Gatt::Client::Delegate implementation:
  void OnConnectChanged(const bluetooth_v2_shlib::Addr& addr,
                        bool status,
                        bool connected) override;
  void OnNotification(const bluetooth_v2_shlib::Addr& addr,
                      uint16_t handle,
                      const std::vector<uint8_t>& value) override;
  void OnCharacteristicReadResponse(const bluetooth_v2_shlib::Addr& addr,
                                    bool status,
                                    uint16_t handle,
                                    const std::vector<uint8_t>& value) override;
  void OnCharacteristicWriteResponse(const bluetooth_v2_shlib::Addr& addr,
                                     bool status,
                                     uint16_t handle) override;
  void OnDescriptorReadResponse(const bluetooth_v2_shlib::Addr& addr,
                                bool status,
                                uint16_t handle,
                                const std::vector<uint8_t>& value) override;
  void OnDescriptorWriteResponse(const bluetooth_v2_shlib::Addr& addr,
                                 bool status,
                                 uint16_t handle) override;
  void OnReadRemoteRssi(const bluetooth_v2_shlib::Addr& addr,
                        bool status,
                        int rssi) override;
  void OnMtuChanged(const bluetooth_v2_shlib::Addr& addr,
                    bool status,
                    int mtu) override;
  void OnGetServices(
      const bluetooth_v2_shlib::Addr& addr,
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) override;
  void OnServicesRemoved(const bluetooth_v2_shlib::Addr& addr,
                         uint16_t start_handle,
                         uint16_t end_handle) override;
  void OnServicesAdded(
      const bluetooth_v2_shlib::Addr& addr,
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) override;

  static void FinalizeOnIoThread(
      std::unique_ptr<base::WeakPtrFactory<GattClientManager>> weak_factory);

  bluetooth_v2_shlib::GattClient* const gatt_client_;

  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // All bluetooth_v2_shlib calls are run on this task_runner. Following members
  // must only be accessed on this task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // TODO(bcf): Need to delete on disconnect.
  std::map<bluetooth_v2_shlib::Addr, scoped_refptr<RemoteDevice>>
      addr_to_device_;
  std::set<bluetooth_v2_shlib::Addr> connected_devices_;

  base::WeakPtr<GattClientManager> weak_this_;
  std::unique_ptr<base::WeakPtrFactory<GattClientManager>> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(GattClientManager);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_H_
