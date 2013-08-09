// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_strings.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"

#if defined(OS_CHROMEOS)

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  // On Windows+Ash the Tray will not be hosted in ash::Shell.
  NOTREACHED();
  return NULL;
}

}  // namespace message_center

#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace internal {
namespace {

const int kWebNotificationIconSize = 31;
// Height of the art assets used in alternate shelf layout,
// see ash/ash_switches.h:UseAlternateShelfLayout.
const int kWebNotificationAlternateSize = 38;
const SkColor kWebNotificationColorNoUnread = SkColorSetA(SK_ColorWHITE, 128);
const SkColor kWebNotificationColorWithUnread = SK_ColorWHITE;

}

// Observes the change of work area (including temporary change by auto-hide)
// and notifies MessagePopupCollection.
class WorkAreaObserver : public ShelfLayoutManagerObserver,
                         public ShellObserver {
 public:
  WorkAreaObserver(message_center::MessagePopupCollection* collection,
                   ShelfLayoutManager* shelf);
  virtual ~WorkAreaObserver();

  void SetSystemTrayHeight(int height);

  // Overridden from ShellObserver:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // Overridden from ShelfLayoutManagerObserver:
  virtual void OnAutoHideStateChanged(ShelfAutoHideState new_state) OVERRIDE;

 private:
  message_center::MessagePopupCollection* collection_;
  ShelfLayoutManager* shelf_;
  int system_tray_height_;

  DISALLOW_COPY_AND_ASSIGN(WorkAreaObserver);
};

WorkAreaObserver::WorkAreaObserver(
    message_center::MessagePopupCollection* collection,
    ShelfLayoutManager* shelf)
    : collection_(collection),
      shelf_(shelf),
      system_tray_height_(0) {
  DCHECK(collection_);
  shelf_->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

WorkAreaObserver::~WorkAreaObserver() {
  Shell::GetInstance()->RemoveShellObserver(this);
  shelf_->RemoveObserver(this);
}

void WorkAreaObserver::SetSystemTrayHeight(int height) {
  system_tray_height_ = height;
  if (system_tray_height_ > 0 && ash::switches::UseAlternateShelfLayout())
    system_tray_height_ += message_center::kMarginBetweenItems;

  OnAutoHideStateChanged(shelf_->auto_hide_state());
}

void WorkAreaObserver::OnDisplayWorkAreaInsetsChanged() {
  collection_->OnDisplayBoundsChanged(
      Shell::GetScreen()->GetDisplayNearestWindow(
          shelf_->shelf_widget()->GetNativeView()));
}

void WorkAreaObserver::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      shelf_->shelf_widget()->GetNativeView());
  gfx::Rect work_area = display.work_area();
  int width = 0;
  if (shelf_->auto_hide_behavior() != SHELF_AUTO_HIDE_BEHAVIOR_NEVER) {
    width = (new_state == SHELF_AUTO_HIDE_HIDDEN) ?
        ShelfLayoutManager::kAutoHideSize :
        ShelfLayoutManager::GetPreferredShelfSize();
  }
  switch (shelf_->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
      work_area.Inset(0, 0, 0, width);
      if (system_tray_height_ > 0) {
        work_area.set_height(
            std::max(0, work_area.height() - system_tray_height_));
      }
      break;
    case SHELF_ALIGNMENT_LEFT:
      work_area.Inset(width, 0, 0, 0);
      // Popups appear on the left bottom only when UI is RTL.
      if (base::i18n::IsRTL() && system_tray_height_ > 0) {
        work_area.set_height(
            std::max(0, work_area.height() - system_tray_height_));
      }
      break;
    case SHELF_ALIGNMENT_RIGHT:
      work_area.Inset(0, 0, width, 0);
      // Popups appear on the right bottom only when UI isn't RTL.
      if (!base::i18n::IsRTL() && system_tray_height_ > 0) {
        work_area.set_height(
            std::max(0, work_area.height() - system_tray_height_));
      }
      break;
    case SHELF_ALIGNMENT_TOP:
      work_area.Inset(0, width, 0, 0);
      if (system_tray_height_ > 0) {
        work_area.set_y(work_area.y() + system_tray_height_);
        work_area.set_height(
            std::max(0, work_area.height() - system_tray_height_));
      }
      break;
  }
  collection_->SetDisplayInfo(work_area, display.bounds());
}

// Class to initialize and manage the WebNotificationBubble and
// TrayBubbleWrapper instances for a bubble.
class WebNotificationBubbleWrapper {
 public:
  // Takes ownership of |bubble| and creates |bubble_wrapper_|.
  WebNotificationBubbleWrapper(WebNotificationTray* tray,
                               message_center::MessageBubbleBase* bubble) {
    bubble_.reset(bubble);
    views::TrayBubbleView::AnchorAlignment anchor_alignment =
        tray->GetAnchorAlignment();
    views::TrayBubbleView::InitParams init_params =
        bubble->GetInitParams(anchor_alignment);
    views::View* anchor = tray->tray_container();
    if (anchor_alignment == views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
      gfx::Point bounds(anchor->width() / 2, 0);
      views::View::ConvertPointToWidget(anchor, &bounds);
      init_params.arrow_offset = bounds.x();
    }
    views::TrayBubbleView* bubble_view = views::TrayBubbleView::Create(
        tray->GetBubbleWindowContainer(), anchor, tray, &init_params);
    if (ash::switches::UseAlternateShelfLayout())
      bubble_view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_view));
    bubble->InitializeContents(bubble_view);
  }

  message_center::MessageBubbleBase* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  views::TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  scoped_ptr<message_center::MessageBubbleBase> bubble_;
  scoped_ptr<internal::TrayBubbleWrapper> bubble_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubbleWrapper);
};

class WebNotificationButton : public views::CustomButton {
 public:
  WebNotificationButton(views::ButtonListener* listener)
      : views::CustomButton(listener),
        is_bubble_visible_(false),
        unread_count_(0) {
    SetLayoutManager(new views::FillLayout);
    unread_label_ = new views::Label();
    SetupLabelForTray(unread_label_);
    AddChildView(unread_label_);
  }

  void SetBubbleVisible(bool visible) {
    if (visible == is_bubble_visible_)
      return;

    is_bubble_visible_ = visible;
    UpdateIconVisibility();
  }

  void SetUnreadCount(int unread_count) {
    // base::FormatNumber doesn't convert to arabic numeric characters.
    // TODO(mukai): use ICU to support conversion for such locales.
    unread_count_ = unread_count;
    // TODO(mukai): move NINE_PLUS message to ui_strings, it doesn't need to be
    // in ash_strings.
    unread_label_->SetText((unread_count > 9) ?
        l10n_util::GetStringUTF16(IDS_ASH_NOTIFICATION_UNREAD_COUNT_NINE_PLUS) :
        base::FormatNumber(unread_count));
    UpdateIconVisibility();
  }

 protected:
  // Overridden from views::ImageButton:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    if (ash::switches::UseAlternateShelfLayout())
      return gfx::Size(kWebNotificationAlternateSize,
                       kWebNotificationAlternateSize);
    return gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize);
  }

 private:
  void UpdateIconVisibility() {
    unread_label_->SetEnabledColor(
        (!is_bubble_visible_ && unread_count_ > 0) ?
        kWebNotificationColorWithUnread : kWebNotificationColorNoUnread);
    SchedulePaint();
  }

  bool is_bubble_visible_;
  int unread_count_;

  views::Label* unread_label_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButton);
};

}  // namespace internal

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : TrayBackgroundView(status_area_widget),
      button_(NULL),
      show_message_center_on_unlock_(false),
      should_update_tray_content_(false),
      should_block_shelf_auto_hide_(false) {
  button_ = new internal::WebNotificationButton(this);
  button_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON);
  tray_container()->AddChildView(button_);
  SetContentsBackground();
  tray_container()->set_border(NULL);
  SetVisible(false);
  message_center_tray_.reset(new message_center::MessageCenterTray(
      this,
      message_center::MessageCenter::Get()));
  OnMessageCenterTrayChanged();
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_collection_.reset();
  work_area_observer_.reset();
}

// Public methods.

bool WebNotificationTray::ShowMessageCenterInternal(bool show_settings) {
  if (!ShouldShowMessageCenter())
    return false;

  should_block_shelf_auto_hide_ = true;
  message_center::MessageCenterBubble* message_center_bubble =
      new message_center::MessageCenterBubble(
          message_center(),
          message_center_tray_.get(),
          ash::switches::UseAlternateShelfLayout());

  int max_height = 0;
  aura::Window* status_area_window = status_area_widget()->GetNativeView();
  switch (GetShelfLayoutManager()->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM: {
      gfx::Rect shelf_bounds = GetShelfLayoutManager()->GetIdealBounds();
      max_height = shelf_bounds.y();
      break;
    }
    case SHELF_ALIGNMENT_TOP: {
      aura::RootWindow* root = status_area_window->GetRootWindow();
      max_height =
          root->bounds().height() - status_area_window->bounds().height();
      break;
    }
    case SHELF_ALIGNMENT_LEFT:
    case SHELF_ALIGNMENT_RIGHT: {
      // Assume that the bottom line of the status area widget and the bubble
      // are aligned.
      max_height = status_area_window->GetBoundsInRootWindow().bottom();
      break;
    }
    default:
      NOTREACHED();
  }

  message_center_bubble->SetMaxHeight(std::max(0,
                                               max_height - GetTraySpacing()));
  if (show_settings)
    message_center_bubble->SetSettingsVisible();
  message_center_bubble_.reset(
      new internal::WebNotificationBubbleWrapper(this, message_center_bubble));

  status_area_widget()->SetHideSystemNotifications(true);
  GetShelfLayoutManager()->UpdateAutoHideState();
  button_->SetBubbleVisible(true);
  return true;
}

bool WebNotificationTray::ShowMessageCenter() {
  return ShowMessageCenterInternal(false /* show_settings */);
}

void WebNotificationTray::HideMessageCenter() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  should_block_shelf_auto_hide_ = false;
  show_message_center_on_unlock_ = false;
  status_area_widget()->SetHideSystemNotifications(false);
  GetShelfLayoutManager()->UpdateAutoHideState();
  button_->SetBubbleVisible(false);
}

void WebNotificationTray::SetSystemTrayHeight(int height) {
  if (!work_area_observer_)
    return;
  work_area_observer_->SetSystemTrayHeight(height);
}

bool WebNotificationTray::ShowPopups() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED ||
      message_center_bubble() ||
      !status_area_widget()->ShouldShowWebNotifications()) {
    return false;
  }

  popup_collection_.reset(new message_center::MessagePopupCollection(
      ash::Shell::GetContainer(
          GetWidget()->GetNativeView()->GetRootWindow(),
          internal::kShellWindowId_StatusContainer),
      message_center(),
      message_center_tray_.get(),
      ash::switches::UseAlternateShelfLayout()));
  work_area_observer_.reset(new internal::WorkAreaObserver(
      popup_collection_.get(), GetShelfLayoutManager()));
  return true;
}

void WebNotificationTray::HidePopups() {
  popup_collection_.reset();
  work_area_observer_.reset();
}

// Private methods.

bool WebNotificationTray::ShouldShowMessageCenter() {
  return status_area_widget()->login_status() != user::LOGGED_IN_LOCKED &&
      !(status_area_widget()->system_tray() &&
        status_area_widget()->system_tray()->HasNotificationBubble());
}

void WebNotificationTray::ShowQuietModeMenu(const ui::Event& event) {
  base::AutoReset<bool> reset(&should_block_shelf_auto_hide_, true);
  scoped_ptr<ui::MenuModel> menu_model(
      message_center_tray_->CreateQuietModeMenu());
  quiet_mode_menu_runner_.reset(new views::MenuRunner(menu_model.get()));
  gfx::Point point;
  views::View::ConvertPointToScreen(this, &point);
  if (quiet_mode_menu_runner_->RunMenuAt(
      GetWidget(),
      NULL,
      gfx::Rect(point, bounds().size()),
      views::MenuItemView::BUBBLE_ABOVE,
      ui::GetMenuSourceTypeForEvent(event),
      views::MenuRunner::HAS_MNEMONICS) == views::MenuRunner::MENU_DELETED)
    return;

  quiet_mode_menu_runner_.reset();
  GetShelfLayoutManager()->UpdateAutoHideState();
}

bool WebNotificationTray::ShouldShowQuietModeMenu(const ui::Event& event) {
  // TODO(mukai): Add keyboard event handler.
  if (!event.IsMouseEvent())
    return false;

  const ui::MouseEvent* mouse_event =
      static_cast<const ui::MouseEvent*>(&event);

  return mouse_event->IsRightMouseButton();
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (login_status == user::LOGGED_IN_LOCKED) {
    show_message_center_on_unlock_ =
        message_center_tray_->HideMessageCenterBubble();
    message_center_tray_->HidePopupBubble();
  } else {
    // Only try once to show the message center bubble on login status change,
    // so always set |show_message_center_on_unlock_| to false.
    if (show_message_center_on_unlock_)
      message_center_tray_->ShowMessageCenterBubble();
    show_message_center_on_unlock_ = false;
  }
  OnMessageCenterTrayChanged();
}

bool WebNotificationTray::ShouldBlockLauncherAutoHide() const {
  return should_block_shelf_auto_hide_;
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() &&
          message_center_bubble()->bubble()->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  return false;
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (!IsMessageCenterBubbleVisible())
    message_center_tray_->ShowMessageCenterBubble();
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container()->set_border(NULL);
  // Destroy any existing bubble so that it will be rebuilt correctly.
  message_center_tray_->HideMessageCenterBubble();
  message_center_tray_->HidePopupBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (message_center_bubble()) {
    message_center_bubble()->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(message_center_bubble()->bubble_view());
  }
}

base::string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

void WebNotificationTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    message_center_tray_->HideMessageCenterBubble();
  } else if (popup_collection_.get()) {
    message_center_tray_->HidePopupBubble();
  }
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (ShouldShowQuietModeMenu(event)) {
    ShowQuietModeMenu(event);
    return true;
  }

  if (message_center_bubble())
    message_center_tray_->HideMessageCenterBubble();
  else
    message_center_tray_->ShowMessageCenterBubble();
  return true;
}

void WebNotificationTray::BubbleViewDestroyed() {
  if (message_center_bubble())
    message_center_bubble()->bubble()->BubbleViewDestroyed();
}

void WebNotificationTray::OnMouseEnteredView() {}

void WebNotificationTray::OnMouseExitedView() {}

base::string16 WebNotificationTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect WebNotificationTray::GetAnchorRect(
    views::Widget* anchor_widget,
    views::TrayBubbleView::AnchorType anchor_type,
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void WebNotificationTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

bool WebNotificationTray::ShowNotifierSettings() {
  if (message_center_bubble()) {
    static_cast<message_center::MessageCenterBubble*>(
        message_center_bubble()->bubble())->SetSettingsVisible();
    return true;
  }
  return ShowMessageCenterInternal(true /* show_settings */);
}

message_center::MessageCenterTray* WebNotificationTray::GetMessageCenterTray() {
  return message_center_tray_.get();
}

bool WebNotificationTray::IsPressed() {
  return IsMessageCenterBubbleVisible();
}

void WebNotificationTray::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(button_, sender);
  PerformAction(event);
}

void WebNotificationTray::OnMessageCenterTrayChanged() {
  // Do not update the tray contents directly. Multiple change events can happen
  // consecutively, and calling Update in the middle of those events will show
  // intermediate unread counts for a moment.
  should_update_tray_content_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&WebNotificationTray::UpdateTrayContent, AsWeakPtr()));
}

void WebNotificationTray::UpdateTrayContent() {
  if (!should_update_tray_content_)
    return;
  should_update_tray_content_ = false;

  message_center::MessageCenter* message_center =
      message_center_tray_->message_center();
  button_->SetUnreadCount(message_center->UnreadNotificationCount());
  if (IsMessageCenterBubbleVisible())
    button_->SetState(views::CustomButton::STATE_PRESSED);
  else
    button_->SetState(views::CustomButton::STATE_NORMAL);
  SetVisible((status_area_widget()->login_status() != user::LOGGED_IN_NONE) &&
             (status_area_widget()->login_status() != user::LOGGED_IN_LOCKED) &&
             (message_center->NotificationCount() > 0));
  Layout();
  SchedulePaint();
}

bool WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center
  if (!message_center_bubble())
    return false;

  message_center_tray_->HideMessageCenterBubble();
  return true;
}

message_center::MessageCenter* WebNotificationTray::message_center() {
  return message_center_tray_->message_center();
}

// Methods for testing

bool WebNotificationTray::IsPopupVisible() const {
  return message_center_tray_->popups_visible();
}

message_center::MessageCenterBubble*
WebNotificationTray::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble())
    return NULL;
  return static_cast<message_center::MessageCenterBubble*>(
      message_center_bubble()->bubble());
}

}  // namespace ash
