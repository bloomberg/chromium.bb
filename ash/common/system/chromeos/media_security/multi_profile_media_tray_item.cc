// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/media_security/multi_profile_media_tray_item.h"

#include "ash/common/ash_view_ids.h"
#include "ash/common/media_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
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
    WmShell::Get()->media_controller()->AddObserver(this);
    SetVisible(false);
    WmShell::Get()->media_controller()->RequestCaptureState();
    set_id(VIEW_ID_MEDIA_TRAY_VIEW);
  }

  ~MultiProfileMediaTrayView() override {
    WmShell::Get()->media_controller()->RemoveObserver(this);
  }

  // MediaCaptureObserver:
  void OnMediaCaptureChanged(
      const std::vector<mojom::MediaCaptureState>& capture_states) override {
    SessionStateDelegate* session_state_delegate =
        WmShell::Get()->GetSessionStateDelegate();
    // The user at 0 is the current desktop user.
    for (UserIndex index = 1;
         index < session_state_delegate->NumberOfLoggedInUsers(); ++index) {
      if (capture_states[index] != mojom::MediaCaptureState::NONE) {
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
