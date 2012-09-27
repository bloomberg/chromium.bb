// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/message_center_bubble.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/system/web_notification/web_notification_contents_view.h"
#include "ash/system/web_notification/web_notification_list.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/system/web_notification/web_notification_view.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

using internal::TrayBubbleView;
using internal::TrayPopupTextButton;

namespace message_center {

// Web Notification Bubble constants
const int kWebNotificationBubbleMinHeight = 80;
const int kWebNotificationBubbleMaxHeight = 400;

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

  virtual ~WebNotificationButtonView() {}

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
  internal::TrayPopupTextButton* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};


// Message Center contents.
class MessageCenterContentsView : public WebNotificationContentsView {
 public:
  class ScrollContentView : public views::View {
   public:
    ScrollContentView() {
      views::BoxLayout* layout =
          new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
      layout->set_spread_blank_space(true);
      SetLayoutManager(layout);
    }

    virtual ~ScrollContentView() {
    }

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
      : WebNotificationContentsView(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    scroll_content_ = new ScrollContentView;
    scroller_ = new internal::FixedSizedScrollView;
    scroller_->SetContentsView(scroll_content_);
    AddChildView(scroller_);

    scroller_->SetPaintToLayer(true);
    scroller_->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetMasksToBounds(true);

    button_view_ = new WebNotificationButtonView(tray);
    AddChildView(button_view_);
  }

  void Update(const WebNotificationList::Notifications& notifications)  {
    scroll_content_->RemoveAllChildViews(true);
    scroll_content_->set_preferred_size(gfx::Size());
    size_t num_children = 0;
    for (WebNotificationList::Notifications::const_iterator iter =
             notifications.begin(); iter != notifications.end(); ++iter) {
      WebNotificationView* view = new WebNotificationView(tray_, *iter);
      view->set_scroller(scroller_);
      scroll_content_->AddChildView(view);
      if (++num_children >= WebNotificationTray::kMaxVisibleTrayNotifications)
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

  size_t NumMessageViewsForTest() const {
    return scroll_content_->child_count();
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
  WebNotificationButtonView* button_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterContentsView);
};

// Message Center Bubble.
MessageCenterBubble::MessageCenterBubble(WebNotificationTray* tray) :
    WebNotificationBubble(tray),
    contents_view_(NULL) {
  TrayBubbleView::InitParams init_params = GetInitParams();
  init_params.max_height = message_center::kWebNotificationBubbleMaxHeight;
  init_params.can_activate = true;
  views::View* anchor = tray_->tray_container();
  bubble_view_ = TrayBubbleView::Create(anchor, this, init_params);
  contents_view_ = new MessageCenterContentsView(tray);

  Initialize(contents_view_);
}

MessageCenterBubble::~MessageCenterBubble() {}

size_t MessageCenterBubble::NumMessageViewsForTest() const {
  return contents_view_->NumMessageViewsForTest();
}

void MessageCenterBubble::BubbleViewDestroyed() {
  contents_view_ = NULL;
  WebNotificationBubble::BubbleViewDestroyed();
}

void MessageCenterBubble::UpdateBubbleView() {
  contents_view_->Update(tray_->notification_list()->notifications());
  bubble_view_->Show();
  bubble_view_->UpdateBubble();
}

void MessageCenterBubble::OnClickedOutsideView() {
  // May delete |this|.
  tray_->HideMessageCenterBubble();
}

}  // namespace message_center

}  // namespace ash
