// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

StatusIconLinuxWrapper::StatusIconLinuxWrapper(StatusIconLinux* status_icon) {
  status_icon_.reset(status_icon);
  status_icon_->set_delegate(this);
}

StatusIconLinuxWrapper::~StatusIconLinuxWrapper() {}

void StatusIconLinuxWrapper::SetImage(const gfx::ImageSkia& image) {
  status_icon_->SetImage(image);
}

void StatusIconLinuxWrapper::SetPressedImage(const gfx::ImageSkia& image) {
  status_icon_->SetPressedImage(image);
}

void StatusIconLinuxWrapper::SetToolTip(const string16& tool_tip) {
  status_icon_->SetToolTip(tool_tip);
}

void StatusIconLinuxWrapper::DisplayBalloon(const gfx::ImageSkia& icon,
                                            const string16& title,
                                            const string16& contents) {
  notification_.DisplayBalloon(icon, title, contents);
}

void StatusIconLinuxWrapper::OnClick() {
  DispatchClickEvent();
}

bool StatusIconLinuxWrapper::HasClickAction() {
  return HasObservers();
}

StatusIconLinuxWrapper* StatusIconLinuxWrapper::CreateWrappedStatusIcon(
    const gfx::ImageSkia& image,
    const string16& tool_tip) {
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  if (linux_ui) {
    scoped_ptr<StatusIconLinux> status_icon =
        linux_ui->CreateLinuxStatusIcon(image, tool_tip);
    if (status_icon.get())
      return new StatusIconLinuxWrapper(status_icon.release());
  }
  return NULL;
}

void StatusIconLinuxWrapper::UpdatePlatformContextMenu(ui::MenuModel* model) {
  status_icon_->UpdatePlatformContextMenu(model);
}
