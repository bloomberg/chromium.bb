// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/managed_device_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

ManagedDeviceView::ManagedDeviceView() : TrayItemView(nullptr) {
  Shell::Get()->system_tray_model()->enterprise_domain()->AddObserver(this);
  CreateImageView();
  image_view()->SetImage(gfx::CreateVectorIcon(
      kSystemTrayManagedIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
  Update();
}

ManagedDeviceView::~ManagedDeviceView() {
  Shell::Get()->system_tray_model()->enterprise_domain()->RemoveObserver(this);
}

void ManagedDeviceView::OnEnterpriseDomainChanged() {
  Update();
}

void ManagedDeviceView::Update() {
  // TODO(crbug.com/870409): Also support family link.
  EnterpriseDomainModel* model =
      Shell::Get()->system_tray_model()->enterprise_domain();
  SetVisible(model->active_directory_managed() ||
             !model->enterprise_display_domain().empty());
}

}  // namespace ash
