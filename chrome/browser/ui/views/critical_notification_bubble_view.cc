// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/critical_notification_bubble_view.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using content::UserMetricsAction;

namespace {

// Layout constants.
const int kInset = 2;
const int kImageHeadlinePadding = 4;
const int kHeadlineMessagePadding = 4;
const int kMessageBubblePadding = 11;

// How long to give the user until auto-restart if no action is taken. The code
// assumes this to be less than a minute.
const int kCountdownDuration = 30;  // Seconds.

// How often to refresh the bubble UI to update the counter. As long as the
// countdown is in seconds, this should be 1000 or lower.
const int kRefreshBubbleEvery = 1000;  // Millisecond.

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CriticalNotificationBubbleView

CriticalNotificationBubbleView::CriticalNotificationBubbleView(
    views::View* anchor_view)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      headline_(NULL),
      restart_button_(NULL),
      dismiss_button_(NULL) {
  set_close_on_deactivate(false);
  set_move_with_anchor(true);
}

CriticalNotificationBubbleView::~CriticalNotificationBubbleView() {
}

int CriticalNotificationBubbleView::GetRemainingTime() {
  base::TimeDelta time_lapsed = base::Time::Now() - bubble_created_;
  return kCountdownDuration - time_lapsed.InSeconds();
}

void CriticalNotificationBubbleView::UpdateBubbleHeadline(int seconds) {
  if (seconds > 0) {
    headline_->SetText(
        l10n_util::GetStringFUTF16(IDS_CRITICAL_NOTIFICATION_HEADLINE,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
            base::IntToString16(seconds)));
  } else {
    headline_->SetText(
        l10n_util::GetStringFUTF16(IDS_CRITICAL_NOTIFICATION_HEADLINE_ALTERNATE,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  }
}

void CriticalNotificationBubbleView::OnCountdown() {
  UpgradeDetector* upgrade_detector = UpgradeDetector::GetInstance();
  if (upgrade_detector->critical_update_acknowledged()) {
    // The user has already interacted with the bubble and chosen a path.
    GetWidget()->Close();
    return;
  }

  int seconds = GetRemainingTime();
  if (seconds <= 0) {
    // Time's up!
    upgrade_detector->acknowledge_critical_update();

    content::RecordAction(
        UserMetricsAction("CriticalNotification_AutoRestart"));
    refresh_timer_.Stop();
    BrowserList::AttemptRestart();
  }

  // Update the counter. It may seem counter-intuitive to update the message
  // after we attempt restart, but remember that shutdown may be aborted by
  // an onbeforeunload handler, leaving the bubble up when the browser should
  // have restarted (giving the user another chance).
  UpdateBubbleHeadline(seconds);
  SchedulePaint();
}

void CriticalNotificationBubbleView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  // Let other bubbles know we have an answer from the user.
  UpgradeDetector::GetInstance()->acknowledge_critical_update();

  if (sender == restart_button_) {
    content::RecordAction(
        UserMetricsAction("CriticalNotification_Restart"));
    BrowserList::AttemptRestart();
  } else if (sender == dismiss_button_) {
    content::RecordAction(UserMetricsAction("CriticalNotification_Ignore"));
    // If the counter reaches 0, we set a restart flag that must be cleared if
    // the user selects, for example, "Stay on this page" during an
    // onbeforeunload handler.
    PrefService* prefs = g_browser_process->local_state();
    if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown))
      prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
  } else {
    NOTREACHED();
  }

  GetWidget()->Close();
}

void CriticalNotificationBubbleView::WindowClosing() {
  refresh_timer_.Stop();
}

void CriticalNotificationBubbleView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

void CriticalNotificationBubbleView::ViewHierarchyChanged(
    bool is_add, View* parent, View* child) {
  if (is_add && child == this) {
    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_ALERT, true);
  }
}

bool CriticalNotificationBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE)
    UpgradeDetector::GetInstance()->acknowledge_critical_update();
  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void CriticalNotificationBubbleView::Init() {
  bubble_created_ = base::Time::Now();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(0, kInset, kInset, kInset);
  SetLayoutManager(layout);

  const int top_column_set_id = 0;
  views::ColumnSet* top_columns = layout->AddColumnSet(top_column_set_id);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(0, kImageHeadlinePadding);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(1, 0);
  layout->StartRow(0, top_column_set_id);

  views::ImageView* image = new views::ImageView();
  image->SetImage(rb.GetImageSkiaNamed(IDR_UPDATE_MENU3));
  layout->AddView(image);

  headline_ = new views::Label();
  headline_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  UpdateBubbleHeadline(GetRemainingTime());
  layout->AddView(headline_);

  const int middle_column_set_id = 1;
  views::ColumnSet* middle_column = layout->AddColumnSet(middle_column_set_id);
  middle_column->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                           0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRowWithPadding(0, middle_column_set_id,
                              0, kHeadlineMessagePadding);

  views::Label* message = new views::Label();
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  message->SetText(l10n_util::GetStringFUTF16(IDS_CRITICAL_NOTIFICATION_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  message->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_CRUCIAL_NOTIFICATION_BUBBLE_WIDTH_CHARS));
  layout->AddView(message);

  const int bottom_column_set_id = 2;
  views::ColumnSet* bottom_columns = layout->AddColumnSet(bottom_column_set_id);
  bottom_columns->AddPaddingColumn(1, 0);
  bottom_columns->AddColumn(views::GridLayout::CENTER,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  bottom_columns->AddColumn(views::GridLayout::CENTER,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRowWithPadding(0, bottom_column_set_id,
                              0, kMessageBubblePadding);

  restart_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_RESTART));
  restart_button_->SetIsDefault(true);
  layout->AddView(restart_button_);
  dismiss_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_DISMISS));
  layout->AddView(dismiss_button_);

  refresh_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kRefreshBubbleEvery),
      this, &CriticalNotificationBubbleView::OnCountdown);

  content::RecordAction(UserMetricsAction("CriticalNotificationShown"));
}
