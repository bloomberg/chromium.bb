// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_STATUS_ICON_CONTAINER_VIEW_H_
#define ATHENA_SYSTEM_STATUS_ICON_CONTAINER_VIEW_H_

#include "athena/system/public/system_ui.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/view.h"

namespace athena {

// View which displays the system tray icons.
class StatusIconContainerView : public views::View {
 public:
  StatusIconContainerView(SystemUI::ColorScheme color_scheme);
  ~StatusIconContainerView() override;

 private:
  // views::View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) override;
  virtual void OnGestureEvent(ui::GestureEvent* event) override;
  virtual void ChildPreferredSizeChanged(views::View* child) override;

  class PowerStatus;
  scoped_ptr<PowerStatus> power_status_;

  class NetworkStatus;
  scoped_ptr<NetworkStatus> network_status_;

  class UpdateStatus;
  scoped_ptr<UpdateStatus> update_status_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconContainerView);
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_STATUS_ICON_CONTAINER_VIEW_H_
