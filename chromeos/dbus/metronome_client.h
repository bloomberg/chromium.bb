// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_METRONOME_CLIENT_H_
#define CHROMEOS_DBUS_METRONOME_CLIENT_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

namespace chromeos {

// A DBus client class for the org.chromium.Metronome service.
class CHROMEOS_EXPORT MetronomeClient : public DBusClient {
 public:
  // Interface for observing timestamp events from a metronome client.
  class Observer {
   public:
    // Called when the TimestampUpdated signal is received.
    virtual void OnTimestampUpdated(uint64 beacon_timestamp,
                                    uint64 local_timestamp) = 0;
    virtual ~Observer() {}
  };

  ~MetronomeClient() override;

  // Adds and removes observers for timestamp events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static MetronomeClient* Create(DBusClientImplementationType type);

 protected:
  // Create() should be used instead.
  MetronomeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(MetronomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_METRONOME_CLIENT_H_
