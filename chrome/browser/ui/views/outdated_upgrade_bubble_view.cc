// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"

#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/installer/util/google_update_util.h"
#endif

namespace {

// Fixed width of the column holding the description label of the bubble.
// TODO(mad): Make sure there is enough room for all languages.
const int kWidthOfDescriptionText = 330;

// We subtract 2 to account for the natural button padding, and
// to bring the separation visually in line with the row separation
// height.
const int kButtonPadding = views::kRelatedButtonHSpacing - 2;

// The URL to be used to re-install Chrome when auto-update failed for too long.
const char kDownloadChromeUrl[] = "https://www.google.com/chrome/?&brand=CHWL"
    "&utm_campaign=en&utm_source=en-et-na-us-chrome-bubble&utm_medium=et";

// The maximum number of ignored bubble we track in the NumLaterPerReinstall
// histogram.
const int kMaxIgnored = 50;
// The number of buckets we want the NumLaterPerReinstall histogram to use.
const int kNumIgnoredBuckets = 5;

}  // namespace

// OutdatedUpgradeBubbleView ---------------------------------------------------

OutdatedUpgradeBubbleView* OutdatedUpgradeBubbleView::upgrade_bubble_ = NULL;
int OutdatedUpgradeBubbleView::num_ignored_bubbles_ = 0;

// static
void OutdatedUpgradeBubbleView::ShowBubble(views::View* anchor_view,
                                           content::PageNavigator* navigator,
                                           bool auto_update_enabled) {
  if (IsShowing())
    return;
  upgrade_bubble_ = new OutdatedUpgradeBubbleView(
      anchor_view, navigator, auto_update_enabled);
  views::BubbleDelegateView::CreateBubble(upgrade_bubble_)->Show();
  content::RecordAction(base::UserMetricsAction(
      auto_update_enabled ? "OutdatedUpgradeBubble.Show"
                          : "OutdatedUpgradeBubble.ShowNoAU"));
}

bool OutdatedUpgradeBubbleView::IsAvailable() {
// This should only work on non-Chrome OS desktop platforms.
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  return true;
#else
  return false;
#endif
}

OutdatedUpgradeBubbleView::~OutdatedUpgradeBubbleView() {
  if (!accepted_ && num_ignored_bubbles_ < kMaxIgnored)
    ++num_ignored_bubbles_;

  // Ensure |elevation_icon_setter_| is destroyed before |accept_button_|.
  elevation_icon_setter_.reset();
}

views::View* OutdatedUpgradeBubbleView::GetInitiallyFocusedView() {
  return accept_button_;
}

void OutdatedUpgradeBubbleView::WindowClosing() {
  // Reset |upgrade_bubble_| here, not in destructor, because destruction is
  // asynchronous and ShowBubble may be called before full destruction and
  // would attempt to show a bubble that is closing.
  DCHECK_EQ(upgrade_bubble_, this);
  upgrade_bubble_ = NULL;
}

void OutdatedUpgradeBubbleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  accept_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(
          auto_update_enabled_ ? IDS_REINSTALL_APP : IDS_REENABLE_UPDATES));
  accept_button_->SetStyle(views::Button::STYLE_BUTTON);
  accept_button_->SetIsDefault(true);
  accept_button_->SetFontList(rb.GetFontList(ui::ResourceBundle::BoldFont));
  elevation_icon_setter_.reset(new ElevationIconSetter(accept_button_));

  later_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_LATER));
  later_button_->SetStyle(views::Button::STYLE_BUTTON);

  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_TITLE));
  title_label->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* text_label = new views::Label(l10n_util::GetStringUTF16(
      auto_update_enabled_ ? IDS_UPGRADE_BUBBLE_TEXT
                           : IDS_UPGRADE_BUBBLE_REENABLE_TEXT));
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(rb.GetImageSkiaNamed(IDR_UPDATE_MENU_SEVERITY_HIGH));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int kIconTitleColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kIconTitleColumnSetId);

  // Top (icon-title) row.
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);

  // Middle (text) row.
  const int kTextColumnSetId = 1;
  cs = layout->AddColumnSet(kTextColumnSetId);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                views::GridLayout::FIXED, kWidthOfDescriptionText, 0);

  // Bottom (buttons) row.
  const int kButtonsColumnSetId = 2;
  cs = layout->AddColumnSet(kButtonsColumnSetId);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::TRAILING, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kButtonPadding);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::TRAILING, 0,
                views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kIconTitleColumnSetId);
  layout->AddView(image_view);
  layout->AddView(title_label);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, kTextColumnSetId);
  layout->AddView(text_label);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, kButtonsColumnSetId);
  layout->AddView(accept_button_);
  layout->AddView(later_button_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

OutdatedUpgradeBubbleView::OutdatedUpgradeBubbleView(
    views::View* anchor_view,
    content::PageNavigator* navigator,
    bool auto_update_enabled)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      auto_update_enabled_(auto_update_enabled),
      accepted_(false),
      accept_button_(NULL),
      later_button_(NULL),
      navigator_(navigator) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
}

void OutdatedUpgradeBubbleView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (event.IsMouseEvent() &&
      !(static_cast<const ui::MouseEvent*>(&event))->IsOnlyLeftMouseButton()) {
    return;
  }
  HandleButtonPressed(sender);
}

void OutdatedUpgradeBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == accept_button_) {
    accepted_ = true;
    if (auto_update_enabled_) {
      DCHECK(UpgradeDetector::GetInstance()->is_outdated_install());
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "OutdatedUpgradeBubble.NumLaterPerReinstall", num_ignored_bubbles_,
          0, kMaxIgnored, kNumIgnoredBuckets);
      content::RecordAction(
          base::UserMetricsAction("OutdatedUpgradeBubble.Reinstall"));
      navigator_->OpenURL(content::OpenURLParams(GURL(kDownloadChromeUrl),
                                                 content::Referrer(),
                                                 NEW_FOREGROUND_TAB,
                                                 ui::PAGE_TRANSITION_LINK,
                                                 false));
#if defined(OS_WIN)
    } else {
      DCHECK(UpgradeDetector::GetInstance()->is_outdated_install_no_au());
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "OutdatedUpgradeBubble.NumLaterPerEnableAU", num_ignored_bubbles_,
          0, kMaxIgnored, kNumIgnoredBuckets);
      content::RecordAction(
          base::UserMetricsAction("OutdatedUpgradeBubble.EnableAU"));
      // Record that the autoupdate flavour of the dialog has been shown.
      if (g_browser_process->local_state()) {
        g_browser_process->local_state()->SetBoolean(
            prefs::kAttemptedToEnableAutoupdate, true);
      }

      // Re-enable updates by shelling out to setup.exe in the blocking pool.
      content::BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&google_update::ElevateIfNeededToReenableUpdates));
#endif  // defined(OS_WIN)
    }
  } else {
    DCHECK_EQ(later_button_, sender);
    content::RecordAction(
        base::UserMetricsAction("OutdatedUpgradeBubble.Later"));
  }
  GetWidget()->Close();
}
