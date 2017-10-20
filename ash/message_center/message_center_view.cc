// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/message_center_view.h"

#include <list>
#include <map>

#include "ash/message_center/message_center_button_bar.h"
#include "ash/message_center/message_center_style.h"
#include "ash/message_center/notifier_settings_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::MessageCenterTray;
using message_center::MessageView;
using message_center::Notification;
using message_center::NotificationList;

namespace ash {

// static
const SkColor MessageCenterView::kBackgroundColor =
    SkColorSetARGB(0xF2, 0xf0, 0xf0, 0xf2);

// static
const size_t MessageCenterView::kMaxVisibleNotifications = 100;

// static
bool MessageCenterView::disable_animation_for_testing = false;

namespace {

constexpr int kDefaultAnimationDurationMs = 500;
constexpr int kMinScrollViewHeight = 77;
constexpr int kEmptyViewHeight = 96;
constexpr gfx::Insets kEmptyViewPadding(0, 0, 24, 0);

void SetViewHierarchyEnabled(views::View* view, bool enabled) {
  for (int i = 0; i < view->child_count(); i++)
    SetViewHierarchyEnabled(view->child_at(i), enabled);
  view->SetEnabled(enabled);
}

// View that is shown when there are no notifications.
class EmptyNotificationView : public views::View {
 public:
  EmptyNotificationView() {
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kVertical, kEmptyViewPadding, 0);
    layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(layout);

    views::ImageView* icon = new views::ImageView();
    icon->SetImage(gfx::CreateVectorIcon(
        kNotificationCenterAllDoneIcon, message_center_style::kEmptyIconSize,
        message_center_style::kEmptyViewColor));
    icon->SetBorder(
        views::CreateEmptyBorder(message_center_style::kEmptyIconPadding));
    AddChildView(icon);

    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_NO_MESSAGES));
    label->SetEnabledColor(message_center_style::kEmptyViewColor);
    // "Roboto-Medium, 12sp" is specified in the mock.
    label->SetFontList(message_center_style::GetFontListForSizeAndWeight(
        message_center_style::kEmptyLabelSize, gfx::Font::Weight::MEDIUM));
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    AddChildView(label);
  }

  // views::View:
  int GetHeightForWidth(int w) const override { return kEmptyViewHeight; }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyNotificationView);
};

}  // namespace

// MessageCenterView ///////////////////////////////////////////////////////////

MessageCenterView::MessageCenterView(MessageCenter* message_center,
                                     MessageCenterTray* tray,
                                     int max_height,
                                     bool initially_settings_visible)
    : message_center_(message_center),
      tray_(tray),
      scroller_(nullptr),
      settings_view_(nullptr),
      no_notifications_view_(nullptr),
      button_bar_(nullptr),
      settings_visible_(initially_settings_visible),
      source_view_(nullptr),
      source_height_(0),
      target_view_(nullptr),
      target_height_(0),
      is_closing_(false),
      is_locked_(message_center_->IsLockedState()),
      mode_(Mode::NO_NOTIFICATIONS),
      context_menu_controller_(this),
      focus_manager_(nullptr) {
  if (is_locked_)
    mode_ = Mode::LOCKED;
  else if (initially_settings_visible)
    mode_ = Mode::SETTINGS;

  message_center_->AddObserver(this);
  set_notify_enter_exit_on_child(true);
  SetBackground(views::CreateSolidBackground(kBackgroundColor));

  message_center::NotifierSettingsProvider* notifier_settings_provider =
      message_center_->GetNotifierSettingsProvider();
  button_bar_ = new MessageCenterButtonBar(
      this, message_center, notifier_settings_provider,
      initially_settings_visible, GetButtonBarTitle());
  button_bar_->SetCloseAllButtonEnabled(false);

  const int button_height = button_bar_->GetPreferredSize().height();

  scroller_ = new views::ScrollView();
  scroller_->SetBackgroundColor(kBackgroundColor);
  scroller_->ClipHeightTo(kMinScrollViewHeight, max_height - button_height);
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  scroller_->SetHorizontalScrollBar(new views::OverlayScrollBar(true));

  message_list_view_.reset(new MessageListView());
  message_list_view_->set_scroller(scroller_);
  message_list_view_->set_owned_by_client();
  message_list_view_->AddObserver(this);

  // We want to swap the contents of the scroll view between the empty list
  // view and the message list view, without constructing them afresh each
  // time.  So, since the scroll view deletes old contents each time you
  // set the contents (regardless of the |owned_by_client_| setting) we need
  // an intermediate view for the contents whose children we can swap in and
  // out.
  views::View* scroller_contents = new views::View();
  scroller_contents->SetLayoutManager(new views::FillLayout());
  scroller_contents->AddChildView(message_list_view_.get());
  scroller_->SetContents(scroller_contents);

  settings_view_ = new NotifierSettingsView(notifier_settings_provider);

  no_notifications_view_ = new EmptyNotificationView();

  scroller_->SetVisible(false);  // Because it has no notifications at first.
  settings_view_->SetVisible(mode_ == Mode::SETTINGS);
  no_notifications_view_->SetVisible(mode_ == Mode::NO_NOTIFICATIONS);

  AddChildView(scroller_);
  AddChildView(settings_view_);
  AddChildView(no_notifications_view_);
  AddChildView(button_bar_);
}

MessageCenterView::~MessageCenterView() {
  message_list_view_->RemoveObserver(this);

  if (!is_closing_)
    message_center_->RemoveObserver(this);

  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);
}

void MessageCenterView::Init() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void MessageCenterView::SetNotifications(
    const NotificationList::Notifications& notifications) {
  if (is_closing_)
    return;

  int index = 0;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin();
       iter != notifications.end(); ++iter) {
    AddNotificationAt(*(*iter), index++);

    message_center_->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    if (message_list_view_->GetNotificationCount() >=
        kMaxVisibleNotifications) {
      break;
    }
  }

  Update(false /* animate */);
  scroller_->RequestFocus();
}

void MessageCenterView::SetSettingsVisible(bool visible) {
  settings_visible_ = visible;
  Update(true /* animate */);
}

void MessageCenterView::ClearAllClosableNotifications() {
  if (is_closing_)
    return;

  is_clearing_all_notifications_ = true;
  UpdateButtonBarStatus();
  SetViewHierarchyEnabled(scroller_, false);
  message_list_view_->ClearAllClosableNotifications(
      scroller_->GetVisibleRect());
}

void MessageCenterView::OnAllNotificationsCleared() {
  SetViewHierarchyEnabled(scroller_, true);
  button_bar_->SetCloseAllButtonEnabled(false);

  // The status of buttons will be updated after removing all notifications.

  // Action by user.
  message_center_->RemoveAllNotifications(
      true /* by_user */,
      message_center::MessageCenter::RemoveType::NON_PINNED);
  is_clearing_all_notifications_ = false;
}

size_t MessageCenterView::NumMessageViewsForTest() const {
  return message_list_view_->GetNotificationCount();
}

void MessageCenterView::OnSettingsChanged() {
  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetIsClosing(bool is_closing) {
  is_closing_ = is_closing;
  if (is_closing)
    message_center_->RemoveObserver(this);
  else
    message_center_->AddObserver(this);
}

void MessageCenterView::OnDidChangeFocus(views::View* before,
                                         views::View* now) {
  // Update the button visibility when the focus state is changed.
  size_t count = message_list_view_->GetNotificationCount();
  for (size_t i = 0; i < count; ++i) {
    MessageView* view = message_list_view_->GetNotificationAt(i);
    // ControlButtonsView is not in the same view hierarchy on ARC++
    // notifications, so check it separately.
    if (view->Contains(before) || view->Contains(now) ||
        (view->GetControlButtonsView() &&
         (view->GetControlButtonsView()->Contains(before) ||
          view->GetControlButtonsView()->Contains(now)))) {
      view->UpdateControlButtonsVisibility();
    }

    // Ensure that a notification is not removed or added during iteration.
    DCHECK_EQ(count, message_list_view_->GetNotificationCount());
  }
}

void MessageCenterView::Layout() {
  if (is_closing_)
    return;

  int button_height = button_bar_->GetHeightForWidth(width());
  int settings_height =
      std::min(GetSettingsHeightForWidth(width()), height() - button_height);

  // In order to keep the fix for https://crbug.com/767805 working,
  // we have to always call SetBounds of scroller_.
  // TODO(tetsui): Fix the bug above without calling SetBounds, as SetBounds
  // invokes Layout() which is a heavy operation.
  scroller_->SetBounds(0, 0, width(), height() - button_height);
  if (settings_view_->visible()) {
    settings_view_->SetBounds(0, height() - settings_height, width(),
                              settings_height);
  }
  if (no_notifications_view_->visible())
    no_notifications_view_->SetBounds(0, 0, width(), kEmptyViewHeight);
  button_bar_->SetBounds(0, height() - button_height - settings_height, width(),
                         button_height);
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
}

gfx::Size MessageCenterView::CalculatePreferredSize() const {
  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(0);
    if (child->visible())
      width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetHeightForWidth(width));
}

int MessageCenterView::GetHeightForWidth(int width) const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating()) {
    return button_bar_->GetHeightForWidth(width) +
           GetContentHeightDuringAnimation(width);
  }

  int content_height = 0;
  if (mode_ == Mode::NOTIFICATIONS)
    content_height += scroller_->GetHeightForWidth(width);
  else if (mode_ == Mode::SETTINGS)
    content_height += settings_view_->GetHeightForWidth(width);
  else if (no_notifications_view_->visible())
    content_height += no_notifications_view_->GetHeightForWidth(width);
  return button_bar_->GetHeightForWidth(width) + content_height;
}

bool MessageCenterView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do not rely on the default scroll event handler of ScrollView because
  // the scroll happens only when the focus is on the ScrollView. The
  // notification center will allow the scrolling even when the focus is on
  // the buttons.
  if (scroller_->bounds().Contains(event.location()))
    return scroller_->OnMouseWheel(event);
  return views::View::OnMouseWheel(event);
}

void MessageCenterView::OnMouseExited(const ui::MouseEvent& event) {
  if (is_closing_)
    return;

  message_list_view_->ResetRepositionSession();
  Update(true /* animate */);
}

void MessageCenterView::OnNotificationAdded(const std::string& id) {
  int index = 0;
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator
           iter = notifications.begin();
       iter != notifications.end(); ++iter, ++index) {
    if ((*iter)->id() == id) {
      AddNotificationAt(*(*iter), index);
      break;
    }
    if (message_list_view_->GetNotificationCount() >=
        kMaxVisibleNotifications) {
      break;
    }
  }
  Update(true /* animate */);
}

void MessageCenterView::OnNotificationRemoved(const std::string& id,
                                              bool by_user) {
  auto view_pair = message_list_view_->GetNotificationById(id);
  MessageView* view = view_pair.second;
  if (!view)
    return;
  size_t index = view_pair.first;

  // We skip repositioning during clear-all anomation, since we don't need keep
  // positions.
  if (by_user && !is_clearing_all_notifications_) {
    message_list_view_->SetRepositionTarget(view->bounds());
    // Moves the keyboard focus to the next notification if the removed
    // notification is focused so that the user can dismiss notifications
    // without re-focusing by tab key.
    if (view->IsCloseButtonFocused() || view->HasFocus()) {
      views::View* next_focused_view = nullptr;
      if (message_list_view_->GetNotificationCount() > index + 1)
        next_focused_view = message_list_view_->GetNotificationAt(index + 1);
      else if (index > 0)
        next_focused_view = message_list_view_->GetNotificationAt(index - 1);

      if (next_focused_view) {
        if (view->IsCloseButtonFocused()) {
          // Safe cast since all views in MessageListView are MessageViews.
          static_cast<MessageView*>(next_focused_view)
              ->RequestFocusOnCloseButton();
        } else {
          next_focused_view->RequestFocus();
        }
      }
    }
  }
  message_list_view_->RemoveNotification(view);
  Update(true /* animate */);
}

// This is a separate function so we can override it in tests.
bool MessageCenterView::SetRepositionTarget() {
  // Set the item on the mouse cursor as the reposition target so that it
  // should stick to the current position over the update.
  if (message_list_view_->IsMouseHovered()) {
    size_t count = message_list_view_->GetNotificationCount();
    for (size_t i = 0; i < count; ++i) {
      const views::View* hover_view = message_list_view_->GetNotificationAt(i);

      if (hover_view->IsMouseHovered()) {
        message_list_view_->SetRepositionTarget(hover_view->bounds());
        return true;
      }
    }
  }
  return false;
}

void MessageCenterView::OnNotificationUpdated(const std::string& id) {
  // If there is no reposition target anymore, make sure to reset the reposition
  // session.
  if (!SetRepositionTarget())
    message_list_view_->ResetRepositionSession();

  UpdateNotification(id);
}

void MessageCenterView::OnLockedStateChanged(bool locked) {
  is_locked_ = locked;
  UpdateButtonBarStatus();
  Update(true /* animate */);
}

void MessageCenterView::OnQuietModeChanged(bool is_quiet_mode) {
  settings_view_->SetQuietModeState(is_quiet_mode);
  button_bar_->SetQuietModeState(is_quiet_mode);
}

void MessageCenterView::ClickOnNotification(
    const std::string& notification_id) {
  message_center_->ClickOnNotification(notification_id);
}

void MessageCenterView::RemoveNotification(const std::string& notification_id,
                                           bool by_user) {
  message_center_->RemoveNotification(notification_id, by_user);
}

std::unique_ptr<ui::MenuModel> MessageCenterView::CreateMenuModel(
    const message_center::Notification& notification) {
  return tray_->CreateNotificationMenuModel(notification);
}

void MessageCenterView::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  message_center_->ClickOnNotificationButton(notification_id, button_index);
}

void MessageCenterView::ClickOnSettingsButton(
    const std::string& notification_id) {
  message_center_->ClickOnSettingsButton(notification_id);
}

void MessageCenterView::UpdateNotificationSize(
    const std::string& notification_id) {
  // TODO(edcourtney, yoshiki): We don't call OnNotificationUpdated directly
  // because it resets the reposition session, which can end up deleting
  // notification items when it cancels animations. This causes problems for
  // ARC notifications. See crbug.com/714493. OnNotificationUpdated should not
  // have to consider the reposition session, but OnMouseEntered and
  // OnMouseExited don't work properly for ARC notifications at the moment.
  // See crbug.com/714587.
  UpdateNotification(notification_id);
}

void MessageCenterView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());

  message_center::Visibility visibility =
      mode_ == Mode::SETTINGS ? message_center::VISIBILITY_SETTINGS
                              : message_center::VISIBILITY_MESSAGE_CENTER;
  message_center_->SetVisibility(visibility);

  if (source_view_) {
    source_view_->SetVisible(false);
  }
  if (target_view_)
    target_view_->SetVisible(true);
  settings_transition_animation_.reset();
  PreferredSizeChanged();
  Layout();

  // We should update minimum fixed height based on new |scroller_| height.
  // This is required when switching between message list and settings panel.
  if (!scroller_->visible())
    message_list_view_->ResetRepositionSession();
}

void MessageCenterView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MessageCenterView::AnimationCanceled(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  AnimationEnded(animation);
}

void MessageCenterView::AddNotificationAt(const Notification& notification,
                                          int index) {
  MessageView* view = message_center::MessageViewFactory::Create(
      this, notification, false);  // Not top-level.

  // TODO(yoshiki): Temporary disable context menu on custom notifications.
  // See crbug.com/750307 for detail.
  if (notification.type() != message_center::NOTIFICATION_TYPE_CUSTOM &&
      notification.delegate() &&
      notification.delegate()->ShouldDisplaySettingsButton()) {
    view->set_context_menu_controller(&context_menu_controller_);
  }

  view->set_scroller(scroller_);
  message_list_view_->AddNotificationAt(view, index);
}

base::string16 MessageCenterView::GetButtonBarTitle() const {
  if (is_locked_)
    return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_FOOTER_LOCKSCREEN);

  return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_FOOTER_TITLE);
}

void MessageCenterView::Update(bool animate) {
  bool no_message_views = (message_list_view_->GetNotificationCount() == 0);

  if (is_locked_)
    SetVisibilityMode(Mode::LOCKED, animate);
  else if (settings_visible_)
    SetVisibilityMode(Mode::SETTINGS, animate);
  else if (no_message_views)
    SetVisibilityMode(Mode::NO_NOTIFICATIONS, animate);
  else
    SetVisibilityMode(Mode::NOTIFICATIONS, animate);

  if (no_message_views) {
    scroller_->SetFocusBehavior(FocusBehavior::NEVER);
  } else {
#if defined(OS_MACOSX)
    scroller_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
    scroller_->SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
  }

  UpdateButtonBarStatus();

  if (scroller_->visible())
    scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetVisibilityMode(Mode mode, bool animate) {
  if (is_closing_)
    return;

  if (mode == mode_)
    return;

  if (mode_ == Mode::NOTIFICATIONS)
    source_view_ = scroller_;
  else if (mode_ == Mode::SETTINGS)
    source_view_ = settings_view_;
  else if (mode_ == Mode::NO_NOTIFICATIONS)
    source_view_ = no_notifications_view_;
  else
    source_view_ = nullptr;

  if (mode == Mode::NOTIFICATIONS)
    target_view_ = scroller_;
  else if (mode == Mode::SETTINGS)
    target_view_ = settings_view_;
  else if (mode == Mode::NO_NOTIFICATIONS)
    target_view_ = no_notifications_view_;
  else
    target_view_ = nullptr;

  mode_ = mode;

  source_height_ = source_view_ ? source_view_->GetHeightForWidth(width()) : 0;
  target_height_ = target_view_ ? target_view_->GetHeightForWidth(width()) : 0;

  if (source_view_)
    source_view_->SetVisible(true);
  if (target_view_)
    target_view_->SetVisible(true);

  if (!animate || disable_animation_for_testing) {
    AnimationEnded(nullptr);
    return;
  }

  settings_transition_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  settings_transition_animation_->SetSlideDuration(kDefaultAnimationDurationMs);
  settings_transition_animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);
  settings_transition_animation_->Show();
}

void MessageCenterView::UpdateButtonBarStatus() {
  // Disables all buttons during animation of cleaning of all notifications.
  if (is_clearing_all_notifications_) {
    button_bar_->SetSettingsAndQuietModeButtonsEnabled(false);
    button_bar_->SetCloseAllButtonEnabled(false);
    return;
  }

  button_bar_->SetBackArrowVisible(mode_ == Mode::SETTINGS);
  button_bar_->SetButtonsVisible(!is_locked_);
  button_bar_->SetTitle(GetButtonBarTitle());

  if (!is_locked_)
    EnableCloseAllIfAppropriate();
}

void MessageCenterView::EnableCloseAllIfAppropriate() {
  if (mode_ == Mode::NOTIFICATIONS) {
    bool no_closable_views = true;
    size_t count = message_list_view_->GetNotificationCount();
    for (size_t i = 0; i < count; ++i) {
      if (!message_list_view_->GetNotificationAt(i)->GetPinned()) {
        no_closable_views = false;
        break;
      }
    }
    button_bar_->SetCloseAllButtonEnabled(!no_closable_views);
  } else {
    // Disable the close-all button since no notification is visible.
    button_bar_->SetCloseAllButtonEnabled(false);
  }
}

void MessageCenterView::SetNotificationViewForTest(MessageView* view) {
  message_list_view_->AddNotificationAt(view, 0);
}

void MessageCenterView::UpdateNotification(const std::string& id) {
  MessageView* view = message_list_view_->GetNotificationById(id).second;
  if (!view)
    return;
  Notification* notification = message_center_->FindVisibleNotificationById(id);
  if (notification) {
    int old_width = view->width();
    int old_height = view->height();
    bool old_pinned = view->GetPinned();
    message_list_view_->UpdateNotification(view, *notification);
    if (view->GetHeightForWidth(old_width) != old_height) {
      Update(true /* animate */);
    } else if (view->GetPinned() != old_pinned) {
      // Animate flag is false, since the pinned flag transition doesn't need
      // animation.
      Update(false /* animate */);
    }
  }

  // Notify accessibility that the contents have changed.
  view->NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, false);
}

int MessageCenterView::GetSettingsHeightForWidth(int width) const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating() &&
      (source_view_ == settings_view_ || target_view_ == settings_view_)) {
    return settings_transition_animation_->CurrentValueBetween(
        target_view_ == settings_view_ ? 0 : source_height_,
        source_view_ == settings_view_ ? 0 : target_height_);
  } else {
    return mode_ == Mode::SETTINGS ? settings_view_->GetHeightForWidth(width)
                                   : 0;
  }
}

int MessageCenterView::GetContentHeightDuringAnimation(int width) const {
  DCHECK(settings_transition_animation_);
  int content_height = settings_transition_animation_->CurrentValueBetween(
      target_view_ == settings_view_ ? 0 : source_height_,
      source_view_ == settings_view_ ? 0 : target_height_);
  if (target_view_ == settings_view_)
    content_height = std::max(source_height_, content_height);
  if (source_view_ == settings_view_)
    content_height = std::max(target_height_, content_height);
  return content_height;
}

}  // namespace ash
