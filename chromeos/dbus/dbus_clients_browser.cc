// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_clients_browser.h"

#include "base/logging.h"
#include "chromeos/dbus/arc_obb_mounter_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/fake_arc_obb_mounter_client.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"

namespace chromeos {
namespace {

// Avoid ugly casts below.
bool IsUsingReal(DBusClientTypeMask real_clients, DBusClientType type) {
  return real_clients & static_cast<DBusClientTypeMask>(type);
}

}  // namespace

DBusClientsBrowser::DBusClientsBrowser(DBusClientTypeMask real_clients) {
  if (IsUsingReal(real_clients, DBusClientType::ARC_OBB_MOUNTER))
    arc_obb_mounter_client_.reset(ArcObbMounterClient::Create());
  else
    arc_obb_mounter_client_.reset(new FakeArcObbMounterClient);

  cros_disks_client_.reset(CrosDisksClient::Create(
      IsUsingReal(real_clients, DBusClientType::CROS_DISKS)
          ? REAL_DBUS_CLIENT_IMPLEMENTATION
          : FAKE_DBUS_CLIENT_IMPLEMENTATION));

  if (IsUsingReal(real_clients, DBusClientType::DEBUG_DAEMON))
    debug_daemon_client_.reset(DebugDaemonClient::Create());
  else
    debug_daemon_client_.reset(new FakeDebugDaemonClient);

  if (IsUsingReal(real_clients, DBusClientType::EASY_UNLOCK))
    easy_unlock_client_.reset(EasyUnlockClient::Create());
  else
    easy_unlock_client_.reset(new FakeEasyUnlockClient);

  if (IsUsingReal(real_clients, DBusClientType::IMAGE_BURNER))
    image_burner_client_.reset(ImageBurnerClient::Create());
  else
    image_burner_client_.reset(new FakeImageBurnerClient);

  if (IsUsingReal(real_clients, DBusClientType::LORGNETTE_MANAGER))
    lorgnette_manager_client_.reset(LorgnetteManagerClient::Create());
  else
    lorgnette_manager_client_.reset(new FakeLorgnetteManagerClient);
}

DBusClientsBrowser::~DBusClientsBrowser() {}

void DBusClientsBrowser::Initialize(dbus::Bus* system_bus) {
  DCHECK(DBusThreadManager::IsInitialized());

  arc_obb_mounter_client_->Init(system_bus);
  cros_disks_client_->Init(system_bus);
  debug_daemon_client_->Init(system_bus);
  easy_unlock_client_->Init(system_bus);
  image_burner_client_->Init(system_bus);
  lorgnette_manager_client_->Init(system_bus);
}

}  // namespace chromeos
