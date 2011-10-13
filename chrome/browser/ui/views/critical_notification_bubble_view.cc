// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/critical_notification_bubble_view.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/upgrade_detector.h"
#include "content/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

namespace {

// Layout constants.
const int kPaddingAfterImage = 5;
const int kPaddingAboveButtonRow = 4;
const int kPaddingBetweenButtons = 7;

// How long to give the user until auto-restart if no action is taken. The code
// assumes this to be less than a minute.
const int kCountdownDuration = 30;  // Seconds.

// How often to refresh the bubble UI to update the counter. As long as the
// countdown is in seconds, this should be 1000 or lower.
const int kRefreshBubbleEvery = 1000;  // Millisecond.

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CriticalNotificationBubbleView

Bubble* CriticalNotificationBubbleView::bubble_ = NULL;

CriticalNotificationBubbleView::CriticalNotificationBubbleView() {
  bubble_created_ = base::Time::Now();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  image_ = new views::ImageView();
  image_->SetImage(ResourceBundle::GetSharedInstance().
       GetBitmapNamed(IDR_UPDATE_MENU3));
  AddChildView(image_);

  headline_ = new views::Label();
  headline_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  UpdateBubbleHeadline(GetRemainingTime());
  AddChildView(headline_);

  message_ = new views::Label();
  message_->SetMultiLine(true);
  message_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  message_->SetText(UTF16ToWide(
      l10n_util::GetStringFUTF16(IDS_CRITICAL_NOTIFICATION_TEXT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));
  AddChildView(message_);

  restart_button_ = new views::NativeTextButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_CRITICAL_NOTIFICATION_RESTART)));
  restart_button_->SetIsDefault(true);
  AddChildView(restart_button_);
  dismiss_button_ = new views::NativeTextButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_DISMISS)));
  AddChildView(dismiss_button_);

  if (bubble_)
    bubble_->Close();

  refresh_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kRefreshBubbleEvery),
      this, &CriticalNotificationBubbleView::OnCountdown);

  UserMetrics::RecordAction(UserMetricsAction("CriticalNotificationShown"));
}

CriticalNotificationBubbleView::~CriticalNotificationBubbleView() {
}

int CriticalNotificationBubbleView::GetRemainingTime() {
  base::TimeDelta time_lapsed = base::Time::Now() - bubble_created_;
  return kCountdownDuration - time_lapsed.InSeconds();
}

void CriticalNotificationBubbleView::UpdateBubbleHeadline(int seconds) {
  headline_->SetText(UTF16ToWide(
      l10n_util::GetStringFUTF16(IDS_CRITICAL_NOTIFICATION_HEADLINE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          base::IntToString16(seconds))));
}

void CriticalNotificationBubbleView::OnCountdown() {
  int seconds = GetRemainingTime();
  if (seconds <= 0) {
    // Time's up!
    UserMetrics::RecordAction(
        UserMetricsAction("CriticalNotification_AutoRestart"));
    refresh_timer_.Stop();
    BrowserList::AttemptRestart();
  } else {
    // Update the counter.
    UpdateBubbleHeadline(seconds);
    // TODO(msw): figure out why SchedulePaint doesn't work here.
#if !defined(USE_AURA)
    bubble_->ScheduleDraw();
#endif
  }
}

gfx::Size CriticalNotificationBubbleView::GetPreferredSize() {
  int width = views::Widget::GetLocalizedContentsWidth(
      IDS_CRUCIAL_NOTIFICATION_BUBBLE_WIDTH_CHARS);
  gfx::Size size(width, 0);

  gfx::Size headline = headline_->GetPreferredSize();
  int height = message_->GetHeightForWidth(size.width());
  gfx::Size button_row = dismiss_button_->GetPreferredSize();

  size.set_height(headline.height() +
                  height +
                  kPaddingAboveButtonRow +
                  button_row.height());
  return size;
}

void CriticalNotificationBubbleView::Layout() {
  gfx::Size bubble_size = GetPreferredSize();

  gfx::Size sz = image_->GetPreferredSize();
  image_->SetBounds(0, 0, sz.width(), sz.height());

  sz = headline_->GetPreferredSize();
  headline_->SetBounds(image_->width() + kPaddingAfterImage,
                       (image_->height() - sz.height()) / 2,
                       sz.width(), sz.height());

  sz = message_->GetPreferredSize();
  int height = message_->GetHeightForWidth(bubble_size.width());
  message_->SetBounds(0, headline_->height(), bubble_size.width(), height);

  sz = dismiss_button_->GetPreferredSize();
  dismiss_button_->SetBounds(bubble_size.width() - sz.width(),
                             message_->y() + message_->height() +
                                 kPaddingAboveButtonRow,
                             sz.width(),
                             sz.height());

  sz = restart_button_->GetPreferredSize();
  restart_button_->SetBounds(dismiss_button_->x() - sz.width() -
                                 kPaddingBetweenButtons,
                             dismiss_button_->y(),
                             sz.width(),
                             sz.height());
}

void CriticalNotificationBubbleView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  UpgradeDetector::GetInstance()->acknowledge_critical_update();

  if (sender == restart_button_) {
    UserMetrics::RecordAction(
        UserMetricsAction("CriticalNotification_Restart"));
    BrowserList::AttemptRestart();
  } else if (sender == dismiss_button_) {
    UserMetrics::RecordAction(UserMetricsAction("CriticalNotification_Ignore"));
    bubble_->Close();
  } else {
    NOTREACHED();
  }
}

void CriticalNotificationBubbleView::BubbleClosing(Bubble* bubble,
                                                   bool closed_by_escape) {
  bubble_ = NULL;

  refresh_timer_.Stop();

  if (closed_by_escape)
    UpgradeDetector::GetInstance()->acknowledge_critical_update();
}

bool CriticalNotificationBubbleView::CloseOnEscape() {
  return true;
}

bool CriticalNotificationBubbleView::FadeInOnShow() {
  return true;
}

string16 CriticalNotificationBubbleView::GetAccessibleName() {
  return ASCIIToWide("CriticalNotification");
}
