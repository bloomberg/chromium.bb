// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_clients_common.h"

#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"

namespace chromeos {

DBusClientsCommon::DBusClientsCommon(bool use_real_clients) {
  if (use_real_clients)
    cras_audio_client_.reset(CrasAudioClient::Create());
  else
    cras_audio_client_.reset(new FakeCrasAudioClient);
}

DBusClientsCommon::~DBusClientsCommon() = default;

void DBusClientsCommon::Initialize(dbus::Bus* system_bus) {
  DCHECK(DBusThreadManager::IsInitialized());

  cras_audio_client_->Init(system_bus);
}

}  // namespace chromeos
