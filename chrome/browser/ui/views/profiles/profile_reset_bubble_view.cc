// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_reset_bubble_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profile_resetter/profile_reset_global_error.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/profile_reset_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using views::GridLayout;

namespace {

// Fixed width of the column holding the description label of the bubble.
const int kWidthOfDescriptionText = 370;

// Margins width for the top rows to compensate for the bottom panel for which
// we don't want any margin.
const int kMarginWidth = 12;
const int kMarginHeight = kMarginWidth;

// Width of a colum in the FeedbackView.
const int kFeedbackViewColumnWidth = kWidthOfDescriptionText / 2 + kMarginWidth;

// Width of the column used to disaplay the help button.
const int kHelpButtonColumnWidth = 30;

// Width of the reporting checkbox column.
const int kReportingCheckboxColumnWidth =
    kWidthOfDescriptionText + 2 * kMarginWidth;

// Full width including all columns.
const int kAllColumnsWidth =
    kReportingCheckboxColumnWidth + kHelpButtonColumnWidth;

// Maximum height of the scrollable feedback view.
const int kMaxFeedbackViewHeight = 450;

// The vertical padding between two values in the feedback view.
const int kInterFeedbackValuePadding = 4;

// We subtract 2 to account for the natural button padding, and
// to bring the separation visually in line with the row separation
// height.
const int kButtonPadding = views::kRelatedButtonHSpacing - 2;

// The color of the background of the sub panel to report current settings.
const SkColor kLightGrayBackgroundColor = 0xFFF5F5F5;

// This view is used to contain the scrollable contents that are shown the user
// to expose what feedback will be sent back to Google.
class FeedbackView : public views::View {
 public:
  FeedbackView() {}

  // Setup the layout manager of the Feedback view using the content of the
  // |feedback| ListValue which contains a list of key/value pairs stored in
  // DictionaryValues. The key is to be displayed right aligned on the left, and
  // the value as a left aligned multiline text on the right.
  void SetupLayoutManager(const base::ListValue& feedback) {
    RemoveAllChildViews(true);
    set_background(views::Background::CreateSolidBackground(
        kLightGrayBackgroundColor));

    GridLayout* layout = new GridLayout(this);
    SetLayoutManager(layout);

    // We only need a single column set for left/right text and middle margin.
    views::ColumnSet* cs = layout->AddColumnSet(0);
    cs->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                  GridLayout::FIXED, kFeedbackViewColumnWidth, 0);
    cs->AddPaddingColumn(0, kMarginWidth);
    cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                  GridLayout::FIXED, kFeedbackViewColumnWidth, 0);
    for (size_t i = 0; i < feedback.GetSize(); ++i) {
      const base::DictionaryValue* dictionary = NULL;
      if (!feedback.GetDictionary(i, &dictionary) || !dictionary)
        continue;

      base::string16 key;
      if (!dictionary->GetString("key", &key))
        continue;

      base::string16 value;
      if (!dictionary->GetString("value", &value))
        continue;

      // The key is shown on the left, multi-line (required to allow wrapping in
      // case the key name does not fit), and right-aligned.
      views::Label* left_text_label = new views::Label(key);
      left_text_label->SetMultiLine(true);
      left_text_label->SetEnabledColor(SK_ColorGRAY);
      left_text_label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

      // The value is shown on the right, multi-line, left-aligned.
      views::Label* right_text_label = new views::Label(value);
      right_text_label->SetMultiLine(true);
      right_text_label->SetEnabledColor(SK_ColorDKGRAY);
      right_text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

      layout->StartRow(0, 0);
      layout->AddView(left_text_label);
      layout->AddView(right_text_label);
      layout->AddPaddingRow(0, kInterFeedbackValuePadding);
    }

    // We need to set our size to our preferred size because our parent is a
    // scroll view and doesn't know which size to set us to. Also since our
    // parent scrolls, we are not bound to its size. So our size is based on the
    // size computed by the our layout manager, which is what
    // SizeToPreferredSize() does.
    SizeToPreferredSize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedbackView);
};

}  // namespace

// ProfileResetBubbleView ---------------------------------------------------

// static
ProfileResetBubbleView* ProfileResetBubbleView::ShowBubble(
    const base::WeakPtr<ProfileResetGlobalError>& global_error,
    Browser* browser) {
  views::View* anchor_view =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->app_menu();
  ProfileResetBubbleView* reset_bubble = new ProfileResetBubbleView(
      global_error, anchor_view, browser, browser->profile());
  views::BubbleDelegateView::CreateBubble(reset_bubble)->Show();
  content::RecordAction(base::UserMetricsAction("SettingsResetBubble.Show"));
  return reset_bubble;
}

ProfileResetBubbleView::~ProfileResetBubbleView() {}

views::View* ProfileResetBubbleView::GetInitiallyFocusedView() {
  return controls_.reset_button;
}

void ProfileResetBubbleView::WindowClosing() {
  if (global_error_)
    global_error_->OnBubbleViewDidClose();
}

ProfileResetBubbleView::ProfileResetBubbleView(
    const base::WeakPtr<ProfileResetGlobalError>& global_error,
    views::View* anchor_view,
    content::PageNavigator* navigator,
    Profile* profile)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      navigator_(navigator),
      profile_(profile),
      global_error_(global_error),
      resetting_(false),
      chose_to_reset_(false),
      show_help_pane_(false),
      weak_factory_(this) {
}

void ProfileResetBubbleView::ResetAllChildren() {
  controls_.Reset();
  SetLayoutManager(NULL);
  RemoveAllChildViews(true);
}

void ProfileResetBubbleView::Init() {
  set_margins(gfx::Insets(kMarginHeight, 0, 0, 0));
  // Start requesting the feedback data.
  snapshot_.reset(new ResettableSettingsSnapshot(profile_));
  snapshot_->RequestShortcuts(
      base::Bind(&ProfileResetBubbleView::UpdateFeedbackDetails,
                 weak_factory_.GetWeakPtr()));
  SetupLayoutManager(true);
}

void ProfileResetBubbleView::SetupLayoutManager(bool report_checked) {
  ResetAllChildren();

  base::string16 product_name(
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Bubble title label.
  views::Label* title_label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_RESET_BUBBLE_TITLE, product_name),
      rb.GetFontList(ui::ResourceBundle::BoldFont));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Description text label.
  views::Label* text_label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_RESET_BUBBLE_TEXT, product_name));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetEnabledColor(SK_ColorDKGRAY);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Learn more link.
  views::Link* learn_more_link = new views::Link(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  learn_more_link->set_listener(this);
  learn_more_link->SetUnderline(false);

  // Reset button's name is based on |resetting_| state.
  int reset_button_string_id = IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON;
  if (resetting_)
    reset_button_string_id = IDS_RESETTING;
  controls_.reset_button = new views::LabelButton(
      this, l10n_util::GetStringUTF16(reset_button_string_id));
  controls_.reset_button->SetStyle(views::Button::STYLE_BUTTON);
  controls_.reset_button->SetIsDefault(true);
  controls_.reset_button->SetFontList(
      rb.GetFontList(ui::ResourceBundle::BoldFont));
  controls_.reset_button->SetEnabled(!resetting_);
  // For the Resetting... text to fit.
  gfx::Size reset_button_size = controls_.reset_button->GetPreferredSize();
  reset_button_size.set_width(100);
  controls_.reset_button->set_min_size(reset_button_size);

  // No thanks button.
  controls_.no_thanks_button = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_NO_THANKS));
  controls_.no_thanks_button->SetStyle(views::Button::STYLE_BUTTON);
  controls_.no_thanks_button->SetEnabled(!resetting_);

  // Checkbox for reporting settings or not.
  controls_.report_settings_checkbox = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_REPORT_BUBBLE_TEXT));
  controls_.report_settings_checkbox->SetTextColor(
      views::Button::STATE_NORMAL, SK_ColorGRAY);
  controls_.report_settings_checkbox->SetChecked(report_checked);
  controls_.report_settings_checkbox->SetTextMultiLine(true);
  controls_.report_settings_checkbox->set_background(
      views::Background::CreateSolidBackground(kLightGrayBackgroundColor));
  // Have a smaller margin on the right, to have the |controls_.help_button|
  // closer to the edge.
  controls_.report_settings_checkbox->SetBorder(
      views::Border::CreateSolidSidedBorder(kMarginWidth,
                                            kMarginWidth,
                                            kMarginWidth,
                                            kMarginWidth / 2,
                                            kLightGrayBackgroundColor));

  // Help button to toggle the bottom panel on or off.
  controls_.help_button = new views::ImageButton(this);
  const gfx::ImageSkia* help_image = rb.GetImageSkiaNamed(IDR_QUESTION_MARK);
  color_utils::HSL hsl_shift = { -1, 0, 0.8 };
  brighter_help_image_ = gfx::ImageSkiaOperations::CreateHSLShiftedImage(
      *help_image, hsl_shift);
  controls_.help_button->SetImage(
      views::Button::STATE_NORMAL, &brighter_help_image_);
  controls_.help_button->SetImage(views::Button::STATE_HOVERED, help_image);
  controls_.help_button->SetImage(views::Button::STATE_PRESSED, help_image);
  controls_.help_button->set_background(
      views::Background::CreateSolidBackground(kLightGrayBackgroundColor));
  controls_.help_button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                  views::ImageButton::ALIGN_MIDDLE);

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Title row.
  const int kTitleColumnSetId = 0;
  views::ColumnSet* cs = layout->AddColumnSet(kTitleColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kMarginWidth);

  // Text row.
  const int kTextColumnSetId = 1;
  cs = layout->AddColumnSet(kTextColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kWidthOfDescriptionText, 0);
  cs->AddPaddingColumn(0, kMarginWidth);

  // Learn more link & buttons row.
  const int kButtonsColumnSetId = 2;
  cs = layout->AddColumnSet(kButtonsColumnSetId);
  cs->AddPaddingColumn(0, kMarginWidth);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kButtonPadding);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kMarginWidth);

  // Separator.
  const int kSeparatorColumnSetId = 3;
  cs = layout->AddColumnSet(kSeparatorColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kAllColumnsWidth, 0);

  // Reporting row.
  const int kReportColumnSetId = 4;
  cs = layout->AddColumnSet(kReportColumnSetId);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kReportingCheckboxColumnWidth, 0);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                GridLayout::FIXED, kHelpButtonColumnWidth, 0);

  layout->StartRow(0, kTitleColumnSetId);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kMarginHeight);

  layout->StartRow(0, kTextColumnSetId);
  layout->AddView(text_label);
  layout->AddPaddingRow(0, kMarginHeight);

  layout->StartRow(0, kButtonsColumnSetId);
  layout->AddView(learn_more_link);
  layout->AddView(controls_.reset_button);
  layout->AddView(controls_.no_thanks_button);
  layout->AddPaddingRow(0, kMarginHeight);

  layout->StartRow(0, kSeparatorColumnSetId);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL));

  layout->StartRow(0, kReportColumnSetId);
  layout->AddView(controls_.report_settings_checkbox);
  layout->AddView(controls_.help_button);

  if (show_help_pane_ && snapshot_) {
    // We need a single row to add the scroll view containing the feedback.
    const int kReportDetailsColumnSetId = 5;
    cs = layout->AddColumnSet(kReportDetailsColumnSetId);
    cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                  GridLayout::USE_PREF, 0, 0);

    FeedbackView* feedback_view = new FeedbackView();
    scoped_ptr<base::ListValue> feedback_data =
        GetReadableFeedbackForSnapshot(profile_, *snapshot_);
    feedback_view->SetupLayoutManager(*feedback_data);

    views::ScrollView* scroll_view = new views::ScrollView();
    scroll_view->set_background(views::Background::CreateSolidBackground(
        kLightGrayBackgroundColor));
    scroll_view->SetContents(feedback_view);

    layout->StartRow(1, kReportDetailsColumnSetId);
    layout->AddView(scroll_view, 1, 1, GridLayout::FILL,
                    GridLayout::FILL, kAllColumnsWidth,
                    std::min(feedback_view->height() + kMarginHeight,
                             kMaxFeedbackViewHeight));
  }

  Layout();
  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

void ProfileResetBubbleView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == controls_.reset_button) {
    DCHECK(!resetting_);
    content::RecordAction(
        base::UserMetricsAction("SettingsResetBubble.Reset"));

    // Remember that the user chose to reset, and that resetting is underway.
    chose_to_reset_ = true;
    resetting_ = true;

    controls_.reset_button->SetText(l10n_util::GetStringUTF16(IDS_RESETTING));
    controls_.reset_button->SetEnabled(false);
    controls_.no_thanks_button->SetEnabled(false);
    SchedulePaint();

    if (global_error_) {
      global_error_->OnBubbleViewResetButtonPressed(
          controls_.report_settings_checkbox->checked());
    }
  } else if (sender == controls_.no_thanks_button) {
    DCHECK(!resetting_);
    content::RecordAction(
        base::UserMetricsAction("SettingsResetBubble.NoThanks"));

    if (global_error_)
      global_error_->OnBubbleViewNoThanksButtonPressed();
    GetWidget()->Close();
    return;
  } else if (sender == controls_.help_button) {
    show_help_pane_ = !show_help_pane_;

    SetupLayoutManager(controls_.report_settings_checkbox->checked());
    SizeToContents();
  }
}

void ProfileResetBubbleView::LinkClicked(views::Link* source, int flags) {
  content::RecordAction(
      base::UserMetricsAction("SettingsResetBubble.LearnMore"));
  navigator_->OpenURL(content::OpenURLParams(
      GURL(chrome::kResetProfileSettingsLearnMoreURL), content::Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false));
}

void ProfileResetBubbleView::CloseBubbleView() {
  resetting_ = false;
  GetWidget()->Close();
}

void ProfileResetBubbleView::UpdateFeedbackDetails() {
  if (show_help_pane_)
    SetupLayoutManager(controls_.report_settings_checkbox->checked());
}

bool IsProfileResetBubbleSupported() {
  return true;
}

GlobalErrorBubbleViewBase* ShowProfileResetBubble(
    const base::WeakPtr<ProfileResetGlobalError>& global_error,
    Browser* browser) {
  return ProfileResetBubbleView::ShowBubble(global_error, browser);
}
