// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
#define DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom.h"
#include "mojo/public/cpp/system/buffer.h"

namespace device {

class PlatformSensorProvider;
class PlatformSensorConfiguration;

// Base class for the sensors provided by the platform. Concrete instances of
// this class are created by platform specific PlatformSensorProvider.
class PlatformSensor : public base::RefCountedThreadSafe<PlatformSensor> {
 public:
  // The interface that must be implemented by PlatformSensor clients.
  class Client {
   public:
    virtual void OnSensorReadingChanged() = 0;
    virtual void OnSensorError() = 0;
    virtual bool IsNotificationSuspended() = 0;

   protected:
    virtual ~Client() {}
  };

  virtual mojom::ReportingMode GetReportingMode() = 0;
  virtual PlatformSensorConfiguration GetDefaultConfiguration() = 0;

  mojom::SensorType GetType() const;

  bool StartListening(Client* client,
                      const PlatformSensorConfiguration& config);
  bool StopListening(Client* client, const PlatformSensorConfiguration& config);

  void UpdateSensor();

  void AddClient(Client*);
  void RemoveClient(Client*);

 protected:
  virtual ~PlatformSensor();
  PlatformSensor(mojom::SensorType type,
                 mojo::ScopedSharedBufferMapping mapping,
                 PlatformSensorProvider* provider);

  using ConfigMap = std::map<Client*, std::list<PlatformSensorConfiguration>>;

  virtual bool UpdateSensorInternal(const ConfigMap& configurations);
  virtual bool StartSensor(
      const PlatformSensorConfiguration& configuration) = 0;
  virtual void StopSensor() = 0;
  virtual bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) = 0;

  void NotifySensorReadingChanged();
  void NotifySensorError();

  mojo::ScopedSharedBufferMapping shared_buffer_mapping_;

  // For testing purposes.
  const ConfigMap& config_map() const { return config_map_; }

 private:
  friend class base::RefCountedThreadSafe<PlatformSensor>;

  mojom::SensorType type_;
  base::ObserverList<Client, true> clients_;
  ConfigMap config_map_;
  PlatformSensorProvider* provider_;
  base::WeakPtrFactory<PlatformSensor> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PlatformSensor);
};

}  // namespace device

#endif  // DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
