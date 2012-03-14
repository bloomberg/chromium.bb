// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

namespace tray {

enum ResourceSize {
  SMALL,
  LARGE,
};

class NetworkTrayView : public views::View {
 public:
  explicit NetworkTrayView(ResourceSize size) : resource_size_(size) {
    SetLayoutManager(new views::FillLayout());

    image_view_ = new views::ImageView;
    AddChildView(image_view_);

    Update(Shell::GetInstance()->tray_delegate()->GetMostRelevantNetworkIcon());
  }

  virtual ~NetworkTrayView() {}

  void Update(const NetworkIconInfo& info) {
    image_view_->SetImage(info.image);
    SchedulePaint();
  }

 private:
  views::ImageView* image_view_;
  ResourceSize resource_size_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public views::View {
 public:
  NetworkDefaultView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        0, 0, 5));

    icon_ = new NetworkTrayView(LARGE);
    AddChildView(icon_);

    label_ = new views::Label();
    AddChildView(label_);

    Update(Shell::GetInstance()->tray_delegate()->GetMostRelevantNetworkIcon());
  }

  virtual ~NetworkDefaultView() {}

  void Update(const NetworkIconInfo& info) {
    icon_->Update(info);
    label_->SetText(info.description);
  }

 private:
  NetworkTrayView* icon_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

}  // namespace tray

TrayNetwork::TrayNetwork() {
}

TrayNetwork::~TrayNetwork() {
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  tray_.reset(new tray::NetworkTrayView(tray::SMALL));
  return tray_.get();
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  default_.reset(new tray::NetworkDefaultView);
  return default_.get();
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayNetwork::DestroyTrayView() {
  tray_.reset();
}

void TrayNetwork::DestroyDefaultView() {
  default_.reset();
}

void TrayNetwork::DestroyDetailedView() {
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_.get())
    tray_->Update(info);
  if (default_.get())
    default_->Update(info);
}

}  // namespace internal
}  // namespace ash
