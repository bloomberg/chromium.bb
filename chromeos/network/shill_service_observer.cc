// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_service_observer.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/object_path.h"

namespace chromeos {
namespace internal {

ShillServiceObserver::ShillServiceObserver(const std::string& service_path,
                                           const Handler& handler)
    : service_path_(service_path),
      handler_(handler) {
  DBusThreadManager::Get()->GetShillServiceClient()->
      AddPropertyChangedObserver(dbus::ObjectPath(service_path), this);
}

ShillServiceObserver::~ShillServiceObserver() {
  DBusThreadManager::Get()->GetShillServiceClient()->
      RemovePropertyChangedObserver(dbus::ObjectPath(service_path_), this);
}

void ShillServiceObserver::OnPropertyChanged(const std::string& key,
                                             const base::Value& value) {
  handler_.Run(service_path_, key, value);
}

}  // namespace internal
}  // namespace chromeos
