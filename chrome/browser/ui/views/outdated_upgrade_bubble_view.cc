// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"

#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/installer/util/google_update_util.h"
#endif

namespace {

// Fixed width of the description label of the bubble.
// TODO(mad): Make sure there is enough room for all languages.
const int kWidthOfDescriptionText = 330;

// The URL to be used to re-install Chrome when auto-update failed for too long.
const char kDownloadChromeUrl[] = "https://www.google.com/chrome/?&brand=CHWL"
    "&utm_campaign=en&utm_source=en-et-na-us-chrome-bubble&utm_medium=et";

// The maximum number of ignored bubble we track in the NumLaterPerReinstall
// histogram.
const int kMaxIgnored = 50;
// The number of buckets we want the NumLaterPerReinstall histogram to use.
const int kNumIgnoredBuckets = 5;

// The currently showing bubble.
OutdatedUpgradeBubbleView* g_upgrade_bubble = nullptr;

// The number of times the user ignored the bubble before finally choosing to
// reinstall.
int g_num_ignored_bubbles = 0;

}  // namespace

// OutdatedUpgradeBubbleView ---------------------------------------------------

// static
void OutdatedUpgradeBubbleView::ShowBubble(views::View* anchor_view,
                                           content::PageNavigator* navigator,
                                           bool auto_update_enabled) {
  if (g_upgrade_bubble)
    return;
  g_upgrade_bubble = new OutdatedUpgradeBubbleView(anchor_view, navigator,
                                                   auto_update_enabled);
  views::BubbleDialogDelegateView::CreateBubble(g_upgrade_bubble)->Show();
  content::RecordAction(auto_update_enabled ?
      base::UserMetricsAction("OutdatedUpgradeBubble.Show") :
      base::UserMetricsAction("OutdatedUpgradeBubble.ShowNoAU"));
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
  // Increment the ignored bubble count (if this bubble wasn't ignored, this
  // increment is offset by a decrement in Accept()).
  if (g_num_ignored_bubbles < kMaxIgnored)
    ++g_num_ignored_bubbles;
}

void OutdatedUpgradeBubbleView::WindowClosing() {
  // Reset |g_upgrade_bubble| here, not in destructor, because destruction is
  // asynchronous and ShowBubble may be called before full destruction and
  // would attempt to show a bubble that is closing.
  DCHECK_EQ(g_upgrade_bubble, this);
  g_upgrade_bubble = nullptr;
}

base::string16 OutdatedUpgradeBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_TITLE);
}

bool OutdatedUpgradeBubbleView::Cancel() {
  content::RecordAction(base::UserMetricsAction("OutdatedUpgradeBubble.Later"));
  return true;
}

bool OutdatedUpgradeBubbleView::Accept() {
  // Offset the +1 in the dtor.
  --g_num_ignored_bubbles;

  if (auto_update_enabled_) {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install());
    UMA_HISTOGRAM_CUSTOM_COUNTS("OutdatedUpgradeBubble.NumLaterPerReinstall",
                                g_num_ignored_bubbles, 1, kMaxIgnored,
                                kNumIgnoredBuckets);
    content::RecordAction(
        base::UserMetricsAction("OutdatedUpgradeBubble.Reinstall"));
    navigator_->OpenURL(
        content::OpenURLParams(GURL(kDownloadChromeUrl), content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false));
#if defined(OS_WIN)
  } else {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install_no_au());
    UMA_HISTOGRAM_CUSTOM_COUNTS("OutdatedUpgradeBubble.NumLaterPerEnableAU",
                                g_num_ignored_bubbles, 1, kMaxIgnored,
                                kNumIgnoredBuckets);
    content::RecordAction(
        base::UserMetricsAction("OutdatedUpgradeBubble.EnableAU"));
    // Record that the autoupdate flavour of the dialog has been shown.
    if (g_browser_process->local_state()) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kAttemptedToEnableAutoupdate, true);
    }

    // Re-enable updates by shelling out to setup.exe asynchronously.
    base::PostTaskWithTraits(
        FROM_HERE,
        base::TaskTraits()
            .MayBlock()
            .WithPriority(base::TaskPriority::BACKGROUND)
            .WithShutdownBehavior(base::TaskShutdownBehavior::BLOCK_SHUTDOWN),
        base::Bind(&google_update::ElevateIfNeededToReenableUpdates));
#endif  // defined(OS_WIN)
  }

  return true;
}

void OutdatedUpgradeBubbleView::UpdateButton(views::LabelButton* button,
                                             ui::DialogButton type) {
  BubbleDialogDelegateView::UpdateButton(button, type);
  if (type == ui::DIALOG_BUTTON_OK) {
    elevation_icon_setter_.reset(new ElevationIconSetter(
        button, base::Bind(&OutdatedUpgradeBubbleView::SizeToContents,
                           base::Unretained(this))));
  }
}

base::string16 OutdatedUpgradeBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_CANCEL
          ? IDS_LATER
          : auto_update_enabled_ ? IDS_REINSTALL_APP : IDS_REENABLE_UPDATES);
}

void OutdatedUpgradeBubbleView::Init() {
  SetLayoutManager(new views::FillLayout());
  views::Label* text_label = new views::Label(l10n_util::GetStringUTF16(
      auto_update_enabled_ ? IDS_UPGRADE_BUBBLE_TEXT
                           : IDS_UPGRADE_BUBBLE_REENABLE_TEXT));
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(kWidthOfDescriptionText);
  AddChildView(text_label);
}

OutdatedUpgradeBubbleView::OutdatedUpgradeBubbleView(
    views::View* anchor_view,
    content::PageNavigator* navigator,
    bool auto_update_enabled)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      auto_update_enabled_(auto_update_enabled),
      navigator_(navigator) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));
}
