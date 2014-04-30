// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <vector>

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/browser/ui/startup/session_crashed_bubble.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

namespace {

// Fixed width of the column holding the description label of the bubble.
const int kWidthOfDescriptionText = 320;

// Margins width for the top rows to compensate for the bottom panel for which
// we don't want any margin.
const int kMarginWidth = 12;
const int kMarginHeight = kMarginWidth;

// The color of the background of the sub panel to offer UMA optin.
const SkColor kLightGrayBackgroundColor = 0xFFF0F0F0;
const SkColor kWhiteBackgroundColor = 0xFFFFFFFF;

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

}  // namespace

// static
void SessionCrashedBubbleView::Show(Browser* browser) {
  if (browser->profile()->IsOffTheRecord())
    return;

  views::View* anchor_view =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->app_menu();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  SessionCrashedBubbleView* crash_bubble =
      new SessionCrashedBubbleView(anchor_view, browser, web_contents);
  views::BubbleDelegateView::CreateBubble(crash_bubble)->Show();
}

SessionCrashedBubbleView::SessionCrashedBubbleView(
    views::View* anchor_view,
    Browser* browser,
    content::WebContents* web_contents)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      content::WebContentsObserver(web_contents),
      browser_(browser),
      web_contents_(web_contents),
      restore_button_(NULL),
      close_(NULL),
      uma_option_(NULL),
      started_navigation_(false) {
  set_close_on_deactivate(false);
  set_move_with_anchor(true);
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

  // Learn more link
  views::Link* learn_more_link = NULL;
  if (ShouldOfferMetricsReporting()) {
    learn_more_link = new views::Link(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    learn_more_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    learn_more_link->set_listener(this);
    learn_more_link->SetUnderline(false);
  }

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

  // Learn more link and restore button row
  const int kLinkAndButtonColumnSetId = 2;
  cs = layout->AddColumnSet(kLinkAndButtonColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  if (learn_more_link) {
    cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                  GridLayout::USE_PREF, 0, 0);
  }
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

  layout->StartRow(0, kLinkAndButtonColumnSetId);
  if (learn_more_link)
    layout->AddView(learn_more_link);
  layout->AddView(restore_button_);
  layout->AddPaddingRow(0, kMarginHeight);

  // Metrics reporting option.
  if (learn_more_link)
    CreateUmaOptinView(layout);

  set_color(kWhiteBackgroundColor);
  set_margins(gfx::Insets());
  Layout();
}

void SessionCrashedBubbleView::CreateUmaOptinView(GridLayout* layout) {
  // Checkbox for metric reporting setting.
  uma_option_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_UMA_OPTIN));
  uma_option_->SetTextColor(views::Button::STATE_NORMAL, SK_ColorGRAY);
  uma_option_->SetChecked(false);
  uma_option_->SetTextMultiLine(true);
  uma_option_->set_background(
      views::Background::CreateSolidBackground(kLightGrayBackgroundColor));
  uma_option_->set_listener(this);
  // We use a border instead of padding so that the background color reach
  // the edges of the bubble.
  uma_option_->SetBorder(
      views::Border::CreateSolidSidedBorder(
          kMarginHeight, kMarginWidth, kMarginHeight, kMarginWidth,
          kLightGrayBackgroundColor));

  // Separator.
  const int kSeparatorColumnSetId = 3;
  views::ColumnSet* cs = layout->AddColumnSet(kSeparatorColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kWidthOfDescriptionText + kMarginWidth, 0);

  // Reporting row.
  const int kReportColumnSetId = 4;
  cs = layout->AddColumnSet(kReportColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kWidthOfDescriptionText + kMarginWidth, 0);

  layout->StartRow(0, kSeparatorColumnSetId);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));
  layout->StartRow(0, kReportColumnSetId);
  layout->AddView(uma_option_);
}

void SessionCrashedBubbleView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  DCHECK(sender);
  if (sender == restore_button_)
    RestorePreviousSession(sender);
  else if (sender == close_)
    CloseBubble();
}

void SessionCrashedBubbleView::LinkClicked(views::Link* source,
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
  if (uma_option_ && uma_option_->checked())
    OptionsUtil::ResolveMetricsReportingEnabled(true);
  CloseBubble();
}

void SessionCrashedBubbleView::CloseBubble() {
  GetWidget()->Close();
}

bool ShowSessionCrashedBubble(Browser* browser) {
  SessionCrashedBubbleView::Show(browser);
  return true;
}
