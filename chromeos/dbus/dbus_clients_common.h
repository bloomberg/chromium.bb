// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENTS_COMMON_H_
#define CHROMEOS_DBUS_DBUS_CLIENTS_COMMON_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"

namespace dbus {
class Bus;
}

namespace chromeos {

class CrasAudioClient;

// D-Bus clients used in multiple processes (e.g. ash, browser, mus).
class COMPONENT_EXPORT(CHROMEOS_DBUS) DBusClientsCommon {
 public:
  // Creates real implementations if |use_real_clients| is true and fakes
  // otherwise. Fakes are used when running on Linux desktop and in tests.
  explicit DBusClientsCommon(bool use_real_clients);
  ~DBusClientsCommon();

  // Initialize proper runtime environment for its dbus clients.
  void Initialize(dbus::Bus* system_bus);

 private:
  friend class DBusThreadManager;
  friend class DBusThreadManagerSetter;

  std::unique_ptr<CrasAudioClient> cras_audio_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusClientsCommon);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENTS_COMMON_H_
