// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include "ash/shell/panel_window.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/wm/shadow_types.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

const int kTrayIconHeight = 50;
const int kPadding = 5;

class SystemTrayBubble : public views::BubbleDelegateView {
 public:
  SystemTrayBubble(ash::SystemTray* tray,
                   std::vector<ash::SystemTrayItem*>& items,
                   bool detailed)
      : views::BubbleDelegateView(tray, views::BubbleBorder::BOTTOM_RIGHT),
        tray_(tray),
        items_(items),
        detailed_(detailed) {
    set_margin(1);
  }

  virtual ~SystemTrayBubble() {
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      if (detailed_)
        (*it)->DestroyDetailedView();
      else
        (*it)->DestroyDefaultView();
    }
  }

 private:
  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          0, 0, 1));

    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      views::View* view = detailed_ ? (*it)->CreateDetailedView() :
                                      (*it)->CreateDefaultView();
      if (it != items_.begin())
        view->set_border(views::Border::CreateSolidSidedBorder(
              1, 0, 0, 0, SkColorSetARGB(25, 0, 0, 0)));
      AddChildView(view);
    }
  }

  ash::SystemTray* tray_;
  std::vector<ash::SystemTrayItem*> items_;
  bool detailed_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace

namespace ash {

SystemTray::SystemTray()
    : items_(),
      popup_(NULL) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        5, 10, 3));
}

SystemTray::~SystemTray() {
  if (popup_)
    popup_->CloseNow();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  views::View* tray_item = item->CreateTrayView();
  if (tray_item) {
    AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDetailedView(SystemTrayItem* item) {
  if (popup_)
    popup_->Close();
  popup_ = NULL;

  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true);
}

void SystemTray::ShowItems(std::vector<SystemTrayItem*>& items, bool detailed) {
  CHECK(!popup_);
  SystemTrayBubble* bubble = new SystemTrayBubble(this, items, detailed);
  popup_ = views::BubbleDelegateView::CreateBubble(bubble);
  popup_->AddObserver(this);
  bubble->Show();
}

bool SystemTray::OnMousePressed(const views::MouseEvent& event) {
  if (popup_)
    popup_->Show();
  else
    ShowItems(items_, false);
  return true;
}

void SystemTray::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(popup_, widget);
  popup_ = NULL;
}

}  // namespace ash
