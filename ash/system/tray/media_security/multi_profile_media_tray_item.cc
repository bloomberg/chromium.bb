// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/media_security/multi_profile_media_tray_item.h"

#include "ash/ash_view_ids.h"
#include "ash/media_delegate.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/system/tray/media_security/media_capture_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_item_view.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace tray {

class MultiProfileMediaTrayView : public TrayItemView,
                                  public MediaCaptureObserver {
 public:
  explicit MultiProfileMediaTrayView(SystemTrayItem* system_tray_item)
      : TrayItemView(system_tray_item) {
    SetLayoutManager(new views::FillLayout);
    views::ImageView* icon = new views::ImageView;
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    icon->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_RECORDING).ToImageSkia());
    AddChildView(icon);
    OnMediaCaptureChanged();
    Shell::GetInstance()->system_tray_notifier()->AddMediaCaptureObserver(this);
    set_id(VIEW_ID_MEDIA_TRAY_VIEW);
  }

  virtual ~MultiProfileMediaTrayView() {
    Shell::GetInstance()->system_tray_notifier()->RemoveMediaCaptureObserver(
        this);
  }

  // MediaCaptureObserver:
  virtual void OnMediaCaptureChanged() OVERRIDE {
    MediaDelegate* media_delegate = Shell::GetInstance()->media_delegate();
    SessionStateDelegate* session_state_delegate =
        Shell::GetInstance()->session_state_delegate();
    // The user at 0 is the current desktop user.
    for (MultiProfileIndex index = 1;
         index < session_state_delegate->NumberOfLoggedInUsers();
         ++index) {
      content::BrowserContext* context =
          session_state_delegate->GetBrowserContextByIndex(index);
      if (media_delegate->GetMediaCaptureState(context) != MEDIA_CAPTURE_NONE) {
        SetVisible(true);
        return;
      }
    }
    SetVisible(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiProfileMediaTrayView);
};

}  // namespace tray

MultiProfileMediaTrayItem::MultiProfileMediaTrayItem(SystemTray* system_tray)
    : SystemTrayItem(system_tray), tray_view_(NULL) {
}

MultiProfileMediaTrayItem::~MultiProfileMediaTrayItem() {
}

views::View* MultiProfileMediaTrayItem::CreateTrayView(
    user::LoginStatus status) {
  tray_view_ = new tray::MultiProfileMediaTrayView(this);
  return tray_view_;
}

void MultiProfileMediaTrayItem::DestroyTrayView() {
  tray_view_ = NULL;
}

}  // namespace ash
