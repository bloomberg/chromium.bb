// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/media_security/multi_profile_media_tray_item.h"

#include "ash/common/ash_view_ids.h"
#include "ash/common/media_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/chromeos/media_security/media_capture_observer.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace tray {

class MultiProfileMediaTrayView : public TrayItemView,
                                  public MediaCaptureObserver {
 public:
  explicit MultiProfileMediaTrayView(SystemTrayItem* system_tray_item)
      : TrayItemView(system_tray_item) {
    CreateImageView();
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_view()->SetImage(
        UseMd()
            ? gfx::CreateVectorIcon(kSystemTrayRecordingIcon, kTrayIconColor)
            : *bundle.GetImageSkiaNamed(IDR_AURA_UBER_TRAY_RECORDING));
    OnMediaCaptureChanged();
    WmShell::Get()->system_tray_notifier()->AddMediaCaptureObserver(this);
    set_id(VIEW_ID_MEDIA_TRAY_VIEW);
  }

  ~MultiProfileMediaTrayView() override {
    WmShell::Get()->system_tray_notifier()->RemoveMediaCaptureObserver(this);
  }

  // MediaCaptureObserver:
  void OnMediaCaptureChanged() override {
    MediaDelegate* media_delegate = WmShell::Get()->media_delegate();
    SessionStateDelegate* session_state_delegate =
        WmShell::Get()->GetSessionStateDelegate();
    // The user at 0 is the current desktop user.
    for (UserIndex index = 1;
         index < session_state_delegate->NumberOfLoggedInUsers(); ++index) {
      if (media_delegate->GetMediaCaptureState(index) != MEDIA_CAPTURE_NONE) {
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
    : SystemTrayItem(system_tray, UMA_MULTI_PROFILE_MEDIA) {}

MultiProfileMediaTrayItem::~MultiProfileMediaTrayItem() {}

views::View* MultiProfileMediaTrayItem::CreateTrayView(LoginStatus status) {
  return new tray::MultiProfileMediaTrayView(this);
}

}  // namespace ash
