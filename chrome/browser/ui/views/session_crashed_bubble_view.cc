// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/startup/session_crashed_bubble.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

namespace {

// Fixed width of the column holding the description label of the bubble.
const int kWidthOfDescriptionText = 320;

// Distance between checkbox and the text to the right of it.
const int kCheckboxTextDistance = 4;

// The color of the text and background of the sub panel to offer UMA optin.
// These values match the BookmarkSyncPromoView colors.
const SkColor kBackgroundColor = SkColorSetRGB(245, 245, 245);
const SkColor kTextColor = SkColorSetRGB(102, 102, 102);

// The Finch study name and group name that enables session crashed bubble UI.
const char kEnableBubbleUIFinchName[] = "EnableSessionCrashedBubbleUI";
const char kEnableBubbleUIGroupEnabled[] = "Enabled";

enum SessionCrashedBubbleHistogramValue {
  SESSION_CRASHED_BUBBLE_SHOWN,
  SESSION_CRASHED_BUBBLE_ERROR,
  SESSION_CRASHED_BUBBLE_RESTORED,
  SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_HELP,
  SESSION_CRASHED_BUBBLE_IGNORED,
  SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN,
  SESSION_CRASHED_BUBBLE_MAX,
};

void RecordBubbleHistogramValue(SessionCrashedBubbleHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SessionCrashed.Bubble", value, SESSION_CRASHED_BUBBLE_MAX);
}

// Whether or not the bubble UI should be used.
bool IsBubbleUIEnabled() {
  const base::CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableSessionCrashedBubble))
    return false;
  if (command_line.HasSwitch(switches::kEnableSessionCrashedBubble))
    return true;
  const std::string group_name = base::FieldTrialList::FindFullName(
      kEnableBubbleUIFinchName);
  return group_name == kEnableBubbleUIGroupEnabled;
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

  virtual ~BrowserRemovalObserver() {
    BrowserList::RemoveObserver(this);
  }

  // Overridden from chrome::BrowserListObserver.
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE {
    if (browser == browser_)
      browser_ = NULL;
  }

  Browser* browser() const { return browser_; }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalObserver);
};

// static
void SessionCrashedBubbleView::Show(Browser* browser) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (browser->profile()->IsOffTheRecord())
    return;

  // Observes browser removal event and will be deallocated in ShowForReal.
  scoped_ptr<BrowserRemovalObserver> browser_observer(
      new BrowserRemovalObserver(browser));

// Stats collection only applies to Google Chrome builds.
#if defined(GOOGLE_CHROME_BUILD)
  // Schedule a task to run GoogleUpdateSettings::GetCollectStatsConsent() on
  // FILE thread, since it does IO. Then, call
  // SessionCrashedBubbleView::ShowForReal with the result.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GoogleUpdateSettings::GetCollectStatsConsent),
      base::Bind(&SessionCrashedBubbleView::ShowForReal,
                 base::Passed(&browser_observer)));
#else
  SessionCrashedBubbleView::ShowForReal(browser_observer.Pass(), false);
#endif  // defined(GOOGLE_CHROME_BUILD)
}

// static
void SessionCrashedBubbleView::ShowForReal(
    scoped_ptr<BrowserRemovalObserver> browser_observer,
    bool uma_opted_in_already) {
  // Determine whether or not the uma opt-in option should be offered. It is
  // offered only when it is a Google chrome build, user hasn't opted in yet,
  // and the preference is modifiable by the user.
  bool offer_uma_optin = false;

#if defined(GOOGLE_CHROME_BUILD)
  if (!uma_opted_in_already) {
    offer_uma_optin = g_browser_process->local_state()->FindPreference(
        prefs::kMetricsReportingEnabled)->IsUserModifiable();
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  Browser* browser = browser_observer->browser();

  if (!browser) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  views::View* anchor_view =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->app_menu();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  if (!web_contents) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  SessionCrashedBubbleView* crash_bubble =
      new SessionCrashedBubbleView(anchor_view, browser, web_contents,
                                   offer_uma_optin);
  views::BubbleDelegateView::CreateBubble(crash_bubble)->Show();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_SHOWN);
  if (uma_opted_in_already)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN);
}

SessionCrashedBubbleView::SessionCrashedBubbleView(
    views::View* anchor_view,
    Browser* browser,
    content::WebContents* web_contents,
    bool offer_uma_optin)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      content::WebContentsObserver(web_contents),
      browser_(browser),
      web_contents_(web_contents),
      restore_button_(NULL),
      uma_option_(NULL),
      offer_uma_optin_(offer_uma_optin),
      started_navigation_(false),
      restored_(false) {
  set_close_on_deactivate(false);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<content::NavigationController>(&(
          web_contents->GetController())));
  browser->tab_strip_model()->AddObserver(this);
}

SessionCrashedBubbleView::~SessionCrashedBubbleView() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

views::View* SessionCrashedBubbleView::GetInitiallyFocusedView() {
  return restore_button_;
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
  if (!restored_)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_IGNORED);
  BubbleDelegateView::OnWidgetDestroying(widget);
}

void SessionCrashedBubbleView::Init() {
  // Description text label.
  views::Label* text_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(kWidthOfDescriptionText);

  // Restore button.
  restore_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON));
  restore_button_->SetStyle(views::Button::STYLE_BUTTON);
  restore_button_->SetIsDefault(true);

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Text row.
  const int kTextColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kTextColumnSetId);
  cs->AddPaddingColumn(0, GetBubbleFrameView()->GetTitleInsets().left());
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                GridLayout::FIXED, kWidthOfDescriptionText, 0);
  cs->AddPaddingColumn(0, GetBubbleFrameView()->GetTitleInsets().left());

  // Restore button row.
  const int kButtonColumnSetId = 1;
  cs = layout->AddColumnSet(kButtonColumnSetId);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, GetBubbleFrameView()->GetTitleInsets().left());

  layout->StartRow(0, kTextColumnSetId);
  layout->AddView(text_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, kButtonColumnSetId);
  layout->AddView(restore_button_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Metrics reporting option.
  if (offer_uma_optin_) {
    CreateUmaOptinView(layout);
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN);
  }

  set_margins(gfx::Insets());
  Layout();
}

void SessionCrashedBubbleView::CreateUmaOptinView(GridLayout* layout) {
  // Checkbox for metric reporting setting.
  // Since the text to the right of the checkbox can't be a simple string (needs
  // a hyperlink in it), this checkbox contains an empty string as its label,
  // and the real text will be added as a separate view.
  uma_option_ = new views::Checkbox(base::string16());
  uma_option_->SetChecked(false);
  uma_option_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));

  // The text to the right of the checkbox.
  size_t offset;
  base::string16 link_text =
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_UMA_LINK_TEXT);
  base::string16 uma_text = l10n_util::GetStringFUTF16(
      IDS_SESSION_CRASHED_VIEW_UMA_OPTIN,
      link_text,
      &offset);
  views::StyledLabel* uma_label = new views::StyledLabel(uma_text, this);
  uma_label->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.font_style = gfx::Font::NORMAL;
  uma_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                           link_style);
  views::StyledLabel::RangeStyleInfo uma_style;
  uma_style.color = kTextColor;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    uma_label->AddStyleRange(before_link_range, uma_style);
  gfx::Range after_link_range(offset + link_text.length(), uma_text.length());
  if (!after_link_range.is_empty())
    uma_label->AddStyleRange(after_link_range, uma_style);

  // We use a border instead of padding so that the background color reaches
  // the edges of the bubble.
  const gfx::Insets title_insets = GetBubbleFrameView()->GetTitleInsets();
  uma_option_->SetBorder(views::Border::CreateSolidSidedBorder(
      0, title_insets.left(), 0, 0, kBackgroundColor));
  uma_label->SetBorder(views::Border::CreateSolidSidedBorder(
      views::kRelatedControlVerticalSpacing, kCheckboxTextDistance,
      views::kRelatedControlVerticalSpacing, title_insets.left(),
      kBackgroundColor));

  // Separator.
  const int kSeparatorColumnSetId = 2;
  views::ColumnSet* cs = layout->AddColumnSet(kSeparatorColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                GridLayout::USE_PREF, 0, 0);

  // Reporting row.
  const int kReportColumnSetId = 3;
  cs = layout->AddColumnSet(kReportColumnSetId);
  cs->AddColumn(GridLayout::CENTER, GridLayout::FILL, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kWidthOfDescriptionText, 0);

  layout->StartRow(0, kSeparatorColumnSetId);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
  layout->StartRow(0, kReportColumnSetId);
  layout->AddView(uma_option_);
  layout->AddView(uma_label);
}

void SessionCrashedBubbleView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  DCHECK_EQ(sender, restore_button_);
  RestorePreviousSession(sender);
}

void SessionCrashedBubbleView::StyledLabelLinkClicked(const gfx::Range& range,
                                                      int event_flags) {
  browser_->OpenURL(content::OpenURLParams(
      GURL("https://support.google.com/chrome/answer/96817"),
      content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK,
      false));
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_HELP);
}

void SessionCrashedBubbleView::DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) {
  started_navigation_ = true;
}

void SessionCrashedBubbleView::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (started_navigation_)
    CloseBubble();
}

void SessionCrashedBubbleView::WasShown() {
  GetWidget()->Show();
}

void SessionCrashedBubbleView::WasHidden() {
  GetWidget()->Hide();
}

void SessionCrashedBubbleView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_TAB_CLOSING)
    CloseBubble();
}

void SessionCrashedBubbleView::TabDetachedAt(content::WebContents* contents,
                                             int index) {
  if (web_contents_ == contents)
    CloseBubble();
}

void SessionCrashedBubbleView::RestorePreviousSession(views::Button* sender) {
  SessionRestore::RestoreSessionAfterCrash(browser_);
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_RESTORED);
  restored_ = true;

  // Record user's choice for opting in to UMA.
  // There's no opting-out choice in the crash restore bubble.
  if (uma_option_ && uma_option_->checked()) {
    // TODO: Clean up function ResolveMetricsReportingEnabled so that user pref
    // is stored automatically.
    ResolveMetricsReportingEnabled(true);
    g_browser_process->local_state()->SetBoolean(
        prefs::kMetricsReportingEnabled, true);
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_UMA_OPTIN);
  }
  CloseBubble();
}

void SessionCrashedBubbleView::CloseBubble() {
  GetWidget()->Close();
}

bool ShowSessionCrashedBubble(Browser* browser) {
  if (IsBubbleUIEnabled()) {
    SessionCrashedBubbleView::Show(browser);
    return true;
  }
  return false;
}
