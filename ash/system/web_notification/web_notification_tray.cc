// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "grit/ui_resources_standard.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
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

namespace {

const int kTrayBorder = 4;
const int kNotificationIconWidth = 40;
const int kNotificationIconHeight = 25;
const int kWebNotificationBubbleMinHeight = 80;
const int kWebNotificationBubbleMaxHeight = 400;
const int kWebNotificationWidth = 400;
const int kWebNotificationButtonWidth = 32;

const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kShowSettingsCommand = 2;

// The image has three icons: 1 notifiaction, 2 notifications, and 3+.
SkBitmap GetNotificationImage(int notification_count) {
  SkBitmap image;
  gfx::Image all = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_UBER_TRAY_WEB_NOTIFICATON);
  int image_index = notification_count - 1;
  image_index = std::max(0, std::min(image_index, 2));
  SkIRect region = SkIRect::MakeXYWH(
      0, image_index * kNotificationIconHeight,
      kNotificationIconWidth, kNotificationIconHeight);
  all.ToSkBitmap()->extractSubset(&image, region);
  return image;
}

}  // namespace

namespace ash {

namespace internal {

struct WebNotification {
  WebNotification(const std::string& i,
                  const string16& t,
                  const string16& m,
                  const string16& s,
                  const std::string& e)
      : id(i),
        title(t),
        message(m),
        display_source(s),
        extension_id(e) {
  }

  std::string id;
  string16 title;
  string16 message;
  string16 display_source;
  std::string extension_id;
  gfx::ImageSkia image;
};

// A helper class to manage the list of notifications.
class WebNotificationList {
 public:
  typedef std::list<WebNotification> Notifications;

  WebNotificationList() {
  }

  void AddNotification(const std::string& id,
                       const string16& title,
                       const string16& message,
                       const string16& display_source,
                       const std::string& extension_id) {
    Notifications::iterator iter = GetNotification(id);
    if (iter != notifications_.end()) {
      // Update existing notification.
      iter->title = title;
      iter->message = message;
      iter->display_source = display_source;
      iter->extension_id = extension_id;
    } else {
      notifications_.push_back(
          WebNotification(id, title, message, display_source, extension_id));
    }
  }

  void UpdateNotificationMessage(const std::string& id,
                                 const string16& title,
                                 const string16& message) {
    Notifications::iterator iter = GetNotification(id);
    if (iter == notifications_.end())
      return;
    iter->title = title;
    iter->message = message;
  }

  bool RemoveNotification(const std::string& id) {
    Notifications::iterator iter = GetNotification(id);
    if (iter == notifications_.end())
      return false;
    notifications_.erase(iter);
    return true;
  }

  void RemoveAllNotifications() {
    notifications_.clear();
  }

  void RemoveNotificationsBySource(const std::string& id) {
    Notifications::iterator source_iter = GetNotification(id);
    if (source_iter == notifications_.end())
      return;
    string16 display_source = source_iter->display_source;
    for (Notifications::iterator loopiter = notifications_.begin();
         loopiter != notifications_.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->display_source == display_source)
        notifications_.erase(curiter);
    }
  }

  void RemoveNotificationsByExtension(const std::string& id) {
    Notifications::iterator source_iter = GetNotification(id);
    if (source_iter == notifications_.end())
      return;
    std::string extension_id = source_iter->extension_id;
    for (Notifications::iterator loopiter = notifications_.begin();
         loopiter != notifications_.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->extension_id == extension_id)
        notifications_.erase(curiter);
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

  const Notifications& notifications() const { return notifications_; }

 private:
  Notifications::iterator GetNotification(const std::string& id) {
    for (Notifications::iterator iter = notifications_.begin();
         iter != notifications_.end(); ++iter) {
      if (iter->id == id)
        return iter;
    }
    return notifications_.end();
  }

  Notifications notifications_;

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
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    icon_ = new views::ImageView;
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
            IDR_AURA_WINDOW_CLOSE));

    if (!notification.extension_id.empty() ||
        !notification.display_source.empty()) {
      menu_button_ = new views::MenuButton(NULL, string16(), this, true);
      menu_button_->set_border(NULL);
    }

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

    // Notification Icon.
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kNotificationIconWidth, kNotificationIconWidth);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

    // Notification message text.
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                       100, /* resize percent */
                       views::GridLayout::USE_PREF, 0, 0);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

    // Close and menu buttons.
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kWebNotificationButtonWidth,
                       kWebNotificationButtonWidth);

    columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal/2);

    // Layout rows
    layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);

    layout->StartRow(0, 0);
    layout->AddView(icon_, 1, 2);
    layout->AddView(title, 1, 1);
    layout->AddView(close_button_, 1, 1);

    layout->StartRow(0, 0);
    layout->SkipColumns(2);
    layout->AddView(message, 1, 1);
    if (menu_button_) {
      layout->AddView(menu_button_, 1, 1,
                      views::GridLayout::CENTER, views::GridLayout::LEADING);
    }
    layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  }

  // view::Views overrides.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    tray_->OnClicked(notification_.id);
    return true;
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    if (sender == close_button_)
      tray_->RemoveNotification(notification_.id);
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
class WebNotificationButtonView : public TrayPopupTextButtonContainer,
                                  public views::ButtonListener {
 public:
  explicit WebNotificationButtonView(WebNotificationTray* tray)
      : tray_(tray),
        settings_button_(NULL),
        close_all_button_(NULL) {
    set_background(views::Background::CreateBackgroundPainter(
        true,
        views::Painter::CreateVerticalGradient(
            kHeaderBackgroundColorLight,
            kHeaderBackgroundColorDark)));
    set_border(views::Border::CreateSolidSidedBorder(
        2, 0, 0, 0, ash::kBorderDarkColor));

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    settings_button_ = new TrayPopupTextButton(
        this, rb.GetLocalizedString(IDS_ASH_WEB_NOTFICATION_TRAY_SETTINGS));
    AddTextButton(settings_button_);

    close_all_button_ = new TrayPopupTextButton(
        this, rb.GetLocalizedString(IDS_ASH_WEB_NOTFICATION_TRAY_CLOSE_ALL));
    AddTextButton(close_all_button_);
  }

  virtual ~WebNotificationButtonView() {
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    if (sender == settings_button_)
      tray_->ShowSettings("");
    else if (sender == close_all_button_)
      tray_->RemoveAllNotifications();
  }

 private:
  WebNotificationTray* tray_;
  TrayPopupTextButton* settings_button_;
  TrayPopupTextButton* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};

}  // namespace internal

using internal::WebNotificationList;
using internal::WebNotificationView;

class WebNotificationTray::BubbleContentsView : public views::View {
 public:
  explicit BubbleContentsView(WebNotificationTray* tray)
      : tray_(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    scroll_content_ = new views::View;
    scroll_content_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    scroller_ = new internal::FixedSizedScrollView;
    scroller_->SetContentsView(scroll_content_);
    AddChildView(scroller_);

    button_view_ = new internal::WebNotificationButtonView(tray);
    AddChildView(button_view_);
}

  void Update(const WebNotificationList::Notifications& notifications) {
    scroll_content_->RemoveAllChildViews(true);
    for (WebNotificationList::Notifications::const_iterator iter =
             notifications.begin(); iter != notifications.end(); ++iter) {
      WebNotificationView* view = new WebNotificationView(tray_, *iter);
      scroll_content_->AddChildView(view);
    }
    PreferredSizeChanged();
    GetWidget()->GetRootView()->Layout();
    GetWidget()->GetRootView()->SchedulePaint();
  }

  // Overridden from view::View:
  virtual void Layout() OVERRIDE {
    SizeScrollContent();
    views::View::Layout();
    PreferredSizeChanged();
  }

 private:
  void SizeScrollContent() {
    gfx::Size scroll_size = scroll_content_->GetPreferredSize();
    int button_height = button_view_->GetPreferredSize().height();
    int scroll_height = std::min(
        std::max(scroll_size.height(),
                 kWebNotificationBubbleMinHeight - button_height),
        kWebNotificationBubbleMaxHeight - button_height);
    if (scroll_height < scroll_size.height()) {
      scroll_size.set_height(scroll_height);
      scroller_->SetFixedSize(scroll_size);
    } else {
      scroller_->SetFixedSize(gfx::Size());
    }
  }

  WebNotificationTray* tray_;
  internal::FixedSizedScrollView* scroller_;
  views::View* scroll_content_;
  internal::WebNotificationButtonView* button_view_;

  DISALLOW_COPY_AND_ASSIGN(BubbleContentsView);
};

class WebNotificationTray::Bubble : public internal::TrayBubbleView::Host,
                                    public views::Widget::Observer {
 public:
  explicit Bubble(WebNotificationTray* tray)
      : tray_(tray),
        bubble_view_(NULL),
        bubble_widget_(NULL),
        contents_view_(NULL) {
    views::View* anchor = tray->tray_container();
    views::BubbleBorder::ArrowLocation arrow_location;
    int arrow_offset = 0;
    if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      arrow_location = views::BubbleBorder::BOTTOM_RIGHT;
      arrow_offset = anchor->GetContentsBounds().width() / 2;
    } else if (tray_->shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
      arrow_location = views::BubbleBorder::LEFT_BOTTOM;
    } else {
      arrow_location = views::BubbleBorder::RIGHT_BOTTOM;
    }
    bubble_view_ = new internal::TrayBubbleView(
        anchor, arrow_location, this, false, kWebNotificationWidth);
    bubble_view_->SetMaxHeight(kWebNotificationBubbleMaxHeight);

    bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);

    bubble_view_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
    bubble_widget_->non_client_view()->frame_view()->set_background(NULL);
    bubble_view_->SetBubbleBorder(arrow_offset);

    bubble_widget_->AddObserver(this);

    contents_view_ = new BubbleContentsView(tray);
    bubble_view_->AddChildView(contents_view_);

    InitializeHost(bubble_widget_, tray_);

    Update();
    bubble_view_->Show();
  }

  virtual ~Bubble() {
    if (bubble_view_)
      bubble_view_->reset_host();
    if (bubble_widget_) {
      bubble_widget_->RemoveObserver(this);
      bubble_widget_->Close();
    }
  }

  void Update() {
    contents_view_->Update(tray_->notification_list()->notifications());
  }

  views::Widget* bubble_widget() const { return bubble_widget_; }

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE {
    bubble_view_ = NULL;
    contents_view_ = NULL;
  }

  virtual gfx::Rect GetAnchorRect() const OVERRIDE {
    gfx::Rect anchor_rect = tray_->tray_container()->GetScreenBounds();
    return anchor_rect;
  }

  virtual void OnMouseEnteredView() OVERRIDE {
  }

  virtual void OnMouseExitedView() OVERRIDE {
  }

  virtual void OnClickedOutsideView() OVERRIDE {
    // May delete |this|.
    tray_->status_area_widget()->HideWebNotificationBubble();
  }

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE {
    CHECK_EQ(bubble_widget_, widget);
    bubble_widget_ = NULL;
    tray_->HideBubble();  // Will destroy |this|.
  }

 private:
  WebNotificationTray* tray_;
  internal::TrayBubbleView* bubble_view_;
  views::Widget* bubble_widget_;
  BubbleContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(Bubble);
};

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : status_area_widget_(status_area_widget),
      notification_list_(new WebNotificationList()),
      tray_container_(NULL),
      icon_(NULL),
      delegate_(NULL) {
  tray_container_ = new views::View;
  tray_container_->set_border(views::Border::CreateEmptyBorder(
      kTrayBorder, kTrayBorder, kTrayBorder, kTrayBorder));
  SetShelfAlignment(shelf_alignment());

  icon_ = new views::ImageView;
  tray_container_->AddChildView(icon_);
  UpdateIcon();  // Hides the tray initially.

  SetContents(tray_container_);
}

WebNotificationTray::~WebNotificationTray() {
}

void WebNotificationTray::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void WebNotificationTray::AddNotification(const std::string& id,
                                          const string16& title,
                                          const string16& message,
                                          const string16& display_source,
                                          const std::string& extension_id) {
  notification_list_->AddNotification(
      id, title, message, display_source, extension_id);
  UpdateIcon();
  if (bubble()) {
    bubble_->Update();
  } else {
    status_area_widget_->ShowWebNotificationBubble(
        internal::StatusAreaWidget::NON_USER_ACTION);
  }
}

void WebNotificationTray::UpdateNotification(const std::string& id,
                                             const string16& title,
                                             const string16& message) {
  notification_list_->UpdateNotificationMessage(id, title, message);
  if (bubble())
    bubble_->Update();
}

void WebNotificationTray::RemoveNotification(const std::string& id) {
  if (!notification_list_->RemoveNotification(id))
    return;
  if (delegate_)
    delegate_->NotificationRemoved(id);
  UpdateBubbleAndIcon();
}

void WebNotificationTray::RemoveAllNotifications() {
  const WebNotificationList::Notifications& notifications =
      notification_list_->notifications();
  if (delegate_) {
    for (WebNotificationList::Notifications::const_iterator loopiter =
             notifications.begin();
         loopiter != notifications.end(); ) {
      WebNotificationList::Notifications::const_iterator curiter = loopiter++;
      std::string notification_id = curiter->id;
      // May call RemoveNotification and erase curiter.
      delegate_->NotificationRemoved(notification_id);
    }
  }
  notification_list_->RemoveAllNotifications();
  UpdateBubbleAndIcon();
}

void WebNotificationTray::SetNotificationImage(const std::string& id,
                                               const gfx::ImageSkia& image) {
  if (!notification_list_->SetNotificationImage(id, image))
    return;
  if (bubble())
    bubble_->Update();
}

void WebNotificationTray::DisableByExtension(const std::string& id) {
  // When we disable notifications, we remove any existing matching
  // notifications to avoid adding complicated UI to re-enable the source.
  notification_list_->RemoveNotificationsByExtension(id);
  UpdateBubbleAndIcon();
  if (delegate_)
    delegate_->DisableExtension(id);
}

void WebNotificationTray::DisableByUrl(const std::string& id) {
  // See comment for DisableByExtension.
  notification_list_->RemoveNotificationsBySource(id);
  UpdateBubbleAndIcon();
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
}

void WebNotificationTray::ShowBubble() {
  if (bubble())
    return;
  bubble_.reset(new Bubble(this));
}

void WebNotificationTray::HideBubble() {
  bubble_.reset();
}

void WebNotificationTray::ShowSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void WebNotificationTray::OnClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  tray_container_->SetLayoutManager(new views::BoxLayout(
      alignment == SHELF_ALIGNMENT_BOTTOM ?
          views::BoxLayout::kHorizontal : views::BoxLayout::kVertical,
      0, 0, 0));
}

bool WebNotificationTray::PerformAction(const views::Event& event) {
  if (bubble()) {
    status_area_widget_->HideWebNotificationBubble();
  } else {
    status_area_widget_->ShowWebNotificationBubble(
        internal::StatusAreaWidget::USER_ACTION);
  }
  return true;
}

int WebNotificationTray::GetNotificationCount() const {
  return notification_list()->notifications().size();
}

void WebNotificationTray::UpdateIcon() {
  int count = GetNotificationCount();
  if (count == 0) {
    SetVisible(false);
  } else {
    icon_->SetImage(gfx::ImageSkia(GetNotificationImage(count)));
    SetVisible(true);
  }
  PreferredSizeChanged();
}

void WebNotificationTray::UpdateBubbleAndIcon() {
  UpdateIcon();
  if (!bubble())
    return;
  if (GetNotificationCount() == 0)
    status_area_widget_->HideWebNotificationBubble();
  else
    bubble_->Update();
}

}  // namespace ash
