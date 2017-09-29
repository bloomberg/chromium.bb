// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

namespace {

// Fixed width of the column holding the description label of the bubble.
const int kWidthOfDescriptionText = 320;

// Distance between checkbox and the text to the right of it.
const int kCheckboxTextDistance = 4;

enum SessionCrashedBubbleHistogramValue {
  SESSION_CRASHED_BUBBLE_SHOWN,
  SESSION_CRASHED_BUBBLE_ERROR,
  SESSION_CRASHED_BUBBLE_RESTORED,
  SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_HELP,
  SESSION_CRASHED_BUBBLE_IGNORED,
  SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN,
  SESSION_CRASHED_BUBBLE_STARTUP_PAGES,
  SESSION_CRASHED_BUBBLE_MAX,
};

void RecordBubbleHistogramValue(SessionCrashedBubbleHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SessionCrashed.Bubble", value, SESSION_CRASHED_BUBBLE_MAX);
}

bool DoesSupportConsentCheck() {
#if defined(GOOGLE_CHROME_BUILD)
  return true;
#else
  return false;
#endif
}

}  // namespace

// A helper class that listens to browser removal event.
class SessionCrashedBubbleView::BrowserRemovalObserver
    : public chrome::BrowserListObserver {
 public:
  explicit BrowserRemovalObserver(Browser* browser) : browser_(browser) {
    DCHECK(browser_);
    BrowserList::AddObserver(this);
  }

  ~BrowserRemovalObserver() override { BrowserList::RemoveObserver(this); }

  // Overridden from chrome::BrowserListObserver.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_)
      browser_ = NULL;
  }

  Browser* browser() const { return browser_; }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalObserver);
};

// static
bool SessionCrashedBubble::Show(Browser* browser) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser->profile()->IsOffTheRecord())
    return true;

  // Observes browser removal event and will be deallocated in ShowForReal.
  std::unique_ptr<SessionCrashedBubbleView::BrowserRemovalObserver>
      browser_observer(
          new SessionCrashedBubbleView::BrowserRemovalObserver(browser));

  if (DoesSupportConsentCheck()) {
    base::PostTaskAndReplyWithResult(
        GoogleUpdateSettings::CollectStatsConsentTaskRunner(), FROM_HERE,
        base::Bind(&GoogleUpdateSettings::GetCollectStatsConsent),
        base::Bind(&SessionCrashedBubbleView::ShowForReal,
                   base::Passed(&browser_observer)));
  } else {
    SessionCrashedBubbleView::ShowForReal(std::move(browser_observer), false);
  }
  return true;
}

// static
void SessionCrashedBubbleView::ShowForReal(
    std::unique_ptr<BrowserRemovalObserver> browser_observer,
    bool uma_opted_in_already) {
  // Determine whether or not the UMA opt-in option should be offered. It is
  // offered only when it is a Google chrome build, user hasn't opted in yet,
  // and the preference is modifiable by the user.
  bool offer_uma_optin = false;

  if (DoesSupportConsentCheck() && !uma_opted_in_already)
    offer_uma_optin = !IsMetricsReportingPolicyManaged();

  Browser* browser = browser_observer->browser();

  if (!browser || !browser->tab_strip_model()->GetActiveWebContents()) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar()
                                 ->app_menu_button();
  SessionCrashedBubbleView* crash_bubble =
      new SessionCrashedBubbleView(anchor_view, browser, offer_uma_optin);
  views::BubbleDialogDelegateView::CreateBubble(crash_bubble)->Show();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_SHOWN);
  if (uma_opted_in_already)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN);
}

SessionCrashedBubbleView::SessionCrashedBubbleView(views::View* anchor_view,
                                                   Browser* browser,
                                                   bool offer_uma_optin)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      uma_option_(NULL),
      offer_uma_optin_(offer_uma_optin),
      ignored_(true) {
  set_close_on_deactivate(false);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SESSION_CRASHED);
}

SessionCrashedBubbleView::~SessionCrashedBubbleView() {
}

base::string16 SessionCrashedBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE);
}

bool SessionCrashedBubbleView::ShouldShowWindowTitle() const {
  return true;
}

bool SessionCrashedBubbleView::ShouldShowCloseButton() const {
  return true;
}

void SessionCrashedBubbleView::OnWidgetDestroying(views::Widget* widget) {
  if (ignored_)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_IGNORED);
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
}

void SessionCrashedBubbleView::Init() {
  SetLayoutManager(new views::FillLayout());

  // Description text label.
  views::Label* text_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(kWidthOfDescriptionText);
  AddChildView(text_label);
}

views::View* SessionCrashedBubbleView::CreateFootnoteView() {
  if (!offer_uma_optin_)
    return nullptr;

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN);

  // Checkbox for metric reporting setting.
  // Since the text to the right of the checkbox can't be a simple string (needs
  // a hyperlink in it), this checkbox contains an empty string as its label,
  // and the real text will be added as a separate view.
  uma_option_ = new views::Checkbox(base::string16());
  uma_option_->SetChecked(false);

  // The text to the right of the checkbox.
  size_t offset;
  base::string16 link_text =
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_UMA_LINK_TEXT);
  base::string16 uma_text = l10n_util::GetStringFUTF16(
      IDS_SESSION_CRASHED_VIEW_UMA_OPTIN,
      link_text,
      &offset);
  views::StyledLabel* uma_label = new views::StyledLabel(uma_text, this);
  uma_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                           views::StyledLabel::RangeStyleInfo::CreateForLink());
  views::StyledLabel::RangeStyleInfo uma_style;
  uma_style.text_style = STYLE_SECONDARY;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    uma_label->AddStyleRange(before_link_range, uma_style);
  gfx::Range after_link_range(offset + link_text.length(), uma_text.length());
  if (!after_link_range.is_empty())
    uma_label->AddStyleRange(after_link_range, uma_style);
  // Shift the text down by 1px to align with the checkbox.
  uma_label->SetBorder(views::CreateEmptyBorder(1, 0, 0, 0));

  // Create a view to hold the checkbox and the text.
  views::View* uma_view = new views::View();
  GridLayout* uma_layout = GridLayout::CreateAndInstall(uma_view);
  uma_view->SetLayoutManager(uma_layout);

  const int kReportColumnSetId = 0;
  views::ColumnSet* cs = uma_layout->AddColumnSet(kReportColumnSetId);
  cs->AddColumn(GridLayout::CENTER, GridLayout::LEADING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kCheckboxTextDistance);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1, GridLayout::USE_PREF, 0,
                0);

  uma_layout->StartRow(0, kReportColumnSetId);
  uma_layout->AddView(uma_option_);
  uma_layout->AddView(uma_label);

  return uma_view;
}

bool SessionCrashedBubbleView::Accept() {
  RestorePreviousSession();
  return true;
}

// The cancel button is used as an option to open the startup pages instead of
// restoring the previous session.
bool SessionCrashedBubbleView::Cancel() {
  OpenStartupPages();
  return true;
}

bool SessionCrashedBubbleView::Close() {
  // Don't default to Accept() just because that's the only choice. Instead, do
  // nothing.
  return true;
}

int SessionCrashedBubbleView::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_OK;
  // Offer the option to open the startup pages using the cancel button, but
  // only when the user has selected the URLS option, and set at least one url.
  SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(browser_->profile());
  if (session_startup_pref.type == SessionStartupPref::URLS &&
      !session_startup_pref.urls.empty()) {
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  }
  return buttons;
}

base::string16 SessionCrashedBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  return l10n_util::GetStringUTF16(
      IDS_SESSION_CRASHED_VIEW_STARTUP_PAGES_BUTTON);
}

void SessionCrashedBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  browser_->OpenURL(content::OpenURLParams(
      GURL("https://support.google.com/chrome/answer/96817"),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_HELP);
}

void SessionCrashedBubbleView::RestorePreviousSession() {
  ignored_ = false;
  MaybeEnableUMA();
  CloseBubble();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_RESTORED);
  // Restoring tabs has side effects, so it's preferable to do it after the
  // bubble was closed.
  SessionRestore::RestoreSessionAfterCrash(browser_);
}

void SessionCrashedBubbleView::OpenStartupPages() {
  ignored_ = false;
  MaybeEnableUMA();
  CloseBubble();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_STARTUP_PAGES);
  // Opening tabs has side effects, so it's preferable to do it after the bubble
  // was closed.
  SessionRestore::OpenStartupPagesAfterCrash(browser_);
}

void SessionCrashedBubbleView::MaybeEnableUMA() {
  // Record user's choice for opt-in in to UMA.
  // There's no opt-out choice in the crash restore bubble.
  if (uma_option_ && uma_option_->checked()) {
    ChangeMetricsReportingState(true);
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_UMA_OPTIN);
  }
}

void SessionCrashedBubbleView::CloseBubble() {
  GetWidget()->Close();
}
