// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/tray_display.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

using message_center::Notification;

namespace ash {
namespace {

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

base::string16 GetDisplayName(int64 display_id) {
  return base::UTF8ToUTF16(
      GetDisplayManager()->GetDisplayNameForId(display_id));
}

base::string16 GetDisplaySize(int64 display_id) {
  DisplayManager* display_manager = GetDisplayManager();

  const gfx::Display* display = &display_manager->GetDisplayForId(display_id);

  // We don't show display size for mirrored display. Fallback
  // to empty string if this happens on release build.
  bool mirrored_display = display_manager->mirrored_display_id() == display_id;
  DCHECK(!mirrored_display);
  if (mirrored_display)
    return base::string16();

  DCHECK(display->is_valid());
  return base::UTF8ToUTF16(display->size().ToString());
}

// Returns 1-line information for the specified display, like
// "InternalDisplay: 1280x750"
base::string16 GetDisplayInfoLine(int64 display_id) {
  const DisplayInfo& display_info =
      GetDisplayManager()->GetDisplayInfo(display_id);
  if (GetDisplayManager()->mirrored_display_id() == display_id)
    return GetDisplayName(display_id);

  base::string16 size_text = GetDisplaySize(display_id);
  base::string16 display_data;
  if (display_info.has_overscan()) {
    display_data = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION,
        size_text,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION_OVERSCAN));
  } else {
    display_data = size_text;
  }

  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DISPLAY_SINGLE_DISPLAY,
      GetDisplayName(display_id),
      display_data);
}

base::string16 GetAllDisplayInfo() {
  DisplayManager* display_manager = GetDisplayManager();
  std::vector<base::string16> lines;
  int64 internal_id = gfx::Display::kInvalidDisplayID;
  // Make sure to show the internal display first.
  if (display_manager->HasInternalDisplay() &&
      display_manager->IsInternalDisplayId(
          display_manager->first_display_id())) {
    internal_id = display_manager->first_display_id();
    lines.push_back(GetDisplayInfoLine(internal_id));
  }

  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64 id = display_manager->GetDisplayAt(i).id();
    if (id == internal_id)
      continue;
    lines.push_back(GetDisplayInfoLine(id));
  }

  return JoinString(lines, '\n');
}

void OpenSettings() {
  // switch is intentionally introduced without default, to cause an error when
  // a new type of login status is introduced.
  switch (Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus()) {
    case user::LOGGED_IN_NONE:
    case user::LOGGED_IN_LOCKED:
      return;

    case user::LOGGED_IN_USER:
    case user::LOGGED_IN_OWNER:
    case user::LOGGED_IN_GUEST:
    case user::LOGGED_IN_RETAIL_MODE:
    case user::LOGGED_IN_PUBLIC:
    case user::LOGGED_IN_SUPERVISED:
    case user::LOGGED_IN_KIOSK_APP:
      ash::SystemTrayDelegate* delegate =
          Shell::GetInstance()->system_tray_delegate();
      if (delegate->ShouldShowSettings())
        delegate->ShowDisplaySettings();
  }
}

}  // namespace

const char TrayDisplay::kNotificationId[] = "chrome://settings/display";

class DisplayView : public ActionableView {
 public:
  explicit DisplayView() {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal,
        kTrayPopupPaddingHorizontal, 0,
        kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    image_ = new FixedSizedImageView(0, kTrayPopupItemHeight);
    image_->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_DISPLAY).ToImageSkia());
    AddChildView(image_);

    label_ = new views::Label();
    label_->SetMultiLine(true);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(label_);
    Update();
  }

  virtual ~DisplayView() {}

  void Update() {
    base::string16 message = GetTrayDisplayMessage(NULL);
    if (message.empty() && ShouldShowFirstDisplayInfo())
      message = GetDisplayInfoLine(GetDisplayManager()->first_display_id());
    SetVisible(!message.empty());
    label_->SetText(message);
    SetAccessibleName(message);
    Layout();
  }

  const views::Label* label() const { return label_; }

  // Overridden from views::View.
  virtual bool GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const OVERRIDE {
    base::string16 tray_message = GetTrayDisplayMessage(NULL);
    base::string16 display_message = GetAllDisplayInfo();
    if (tray_message.empty() && display_message.empty())
      return false;

    *tooltip = tray_message + base::ASCIIToUTF16("\n") + display_message;
    return true;
  }

  // Returns the name of the currently connected external display.
  // This should not be used when the external display is used for
  // mirroring.
  static base::string16 GetExternalDisplayName() {
    DisplayManager* display_manager = GetDisplayManager();
    DCHECK(!display_manager->IsMirrored());

    int64 external_id = gfx::Display::kInvalidDisplayID;
    for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
      int64 id = display_manager->GetDisplayAt(i).id();
      if (id != gfx::Display::InternalDisplayId()) {
        external_id = id;
        break;
      }
    }

    if (external_id == gfx::Display::kInvalidDisplayID) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);
    }

    // The external display name may have an annotation of "(width x height)" in
    // case that the display is rotated or its resolution is changed.
    base::string16 name = GetDisplayName(external_id);
    const DisplayInfo& display_info =
        display_manager->GetDisplayInfo(external_id);
    if (display_info.rotation() != gfx::Display::ROTATE_0 ||
        display_info.configured_ui_scale() != 1.0f ||
        !display_info.overscan_insets_in_dip().empty()) {
      name = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATED_NAME,
          name, GetDisplaySize(external_id));
    } else if (display_info.overscan_insets_in_dip().empty() &&
               display_info.has_overscan()) {
      name = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATED_NAME,
          name, l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION_OVERSCAN));
    }

    return name;
  }

  static base::string16 GetTrayDisplayMessage(
      base::string16* additional_message_out) {
    DisplayManager* display_manager = GetDisplayManager();
    if (display_manager->GetNumDisplays() > 1) {
      if (GetDisplayManager()->HasInternalDisplay()) {
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED, GetExternalDisplayName());
      }
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL);
    }

    if (display_manager->IsMirrored()) {
      if (GetDisplayManager()->HasInternalDisplay()) {
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
            GetDisplayName(display_manager->mirrored_display_id()));
      }
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL);
    }

    int64 primary_id = Shell::GetScreen()->GetPrimaryDisplay().id();
    if (display_manager->HasInternalDisplay() &&
        !display_manager->IsInternalDisplayId(primary_id)) {
      if (additional_message_out) {
        *additional_message_out = l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED_DESCRIPTION);
      }
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED);
    }

    return base::string16();
  }

 private:
  bool ShouldShowFirstDisplayInfo() const {
    const DisplayInfo& display_info = GetDisplayManager()->GetDisplayInfo(
        GetDisplayManager()->first_display_id());
    return display_info.rotation() != gfx::Display::ROTATE_0 ||
        display_info.configured_ui_scale() != 1.0f ||
        !display_info.overscan_insets_in_dip().empty() ||
        display_info.has_overscan();
  }

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    OpenSettings();
    return true;
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE {
    int label_max_width = bounds().width() - kTrayPopupPaddingHorizontal * 2 -
        kTrayPopupPaddingBetweenItems - image_->GetPreferredSize().width();
    label_->SizeToFit(label_max_width);
  }

  views::ImageView* image_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DisplayView);
};

TrayDisplay::TrayDisplay(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      default_(NULL) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
  UpdateDisplayInfo(NULL);
}

TrayDisplay::~TrayDisplay() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

void TrayDisplay::UpdateDisplayInfo(TrayDisplay::DisplayInfoMap* old_info) {
  if (old_info)
    old_info->swap(display_info_);
  display_info_.clear();

  DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64 id = display_manager->GetDisplayAt(i).id();
    display_info_[id] = display_manager->GetDisplayInfo(id);
  }
}

bool TrayDisplay::GetDisplayMessageForNotification(
    const TrayDisplay::DisplayInfoMap& old_info,
    base::string16* message_out,
    base::string16* additional_message_out) {
  // Display is added or removed. Use the same message as the one in
  // the system tray.
  if (display_info_.size() != old_info.size()) {
    *message_out = DisplayView::GetTrayDisplayMessage(additional_message_out);
    return true;
  }

  for (DisplayInfoMap::const_iterator iter = display_info_.begin();
       iter != display_info_.end(); ++iter) {
    DisplayInfoMap::const_iterator old_iter = old_info.find(iter->first);
    // The display's number is same but different displays. This happens
    // for the transition between docked mode and mirrored display. Falls back
    // to GetTrayDisplayMessage().
    if (old_iter == old_info.end()) {
      *message_out = DisplayView::GetTrayDisplayMessage(additional_message_out);
      return true;
    }

    if (iter->second.configured_ui_scale() !=
        old_iter->second.configured_ui_scale()) {
      *message_out = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetDisplayName(iter->first),
          GetDisplaySize(iter->first));
      return true;
    }
    if (iter->second.rotation() != old_iter->second.rotation()) {
      int rotation_text_id = 0;
      switch (iter->second.rotation()) {
        case gfx::Display::ROTATE_0:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_STANDARD_ORIENTATION;
          break;
        case gfx::Display::ROTATE_90:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_90;
          break;
        case gfx::Display::ROTATE_180:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_180;
          break;
        case gfx::Display::ROTATE_270:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_270;
          break;
      }
      *message_out = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED,
          GetDisplayName(iter->first),
          l10n_util::GetStringUTF16(rotation_text_id));
      return true;
    }
  }

  // Found nothing special
  return false;
}

void TrayDisplay::CreateOrUpdateNotification(
    const base::string16& message,
    const base::string16& additional_message) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(
      kNotificationId, false /* by_user */);

  if (message.empty())
    return;

  // Don't display notifications for accelerometer triggered screen rotations.
  // See http://crbug.com/364949
  if (Shell::GetInstance()->maximize_mode_controller()->
      in_set_screen_rotation()) {
    return;
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId,
      message,
      additional_message,
      bundle.GetImageNamed(IDR_AURA_NOTIFICATION_DISPLAY),
      base::string16(),  // display_source
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          system_notifier::kNotifierDisplay),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickedDelegate(
          base::Bind(&OpenSettings))));

    message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

views::View* TrayDisplay::CreateDefaultView(user::LoginStatus status) {
  DCHECK(default_ == NULL);
  default_ = new DisplayView();
  return default_;
}

void TrayDisplay::DestroyDefaultView() {
  default_ = NULL;
}

void TrayDisplay::OnDisplayConfigurationChanged() {
  DisplayInfoMap old_info;
  UpdateDisplayInfo(&old_info);

  if (default_)
    default_->Update();

  if (!Shell::GetInstance()->system_tray_delegate()->
          ShouldShowDisplayNotification()) {
    return;
  }

  base::string16 message;
  base::string16 additional_message;
  if (GetDisplayMessageForNotification(old_info, &message, &additional_message))
    CreateOrUpdateNotification(message, additional_message);
}

base::string16 TrayDisplay::GetDefaultViewMessage() const {
  if (!default_ || !default_->visible())
    return base::string16();

  return static_cast<DisplayView*>(default_)->label()->text();
}

bool TrayDisplay::GetAccessibleStateForTesting(ui::AXViewState* state) {
  views::View* view = default_;
  if (view) {
    view->GetAccessibleState(state);
    return true;
  }
  return false;
}

}  // namespace ash
