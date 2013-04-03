// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/upgrade_detector.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

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
                                           content::PageNavigator* navigator) {
  if (IsShowing())
    return;
  upgrade_bubble_ = new OutdatedUpgradeBubbleView(anchor_view, navigator);
  views::BubbleDelegateView::CreateBubble(upgrade_bubble_);
  upgrade_bubble_->StartFade(true);
  content::RecordAction(
      content::UserMetricsAction("OutdatedUpgradeBubble.Show"));
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
  if (!chose_to_reinstall_ && num_ignored_bubbles_ < kMaxIgnored)
    ++num_ignored_bubbles_;
}

views::View* OutdatedUpgradeBubbleView::GetInitiallyFocusedView() {
  return reinstall_button_;
}

void OutdatedUpgradeBubbleView::WindowClosing() {
  // Reset |upgrade_bubble_| here, not in destructor, because destruction is
  // asynchronous and ShowBubble may be called before full destruction and
  // would attempt to show a bubble that is closing.
  DCHECK_EQ(upgrade_bubble_, this);
  upgrade_bubble_ = NULL;
}

void OutdatedUpgradeBubbleView::Init() {
  string16 product_name(l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  reinstall_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringFUTF16(IDS_REINSTALL_APP, product_name));
  reinstall_button_->SetIsDefault(true);
  reinstall_button_->SetFont(rb.GetFont(ui::ResourceBundle::BoldFont));

  later_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_LATER));

  views::Label* title_label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_UPGRADE_BUBBLE_TITLE, product_name));
  title_label->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* text_label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_UPGRADE_BUBBLE_TEXT, product_name));
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(rb.GetImageSkiaNamed(IDR_UPDATE_MENU3));

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const int kIconTitleColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kIconTitleColumnSetId);

  // Top (icon-title) row.
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);

  // Middle (text) row.
  const int kTextColumnSetId = 1;
  cs = layout->AddColumnSet(kTextColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                GridLayout::FIXED, kWidthOfDescriptionText, 0);

  // Bottom (buttons) row.
  const int kButtonsColumnSetId = 2;
  cs = layout->AddColumnSet(kButtonsColumnSetId);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kButtonPadding);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kIconTitleColumnSetId);
  layout->AddView(image_view);
  layout->AddView(title_label);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, kTextColumnSetId);
  layout->AddView(text_label);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, kButtonsColumnSetId);
  layout->AddView(reinstall_button_);
  layout->AddView(later_button_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

OutdatedUpgradeBubbleView::OutdatedUpgradeBubbleView(
    views::View* anchor_view, content::PageNavigator* navigator)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      chose_to_reinstall_(false),
      reinstall_button_(NULL),
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
  if (sender == reinstall_button_) {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install());
    chose_to_reinstall_ = true;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "OutdatedUpgradeBubble.NumLaterPerReinstall", num_ignored_bubbles_,
        0, kMaxIgnored, kNumIgnoredBuckets);
    content::RecordAction(
        content::UserMetricsAction("OutdatedUpgradeBubble.Reinstall"));
    navigator_->OpenURL(content::OpenURLParams(GURL(kDownloadChromeUrl),
                                               content::Referrer(),
                                               NEW_FOREGROUND_TAB,
                                               content::PAGE_TRANSITION_LINK,
                                               false));
  } else {
    DCHECK_EQ(later_button_, sender);
    content::RecordAction(
        content::UserMetricsAction("OutdatedUpgradeBubble.Later"));
  }
  StartFade(false);
}
