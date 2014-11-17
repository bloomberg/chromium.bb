// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

class CHROMEOS_EXPORT BluetoothMediaTransportClient : public DBusClient {
 public:
  struct Properties : public dbus::PropertySet {
    // The path to the device object which the transport is connected to.
    // Read-only.
    dbus::Property<dbus::ObjectPath> device;

    // UUID of the profile which the transport is for. Read-only.
    dbus::Property<std::string> uuid;

    // Assigned codec value supported by the media transport. Read-only.
    dbus::Property<uint8_t> codec;

    // The configuration used by the media transport. Read-only.
    dbus::Property<std::vector<uint8_t>> configuration;

    // The state of the transport. Read-only.
    // The values can be one of the following:
    // "idle": not streaming
    // "pending": streaming but not acquired
    // "active": streaming and acquired
    dbus::Property<std::string> state;

    // The unit of transport delay is in 1/10 of millisecond. This property is
    // only writeable when the transport was aquired by the sender. Optional.
    dbus::Property<uint16_t> delay;

    // The volume level of the transport. This property is only writable when
    // the transport was aquired by the sender. Optional.
    dbus::Property<uint16_t> volume;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  ~BluetoothMediaTransportClient() override;

  // The ErrorCallback is used by media transport API methods to indicate
  // failure. It receives two arguments: the name of the error in |error_name|
  // and an optional message in |error_message|.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)> ErrorCallback;

  // The AcquireCallback is used by |Acquire| method of media tansport API tp
  // indicate the success of the method.
  typedef base::Callback<void(const dbus::FileDescriptor& fd,
                              const uint16_t read_mtu,
                              const uint16_t write_mtu)> AcquireCallback;

  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Acquires transport file descriptor and the MTU for read and write.
  virtual void Acquire(const dbus::ObjectPath& object_path,
                       const AcquireCallback& callback,
                       const ErrorCallback& error_callback) = 0;

  // Acquires transport file descriptor only if the transport is in "pending"
  // state at the time the message is received by BlueZ. Otherwise no request
  // will be sent to the remote device and the function will just fail with
  // org.bluez.Error.NotAvailable.
  virtual void TryAcquire(const dbus::ObjectPath& object_path,
                          const AcquireCallback& callback,
                          const ErrorCallback& error_callback) = 0;

  // Releases the file descriptor of the transport.
  virtual void Release(const dbus::ObjectPath& object_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) = 0;

  static BluetoothMediaTransportClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kUnexpectedResponse[];

 protected:
  BluetoothMediaTransportClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothMediaTransportClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
