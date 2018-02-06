// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/docked_magnifier/docked_magnifier_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

DockedMagnifierClient::DockedMagnifierClient() : binding_(this) {}

DockedMagnifierClient::~DockedMagnifierClient() {}

void DockedMagnifierClient::Start() {
  if (!docked_magnifier_controller_) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(ash::mojom::kServiceName,
                             &docked_magnifier_controller_);
  }
  ash::mojom::DockedMagnifierClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  docked_magnifier_controller_->SetClient(std::move(client));
}

void DockedMagnifierClient::OnEnabledStatusChanged(bool enabled) {
  if (enabled == observing_focus_changes_)
    return;

  observing_focus_changes_ = enabled;
  if (enabled) {
    registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                   content::NotificationService::AllSources());
  } else {
    registrar_.Remove(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                      content::NotificationService::AllSources());
  }
}

void DockedMagnifierClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE);

  content::FocusedNodeDetails* node_details =
      content::Details<content::FocusedNodeDetails>(details).ptr();
  // Ash uses the InputMethod of the window tree host to observe text input
  // caret bounds changes, which works for both the native UI as well as
  // webpages. We don't need to notify it of editable nodes in this case.
  if (node_details->is_editable_node)
    return;

  const gfx::Rect& bounds_in_screen = node_details->node_bounds_in_screen;
  if (bounds_in_screen.IsEmpty())
    return;

  docked_magnifier_controller_->CenterOnPoint(bounds_in_screen.CenterPoint());
}
