// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/options/options_util.h"
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
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
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

// Margins width for the top rows to compensate for the bottom panel for which
// we don't want any margin.
const int kMarginWidth = 12;
const int kMarginHeight = kMarginWidth;

// The color of the background of the sub panel to offer UMA optin.
const SkColor kLightGrayBackgroundColor = 0xFFF0F0F0;
const SkColor kWhiteBackgroundColor = 0xFFFFFFFF;

// The Finch study name and group name that enables session crashed bubble UI.
const char kEnableBubbleUIFinchName[] = "EnableSessionCrashedBubbleUI";
const char kEnableBubbleUIGroupEnabled[] = "Enabled";

bool ShouldOfferMetricsReporting() {
// Stats collection only applies to Google Chrome builds.
#if defined(GOOGLE_CHROME_BUILD)
  // Only show metrics reporting option if user didn't already consent to it.
  if (GoogleUpdateSettings::GetCollectStatsConsent())
    return false;
  return g_browser_process->local_state()->FindPreference(
      prefs::kMetricsReportingEnabled)->IsUserModifiable();
#else
  return false;
#endif  // defined(GOOGLE_CHROME_BUILD)
}

// Whether or not the bubble UI should be used.
bool IsBubbleUIEnabled() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      kEnableBubbleUIFinchName);
  const base::CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableSessionCrashedBubble))
    return false;
  if (command_line.HasSwitch(switches::kEnableSessionCrashedBubble))
    return true;
  return group_name == kEnableBubbleUIGroupEnabled;
}

}  // namespace

// A helper class that listens to browser removal event.
class SessionCrashedBubbleView::BrowserRemovalObserver
    : public chrome::BrowserListObserver {
 public:
  explicit BrowserRemovalObserver(Browser* browser);
  virtual ~BrowserRemovalObserver();

  // Overridden from chrome::BrowserListObserver.
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  Browser* browser() const;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalObserver);
};

SessionCrashedBubbleView::BrowserRemovalObserver::BrowserRemovalObserver(
    Browser* browser)
    : browser_(browser) {
  DCHECK(browser_);
  BrowserList::AddObserver(this);
}

SessionCrashedBubbleView::BrowserRemovalObserver::~BrowserRemovalObserver() {
  BrowserList::RemoveObserver(this);
}

void SessionCrashedBubbleView::BrowserRemovalObserver::OnBrowserRemoved(
    Browser* browser) {
  if (browser == browser_)
    browser_ = NULL;
}

Browser* SessionCrashedBubbleView::BrowserRemovalObserver::browser() const {
  return browser_;
}

// static
void SessionCrashedBubbleView::Show(Browser* browser) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (browser->profile()->IsOffTheRecord())
    return;

  // Observes browser removal event and will be deallocated in ShowForReal.
  scoped_ptr<BrowserRemovalObserver> browser_observer(
      new BrowserRemovalObserver(browser));

  // Schedule a task to run ShouldOfferMetricsReporting() on FILE thread, since
  // GoogleUpdateSettings::GetCollectStatsConsent() does IO. Then, call
  // SessionCrashedBubbleView::ShowForReal with the result.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ShouldOfferMetricsReporting),
      base::Bind(&SessionCrashedBubbleView::ShowForReal,
                 base::Passed(&browser_observer)));
}

// static
void SessionCrashedBubbleView::ShowForReal(
    scoped_ptr<BrowserRemovalObserver> browser_observer,
    bool offer_uma_optin) {
  Browser* browser = browser_observer->browser();

  if (!browser)
    return;

  views::View* anchor_view =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->app_menu();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  SessionCrashedBubbleView* crash_bubble =
      new SessionCrashedBubbleView(anchor_view, browser, web_contents,
                                   offer_uma_optin);
  views::BubbleDelegateView::CreateBubble(crash_bubble)->Show();
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
      close_(NULL),
      uma_option_(NULL),
      offer_uma_optin_(offer_uma_optin),
      started_navigation_(false) {
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

void SessionCrashedBubbleView::Init() {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  // Close button.
  close_ = new views::LabelButton(this, base::string16());
  close_->SetImage(views::CustomButton::STATE_NORMAL,
                   *rb->GetImageNamed(IDR_CLOSE_2).ToImageSkia());
  close_->SetImage(views::CustomButton::STATE_HOVERED,
                   *rb->GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  close_->SetImage(views::CustomButton::STATE_PRESSED,
                   *rb->GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
  close_->SetSize(close_->GetPreferredSize());
  close_->SetBorder(views::Border::CreateEmptyBorder(0, 0, 0, 0));

  // Bubble title label.
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFontList(rb->GetFontList(ui::ResourceBundle::BoldFont));

  // Description text label.
  views::Label* text_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetEnabledColor(SK_ColorDKGRAY);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(kWidthOfDescriptionText);

  // Restore button.
  restore_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON));
  restore_button_->SetStyle(views::Button::STYLE_BUTTON);
  restore_button_->SetIsDefault(true);
  restore_button_->SetFontList(rb->GetFontList(ui::ResourceBundle::BoldFont));

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Title and close button row.
  const int kTitleColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kTitleColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::LEADING, 0,
                GridLayout::USE_PREF, 0, 0);

  // Text row.
  const int kTextColumnSetId = 1;
  cs = layout->AddColumnSet(kTextColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kWidthOfDescriptionText, 0);

  // Restore button row
  const int kButtonColumnSetId = 2;
  cs = layout->AddColumnSet(kButtonColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kMarginWidth);

  layout->AddPaddingRow(0, kMarginHeight);
  layout->StartRow(0, kTitleColumnSetId);
  layout->AddView(title_label);
  layout->AddView(close_);
  layout->AddPaddingRow(0, kMarginHeight);

  layout->StartRow(0, kTextColumnSetId);
  layout->AddView(text_label);
  layout->AddPaddingRow(0, kMarginHeight);

  layout->StartRow(0, kButtonColumnSetId);
  layout->AddView(restore_button_);
  layout->AddPaddingRow(0, kMarginHeight);

  // Metrics reporting option.
  if (offer_uma_optin_)
    CreateUmaOptinView(layout);

  set_color(kWhiteBackgroundColor);
  set_margins(gfx::Insets());
  Layout();
}

void SessionCrashedBubbleView::CreateUmaOptinView(GridLayout* layout) {
  // Checkbox for metric reporting setting.
  // Since the text to the right of the checkbox can't be a simple string (needs
  // a hyperlink in it), this checkbox contains an empty string as its label,
  // and the real text will be added as a separate view.
  uma_option_ = new views::Checkbox(base::string16());
  uma_option_->SetTextColor(views::Button::STATE_NORMAL, SK_ColorGRAY);
  uma_option_->SetChecked(false);
  uma_option_->set_background(
      views::Background::CreateSolidBackground(kLightGrayBackgroundColor));
  uma_option_->set_listener(this);

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
      views::Background::CreateSolidBackground(kLightGrayBackgroundColor));
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.font_style = gfx::Font::NORMAL;
  uma_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                           link_style);
  views::StyledLabel::RangeStyleInfo uma_style;
  uma_style.color = SK_ColorGRAY;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    uma_label->AddStyleRange(before_link_range, uma_style);
  gfx::Range after_link_range(offset + link_text.length(), uma_text.length());
  if (!after_link_range.is_empty())
    uma_label->AddStyleRange(after_link_range, uma_style);

  // We use a border instead of padding so that the background color reach
  // the edges of the bubble.
  uma_option_->SetBorder(
      views::Border::CreateSolidSidedBorder(0, kMarginWidth, 0, 0,
                                            kLightGrayBackgroundColor));
  uma_label->SetBorder(
      views::Border::CreateSolidSidedBorder(
          kMarginHeight, kCheckboxTextDistance, kMarginHeight, kMarginWidth,
          kLightGrayBackgroundColor));

  // Separator.
  const int kSeparatorColumnSetId = 3;
  views::ColumnSet* cs = layout->AddColumnSet(kSeparatorColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kWidthOfDescriptionText + kMarginWidth, 0);

  // Reporting row.
  const int kReportColumnSetId = 4;
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
  DCHECK(sender);
  if (sender == restore_button_)
    RestorePreviousSession(sender);
  else if (sender == close_)
    CloseBubble();
}

void SessionCrashedBubbleView::StyledLabelLinkClicked(const gfx::Range& range,
                                                      int event_flags) {
  browser_->OpenURL(content::OpenURLParams(
      GURL("https://support.google.com/chrome/answer/96817"),
      content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK,
      false));
}

void SessionCrashedBubbleView::DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) {
  started_navigation_ = true;
}

void SessionCrashedBubbleView::DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) {
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

  // Record user's choice for opting in to UMA.
  // There's no opting-out choice in the crash restore bubble.
  if (uma_option_ && uma_option_->checked()) {
    // TODO: Clean up function ResolveMetricsReportingEnabled so that user pref
    // is stored automatically.
    OptionsUtil::ResolveMetricsReportingEnabled(true);
    g_browser_process->local_state()->SetBoolean(
        prefs::kMetricsReportingEnabled, true);
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
