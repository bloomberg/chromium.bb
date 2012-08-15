// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Tray constants
const int kTrayContainerVeritcalPaddingBottomAlignment = 3;
const int kTrayContainerHorizontalPaddingBottomAlignment = 1;
const int kTrayContainerVerticalPaddingVerticalAlignment = 1;
const int kTrayContainerHorizontalPaddingVerticalAlignment = 0;
const int kPaddingFromLeftEdgeOfSystemTrayBottomAlignment = 8;
const int kPaddingFromTopEdgeOfSystemTrayVerticalAlignment = 10;
const int kTrayWidth = 40;
const int kTrayHeight = 31;
const int kTraySideWidth = 32;
const int kTraySideHeight = 24;

// Web Notification Bubble constants
const int kWebNotificationBubbleMinHeight = 80;
const int kWebNotificationBubbleMaxHeight = 480;
// Delay laying out the Bubble until all notifications have been added and icons
// have had a chance to load.
const int kUpdateDelayMs = 50;
// Limit the number of visible notifications.
const int kMaxVisibleNotifications = 100;
const int kAutocloseDelaySeconds = 5;
const SkColor kMessageCountColor = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);
const SkColor kMessageCountDimmedColor = SkColorSetARGB(0x60, 0xff, 0xff, 0xff);

// Individual notifications constants
const int kWebNotificationWidth = 320;
const int kWebNotificationButtonWidth = 32;
const int kWebNotificationIconSize = 40;

// Menu constants
const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kShowSettingsCommand = 2;

std::string GetNotificationText(int notification_count) {
  if (notification_count >= 100)
    return "99+";
  return base::StringPrintf("%d", notification_count);
}

}  // namespace

namespace ash {

namespace internal {

struct WebNotification {
  WebNotification() : is_read(false) {}
  std::string id;
  string16 title;
  string16 message;
  string16 display_source;
  std::string extension_id;
  gfx::ImageSkia image;
  bool is_read;
};

// A helper class to manage the list of notifications.
class WebNotificationList {
 public:
  typedef std::list<WebNotification> Notifications;

  WebNotificationList()
      : is_visible_(false),
        unread_count_(0) {
  }

  void SetIsVisible(bool visible) {
    if (is_visible_ == visible)
      return;
    is_visible_ = visible;
    if (visible) {
      // Clear the unread count when the list is shown.
      unread_count_ = 0;
    } else {
      // Mark all notifications as read when the list is hidden.
      for (Notifications::iterator iter = notifications_.begin();
           iter != notifications_.end(); ++iter) {
        iter->is_read = true;
      }
    }
  }

  void AddNotification(const std::string& id,
                       const string16& title,
                       const string16& message,
                       const string16& display_source,
                       const std::string& extension_id) {
    WebNotification notification;
    notification.id = id;
    notification.title = title;
    notification.message = message;
    notification.display_source = display_source;
    notification.extension_id = extension_id;
    notification.is_read = false;
    PushNotification(notification);
  }

  void UpdateNotificationMessage(const std::string& old_id,
                                 const std::string& new_id,
                                 const string16& title,
                                 const string16& message) {
    Notifications::iterator iter = GetNotification(old_id);
    if (iter == notifications_.end())
      return;
    // Copy and update notification, then move it to the front of the list.
    WebNotification notification(*iter);
    notification.id = new_id;
    notification.title = title;
    notification.message = message;
    notification.is_read = false;
    EraseNotification(iter);
    PushNotification(notification);
  }

  bool RemoveNotification(const std::string& id) {
    Notifications::iterator iter = GetNotification(id);
    if (iter == notifications_.end())
      return false;
    EraseNotification(iter);
    return true;
  }

  void RemoveAllNotifications() {
    notifications_.clear();
  }

  void SendRemoveNotificationsBySource(WebNotificationTray* tray,
                                       const std::string& id) {
    Notifications::iterator source_iter = GetNotification(id);
    if (source_iter == notifications_.end())
      return;
    string16 display_source = source_iter->display_source;
    for (Notifications::iterator loopiter = notifications_.begin();
         loopiter != notifications_.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->display_source == display_source)
        tray->SendRemoveNotification(curiter->id);
    }
  }

  void SendRemoveNotificationsByExtension(WebNotificationTray* tray,
                                          const std::string& id) {
    Notifications::iterator source_iter = GetNotification(id);
    if (source_iter == notifications_.end())
      return;
    std::string extension_id = source_iter->extension_id;
    for (Notifications::iterator loopiter = notifications_.begin();
         loopiter != notifications_.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->extension_id == extension_id)
        tray->SendRemoveNotification(curiter->id);
    }
  }

  bool SetNotificationImage(const std::string& id,
                            const gfx::ImageSkia& image) {
    Notifications::iterator iter = GetNotification(id);
    if (iter == notifications_.end())
      return false;
    iter->image = image;
    return true;
  }

  std::string GetFirstId() {
    if (notifications_.empty())
      return std::string();
    return notifications_.front().id;
  }

  bool HasNotification(const std::string& id) {
    return GetNotification(id) != notifications_.end();
  }

  const Notifications& notifications() const { return notifications_; }
  int unread_count() const { return unread_count_; }

 private:
  Notifications::iterator GetNotification(const std::string& id) {
    for (Notifications::iterator iter = notifications_.begin();
         iter != notifications_.end(); ++iter) {
      if (iter->id == id)
        return iter;
    }
    return notifications_.end();
  }

  void EraseNotification(Notifications::iterator iter) {
    if (!is_visible_ && !iter->is_read)
      --unread_count_;
    notifications_.erase(iter);
  }

  void PushNotification(const WebNotification& notification) {
    // Ensure that notification.id is unique by erasing any existing
    // notification with the same id (shouldn't normally happen).
    Notifications::iterator iter = GetNotification(notification.id);
    if (iter != notifications_.end())
      EraseNotification(iter);
    // Add the notification to the front (top) of the list.
    if (!is_visible_)
      ++unread_count_;
    notifications_.push_front(notification);
  }

  Notifications notifications_;
  bool is_visible_;
  int unread_count_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationList);
};

// A dropdown menu for notifications.
class WebNotificationMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  explicit WebNotificationMenuModel(WebNotificationTray* tray,
                                    const WebNotification& notification)
      : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
        tray_(tray),
        notification_(notification) {
    // Add 'disable notifications' menu item.
    if (!notification.extension_id.empty()) {
      AddItem(kToggleExtensionCommand,
              GetLabelForCommandId(kToggleExtensionCommand));
    } else if (!notification.display_source.empty()) {
      AddItem(kTogglePermissionCommand,
              GetLabelForCommandId(kTogglePermissionCommand));
    }
    // Add settings menu item.
    if (!notification.display_source.empty()) {
      AddItem(kShowSettingsCommand,
              GetLabelForCommandId(kShowSettingsCommand));
    }
  }

  virtual ~WebNotificationMenuModel() {
  }

  // Overridden from ui::SimpleMenuModel:
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        return l10n_util::GetStringUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_EXTENSIONS_DISABLE);
      case kTogglePermissionCommand:
        return l10n_util::GetStringFUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_SITE_DISABLE,
            notification_.display_source);
      case kShowSettingsCommand:
        return l10n_util::GetStringUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_SETTINGS);
      default:
        NOTREACHED();
    }
    return string16();
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id) OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        tray_->DisableByExtension(notification_.id);
        break;
      case kTogglePermissionCommand:
        tray_->DisableByUrl(notification_.id);
        break;
      case kShowSettingsCommand:
        tray_->ShowSettings(notification_.id);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  WebNotificationTray* tray_;
  WebNotification notification_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationMenuModel);
};

// The view for a notification entry (icon + message + buttons).
class WebNotificationView : public views::View,
                            public views::ButtonListener,
                            public views::MenuButtonListener {
 public:
  WebNotificationView(WebNotificationTray* tray,
                      const WebNotification& notification)
      : tray_(tray),
        notification_(notification),
        icon_(NULL),
        menu_button_(NULL),
        close_button_(NULL) {
    InitView(tray, notification);
  }

  virtual ~WebNotificationView() {
  }

  void InitView(WebNotificationTray* tray,
                const WebNotification& notification) {
    set_border(views::Border::CreateSolidSidedBorder(
        1, 0, 0, 0, kBorderLightColor));
    SkColor bg_color = notification.is_read
        ? kHeaderBackgroundColorLight : kBackgroundColor;
    set_background(views::Background::CreateSolidBackground(bg_color));

    icon_ = new views::ImageView;
    icon_->SetImageSize(
        gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize));
    icon_->SetImage(notification.image);

    views::Label* title = new views::Label(notification.title);
    title->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
    views::Label* message = new views::Label(notification.message);
    message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    message->SetMultiLine(true);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(
        views::CustomButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_NOTIFY_CLOSE));
    close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                     views::ImageButton::ALIGN_MIDDLE);

    if (!notification.extension_id.empty() ||
        !notification.display_source.empty()) {
      menu_button_ = new views::MenuButton(NULL, string16(), this, true);
      menu_button_->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 2));
    }

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);

    const int padding_width = kTrayPopupPaddingHorizontal/2;
    columns->AddPaddingColumn(0, padding_width);

    // Notification Icon.
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kWebNotificationIconSize, kWebNotificationIconSize);

    columns->AddPaddingColumn(0, padding_width);

    // Notification message text.
    const int message_width = kWebNotificationWidth - kWebNotificationIconSize -
        kWebNotificationButtonWidth - (padding_width * 3);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                       100, /* resize percent */
                       views::GridLayout::FIXED, message_width, message_width);

    columns->AddPaddingColumn(0, padding_width);

    // Close and menu buttons.
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kWebNotificationButtonWidth,
                       kWebNotificationButtonWidth);

    // Layout rows
    layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);

    layout->StartRow(0, 0);
    layout->AddView(icon_, 1, 2);
    layout->AddView(title, 1, 1);
    layout->AddView(close_button_, 1, 1);

    layout->StartRow(0, 0);
    layout->SkipColumns(2);
    layout->AddView(message, 1, 1);
    if (menu_button_)
      layout->AddView(menu_button_, 1, 1);
    layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  }

  // views::View overrides.
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    tray_->OnClicked(notification_.id);
    return true;
  }

  virtual ui::GestureStatus OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE {
    if (event.type() != ui::ET_GESTURE_TAP)
      return ui::GESTURE_STATUS_UNKNOWN;
    tray_->OnClicked(notification_.id);
    return ui::GESTURE_STATUS_CONSUMED;
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_button_)
      tray_->SendRemoveNotification(notification_.id);
  }

  // Overridden from MenuButtonListener.
  virtual void OnMenuButtonClicked(
      View* source, const gfx::Point& point) OVERRIDE {
    if (source != menu_button_)
      return;
    WebNotificationMenuModel menu_model(tray_, notification_);
    views::MenuModelAdapter menu_model_adapter(&menu_model);
    views::MenuRunner menu_runner(menu_model_adapter.CreateMenu());

    gfx::Point screen_location;
    views::View::ConvertPointToScreen(menu_button_, &screen_location);
    ignore_result(menu_runner.RunMenuAt(
        source->GetWidget()->GetTopLevelWidget(),
        menu_button_,
        gfx::Rect(screen_location, menu_button_->size()),
        views::MenuItemView::TOPRIGHT,
        views::MenuRunner::HAS_MNEMONICS));
  }

 private:
  WebNotificationTray* tray_;
  WebNotification notification_;
  views::ImageView* icon_;
  views::MenuButton* menu_button_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationView);
};

// The view for the buttons at the bottom of the web notification tray.
class WebNotificationButtonView : public views::View,
                                  public views::ButtonListener {
 public:
  explicit WebNotificationButtonView(WebNotificationTray* tray)
      : tray_(tray),
        close_all_button_(NULL) {
    set_background(views::Background::CreateBackgroundPainter(
        true,
        views::Painter::CreateVerticalGradient(
            kHeaderBackgroundColorLight,
            kHeaderBackgroundColorDark)));
    set_border(views::Border::CreateSolidSidedBorder(
        2, 0, 0, 0, ash::kBorderDarkColor));

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddPaddingColumn(100, 0);
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::USE_PREF, 0, 0);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_all_button_ = new TrayPopupTextButton(
        this, rb.GetLocalizedString(IDS_ASH_WEB_NOTFICATION_TRAY_CLEAR_ALL));

    layout->StartRow(0, 0);
    layout->AddView(close_all_button_);
  }

  virtual ~WebNotificationButtonView() {
  }

  void SetCloseAllVisible(bool visible) {
    close_all_button_->SetVisible(visible);
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_all_button_)
      tray_->SendRemoveAllNotifications();
  }

 private:
  WebNotificationTray* tray_;
  TrayPopupTextButton* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};

class WebContentsView : public views::View {
 public:
  explicit WebContentsView(WebNotificationTray* tray)
      : tray_(tray) {
    // TODO(stevenjb): Remove this border when TrayBubbleBorder is integrated
    // with BubbleBorder.
    int left = (tray->shelf_alignment() == SHELF_ALIGNMENT_LEFT) ? 0 : 1;
    int right = (tray->shelf_alignment() == SHELF_ALIGNMENT_RIGHT) ? 0 : 1;
    int bottom = (tray->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) ? 0 : 1;
    set_border(views::Border::CreateSolidSidedBorder(
        1, left, bottom, right, ash::kBorderDarkColor));
    set_notify_enter_exit_on_child(true);
  }
  virtual ~WebContentsView() {}

  virtual void Update(
      const WebNotificationList::Notifications& notifications) = 0;

 protected:
  WebNotificationTray* tray_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsView);
};

class MessageCenterContentsView : public WebContentsView {
 public:
  class ScrollContentView : public views::View {
   public:
    ScrollContentView() {
      views::BoxLayout* layout =
          new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
      layout->set_spread_blank_space(true);
      SetLayoutManager(layout);
    }

    virtual ~ScrollContentView() {};

    virtual gfx::Size GetPreferredSize() OVERRIDE {
      if (!preferred_size_.IsEmpty())
        return preferred_size_;
      return views::View::GetPreferredSize();
    }

    void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }

   private:
    gfx::Size preferred_size_;
    DISALLOW_COPY_AND_ASSIGN(ScrollContentView);
  };

  explicit MessageCenterContentsView(WebNotificationTray* tray)
      : WebContentsView(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    scroll_content_ = new ScrollContentView;
    scroller_ = new internal::FixedSizedScrollView;
    scroller_->SetContentsView(scroll_content_);
    AddChildView(scroller_);

    button_view_ = new internal::WebNotificationButtonView(tray);
    AddChildView(button_view_);

    // Build initial view with no notifications.
    Update(WebNotificationList::Notifications());
  }

  void Update(const WebNotificationList::Notifications& notifications) {
    scroll_content_->RemoveAllChildViews(true);
    scroll_content_->set_preferred_size(gfx::Size());
    int num_children = 0;
    for (WebNotificationList::Notifications::const_iterator iter =
             notifications.begin(); iter != notifications.end(); ++iter) {
      WebNotificationView* view = new WebNotificationView(tray_, *iter);
      scroll_content_->AddChildView(view);
      if (++num_children >= kMaxVisibleNotifications)
        break;
    }
    if (num_children == 0) {
      views::Label* label = new views::Label(l10n_util::GetStringUTF16(
          IDS_ASH_WEB_NOTFICATION_TRAY_NO_MESSAGES));
      label->SetFont(label->font().DeriveFont(1));
      label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
      label->SetEnabledColor(SK_ColorGRAY);
      scroll_content_->AddChildView(label);
      button_view_->SetCloseAllVisible(false);
    } else {
      button_view_->SetCloseAllVisible(true);
    }
    SizeScrollContent();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

 private:
  void SizeScrollContent() {
    gfx::Size scroll_size = scroll_content_->GetPreferredSize();
    const int button_height = button_view_->GetPreferredSize().height();
    const int min_height = kWebNotificationBubbleMinHeight - button_height;
    const int max_height = kWebNotificationBubbleMaxHeight - button_height;
    int scroll_height = std::min(std::max(
        scroll_size.height(), min_height), max_height);
    scroll_size.set_height(scroll_height);
    if (scroll_height == min_height)
      scroll_content_->set_preferred_size(scroll_size);
    else
      scroll_content_->set_preferred_size(gfx::Size());
    scroller_->SetFixedSize(scroll_size);
    scroller_->SizeToPreferredSize();
  }

  internal::FixedSizedScrollView* scroller_;
  ScrollContentView* scroll_content_;
  internal::WebNotificationButtonView* button_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterContentsView);
};

class WebNotificationContentsView : public WebContentsView {
 public:
  explicit WebNotificationContentsView(WebNotificationTray* tray)
      : WebContentsView(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    content_ = new views::View;
    content_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    AddChildView(content_);

    // Build initial view with no notification.
    Update(WebNotificationList::Notifications());
  }

  void Update(const WebNotificationList::Notifications& notifications) {
    content_->RemoveAllChildViews(true);
    const WebNotification& notification = (notifications.size() > 0) ?
        notifications.front() : WebNotification();
    WebNotificationView* view = new WebNotificationView(tray_, notification);
    content_->AddChildView(view);
    content_->SizeToPreferredSize();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

 private:
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationContentsView);
};

}  // namespace internal

using internal::TrayBubbleView;
using internal::WebNotificationList;
using internal::WebContentsView;

class WebNotificationTray::Bubble : public TrayBubbleView::Host,
                                    public views::WidgetObserver {
 public:
  enum BubbleType {
    BUBBLE_TYPE_MESAGE_CENTER,
    BUBBLE_TYPE_NOTIFICATION
  };

  Bubble(WebNotificationTray* tray, BubbleType bubble_type)
      : tray_(tray),
        bubble_type_(bubble_type),
        bubble_view_(NULL),
        bubble_widget_(NULL),
        contents_view_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
    views::View* anchor = tray->tray_container();
    TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                           tray->shelf_alignment());
    init_params.bubble_width = kWebNotificationWidth;
    if (bubble_type == BUBBLE_TYPE_MESAGE_CENTER) {
      init_params.max_height = kWebNotificationBubbleMaxHeight;
    } else {
      init_params.arrow_color = kBackgroundColor;
      init_params.close_on_deactivate = false;
    }
    if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      gfx::Point bounds(anchor->width() / 2, 0);
      ConvertPointToWidget(anchor, &bounds);
      init_params.arrow_offset = bounds.x();
    }
    bubble_view_ = TrayBubbleView::Create(anchor, this, init_params);

    if (bubble_type == BUBBLE_TYPE_MESAGE_CENTER)
      contents_view_ = new internal::MessageCenterContentsView(tray);
    else
      contents_view_ = new internal::WebNotificationContentsView(tray);
    bubble_view_->AddChildView(contents_view_);

    bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
    bubble_widget_->AddObserver(this);

    InitializeAndShowBubble(bubble_widget_, bubble_view_, tray_);

    ScheduleUpdate();
  }

  virtual ~Bubble() {
    if (bubble_view_)
      bubble_view_->reset_host();
    if (bubble_widget_) {
      bubble_widget_->RemoveObserver(this);
      bubble_widget_->Close();
    }
  }

  void ScheduleUpdate() {
    StartAutoCloseTimer();

    weak_ptr_factory_.InvalidateWeakPtrs();  // Cancel any pending update.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WebNotificationTray::Bubble::UpdateBubbleView,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kUpdateDelayMs));
  }

  views::Widget* bubble_widget() const { return bubble_widget_; }
  TrayBubbleView* bubble_view() const { return bubble_view_; }

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE {
    bubble_view_ = NULL;
    contents_view_ = NULL;
  }

  virtual void OnMouseEnteredView() OVERRIDE {
    StopAutoCloseTimer();
  }

  virtual void OnMouseExitedView() OVERRIDE {
    StartAutoCloseTimer();
  }

  virtual void OnClickedOutsideView() OVERRIDE {
    // May delete |this|.
    tray_->HideMessageCenterBubble();
  }

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE {
    CHECK_EQ(bubble_widget_, widget);
    bubble_widget_ = NULL;
    tray_->HideBubble(this);  // Will destroy |this|.
  }

 private:
  void UpdateBubbleView() {
    const WebNotificationList::Notifications& notifications =
        tray_->notification_list()->notifications();
    contents_view_->Update(notifications);
    bubble_view_->Show();
    bubble_view_->UpdateBubble();
  }

  void StartAutoCloseTimer() {
    if (bubble_type_ != BUBBLE_TYPE_NOTIFICATION)
      return;
    autoclose_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(kAutocloseDelaySeconds),
                     this, &Bubble::OnAutoClose);
  }

  void StopAutoCloseTimer() {
    autoclose_.Stop();
  }

  void OnAutoClose() {
    tray_->HideBubble(this);  // deletes |this|!
  }

  WebNotificationTray* tray_;
  BubbleType bubble_type_;
  TrayBubbleView* bubble_view_;
  views::Widget* bubble_widget_;
  WebContentsView* contents_view_;
  base::OneShotTimer<Bubble> autoclose_;
  base::WeakPtrFactory<Bubble> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Bubble);
};

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : internal::TrayBackgroundView(status_area_widget),
      notification_list_(new WebNotificationList()),
      count_label_(NULL),
      delegate_(NULL),
      show_message_center_on_unlock_(false) {
  count_label_ = new views::Label(UTF8ToUTF16("0"));
  internal::SetupLabelForTray(count_label_);
  gfx::Font font = count_label_->font();
  count_label_->SetFont(font.DeriveFont(0, font.GetStyle() & ~gfx::Font::BOLD));
  count_label_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  count_label_->SetEnabledColor(kMessageCountDimmedColor);

  tray_container()->set_size(gfx::Size(kTrayWidth, kTrayHeight));
  tray_container()->AddChildView(count_label_);

  UpdateTray();
}

WebNotificationTray::~WebNotificationTray() {
}

void WebNotificationTray::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

// Add/Update/RemoveNotification are called by the client code, i.e the
// Delegate implementation or its proxy.

void WebNotificationTray::AddNotification(const std::string& id,
                                          const string16& title,
                                          const string16& message,
                                          const string16& display_source,
                                          const std::string& extension_id) {
  notification_list_->AddNotification(
      id, title, message, display_source, extension_id);
  UpdateTrayAndBubble();
  ShowNotificationBubble();
}

void WebNotificationTray::UpdateNotification(const std::string& old_id,
                                             const std::string& new_id,
                                             const string16& title,
                                             const string16& message) {
  notification_list_->UpdateNotificationMessage(old_id, new_id, title, message);
  UpdateTrayAndBubble();
  ShowNotificationBubble();
}

void WebNotificationTray::RemoveNotification(const std::string& id) {
  if (id == notification_list_->GetFirstId())
    HideNotificationBubble();
  if (!notification_list_->RemoveNotification(id))
    return;
  UpdateTrayAndBubble();
}

void WebNotificationTray::SetNotificationImage(const std::string& id,
                                               const gfx::ImageSkia& image) {
  if (!notification_list_->SetNotificationImage(id, image))
    return;
  UpdateTrayAndBubble();
  if (notification_bubble() && id == notification_list_->GetFirstId())
    ShowNotificationBubble();
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (message_center_bubble()) {
    UpdateTray();
    return;
  }
  notification_list_->SetIsVisible(true);  // clears notification count
  UpdateTray();
  HideNotificationBubble();
  message_center_bubble_.reset(
      new Bubble(this, Bubble::BUBBLE_TYPE_MESAGE_CENTER));
  status_area_widget()->SetHideSystemNotifications(true);
}

void WebNotificationTray::HideMessageCenterBubble() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  show_message_center_on_unlock_ = false;
  notification_list_->SetIsVisible(false);
  status_area_widget()->SetHideSystemNotifications(false);
}

void WebNotificationTray::ShowNotificationBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (message_center_bubble())
    return;
  if (!status_area_widget()->ShouldShowNonSystemNotifications())
    return;
  UpdateTray();
  if (notification_bubble()) {
    notification_bubble()->ScheduleUpdate();
  } else {
    notification_bubble_.reset(
        new Bubble(this, Bubble::BUBBLE_TYPE_NOTIFICATION));
  }
}

void WebNotificationTray::HideNotificationBubble() {
  notification_bubble_.reset();
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (login_status == user::LOGGED_IN_LOCKED) {
    if (message_center_bubble()) {
      message_center_bubble_.reset();
      show_message_center_on_unlock_ = true;
    }
    HideNotificationBubble();
  } else {
    if (show_message_center_on_unlock_)
      ShowMessageCenterBubble();
    show_message_center_on_unlock_ = false;
  }
  UpdateTray();
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  if (alignment == SHELF_ALIGNMENT_BOTTOM)
    tray_container()->set_size(gfx::Size(kTrayWidth, kTrayHeight));
  else
    tray_container()->set_size(gfx::Size(kTraySideWidth, kTraySideHeight));
  // Destroy any existing bubble so that it will be rebuilt correctly.
  HideMessageCenterBubble();
  HideNotificationBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (notification_bubble_.get()) {
    notification_bubble_->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the launcher/status area.
    notification_bubble_->bubble_view()->GetWidget()->StackAtTop();
  }
  if (message_center_bubble_.get())
    message_center_bubble_->bubble_view()->UpdateBubble();
}

// Protected methods (invoked only from Bubble and its child classes)

void WebNotificationTray::SendRemoveNotification(const std::string& id) {
  // If this is the only notification in the list, close the bubble.
  if (notification_list_->notifications().size() == 1 &&
      id == notification_list_->GetFirstId()) {
    HideMessageCenterBubble();
  }
  if (delegate_)
    delegate_->NotificationRemoved(id);
}

void WebNotificationTray::SendRemoveAllNotifications() {
  HideMessageCenterBubble();
  if (delegate_) {
    const WebNotificationList::Notifications& notifications =
        notification_list_->notifications();
    for (WebNotificationList::Notifications::const_iterator loopiter =
             notifications.begin();
         loopiter != notifications.end(); ) {
      WebNotificationList::Notifications::const_iterator curiter = loopiter++;
      std::string notification_id = curiter->id;
      // May call RemoveNotification and erase curiter.
      delegate_->NotificationRemoved(notification_id);
    }
  }
}

// When we disable notifications, we remove any existing matching
// notifications to avoid adding complicated UI to re-enable the source.
void WebNotificationTray::DisableByExtension(const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsByExtension(this, id);
}

void WebNotificationTray::DisableByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsBySource(this, id);
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (message_center_bubble())
    HideMessageCenterBubble();
  else
    ShowMessageCenterBubble();
  return true;
}

int WebNotificationTray::GetNotificationCount() const {
  return notification_list()->notifications().size();
}

void WebNotificationTray::ShowSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void WebNotificationTray::OnClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
}

// Private methods

void WebNotificationTray::UpdateTray() {
  count_label_->SetText(UTF8ToUTF16(
      GetNotificationText(notification_list()->unread_count())));
  // Dim the message count text only if the message center is empty.
  count_label_->SetEnabledColor(
      (notification_list()->notifications().size() == 0) ?
      kMessageCountDimmedColor : kMessageCountColor);
  bool is_visible =
      (status_area_widget()->login_status() != user::LOGGED_IN_NONE) &&
      (status_area_widget()->login_status() != user::LOGGED_IN_LOCKED);
  SetVisible(is_visible);
  Layout();
  SchedulePaint();
}

void WebNotificationTray::UpdateTrayAndBubble() {
  UpdateTray();

  if (message_center_bubble())
    message_center_bubble()->ScheduleUpdate();

  if (notification_bubble()) {
    if (GetNotificationCount() == 0)
      HideNotificationBubble();
    else
      notification_bubble()->ScheduleUpdate();
  }
}

void WebNotificationTray::HideBubble(Bubble* bubble) {
  if (bubble == message_center_bubble()) {
    HideMessageCenterBubble();
  } else if (bubble == notification_bubble()) {
    HideNotificationBubble();
  }
}

bool WebNotificationTray::HasNotificationForTest(const std::string& id) const {
  return notification_list_->HasNotification(id);
}

}  // namespace ash
