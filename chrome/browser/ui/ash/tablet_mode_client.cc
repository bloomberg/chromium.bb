// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/tablet_mode_client.h"

#include <utility>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/command_line.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

TabletModeClient* g_instance = nullptr;

}  // namespace

TabletModeClient::TabletModeClient()
    : auto_hide_title_bars_(!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshDisableTabletAutohideTitlebars)),
      binding_(this) {
  DCHECK(!g_instance);
  g_instance = this;
}

TabletModeClient::~TabletModeClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void TabletModeClient::Init() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &tablet_mode_controller_);
  BindAndSetClient();
}

void TabletModeClient::InitForTesting(
    ash::mojom::TabletModeControllerPtr controller) {
  tablet_mode_controller_ = std::move(controller);
  BindAndSetClient();
}

// static
TabletModeClient* TabletModeClient::Get() {
  return g_instance;
}

void TabletModeClient::AddObserver(TabletModeClientObserver* observer) {
  observers_.AddObserver(observer);
  observer->OnTabletModeToggled(tablet_mode_enabled_);
}

void TabletModeClient::RemoveObserver(TabletModeClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabletModeClient::OnTabletModeToggled(bool enabled) {
  tablet_mode_enabled_ = enabled;
  for (auto& observer : observers_)
    observer.OnTabletModeToggled(enabled);
}

void TabletModeClient::FlushForTesting() {
  tablet_mode_controller_.FlushForTesting();
}

void TabletModeClient::BindAndSetClient() {
  ash::mojom::TabletModeClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  tablet_mode_controller_->SetClient(std::move(client));
}
