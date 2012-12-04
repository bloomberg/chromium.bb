// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/disabled_extensions_view.h"

#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using content::UserMetricsAction;

namespace {

// Layout constants.
const int kColumnPadding = 4;
const int kExtensionListPadding = 20;
const int kImagePadding = 7;
const int kLeftColumnPadding = 3;
const int kInsetBottomRight = 13;
const int kInsetTopLeft = 14;
const int kHeadlineMessagePadding = 4;
const int kHeadlineRowPadding = 10;
const int kMessageBubblePadding = 11;

// How often to show the disabled extension (sideload wipeout) bubble.
const int kShowSideloadWipeoutBubbleMax = 3;

// How many extensions to show in the bubble (max).
const int kMaxExtensionsToShow = 7;

// UMA histogram constants.
enum UmaWipeoutHistogramOptions {
  ACTION_LEARN_MORE = 0,
  ACTION_SETTINGS_PAGE,
  ACTION_DISMISS,
  ACTION_BOUNDARY, // Must be the last value.
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisabledExtensionsView

// static
void DisabledExtensionsView::MaybeShow(Browser* browser,
                                       views::View* anchor_view) {
  if (!extensions::FeatureSwitch::sideload_wipeout()->IsEnabled())
    return;

  static bool done_showing_ui = false;
  if (done_showing_ui)
    return;  // Only show the bubble once per launch.

  // A pref that counts how often the bubble has been shown.
  IntegerPrefMember sideload_wipeout_bubble_shown;

  sideload_wipeout_bubble_shown.Init(
      prefs::kExtensionsSideloadWipeoutBubbleShown,
          browser->profile()->GetPrefs());
  int bubble_shown_count = sideload_wipeout_bubble_shown.GetValue();
  if (bubble_shown_count >= kShowSideloadWipeoutBubbleMax)
    return;
  sideload_wipeout_bubble_shown.SetValue(++bubble_shown_count);

  // Fetch all disabled extensions.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(
          browser->profile())->extension_service();
  scoped_ptr<const ExtensionSet> wiped_out(
      extension_service->GetWipedOutExtensions());
  UMA_HISTOGRAM_BOOLEAN("DisabledExtension.SideloadWipeoutNeeded",
                        wiped_out->size() > 0);
  if (wiped_out->size()) {
    DisabledExtensionsView* bubble_delegate =
        new DisabledExtensionsView(
            anchor_view, browser, wiped_out.release());
    views::BubbleDelegateView::CreateBubble(bubble_delegate);
    bubble_delegate->StartFade(true);

    done_showing_ui = true;
  }
}

DisabledExtensionsView::DisabledExtensionsView(
    views::View* anchor_view,
    Browser* browser,
    const ExtensionSet* wiped_out)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      wiped_out_(wiped_out),
      headline_(NULL),
      learn_more_(NULL),
      settings_button_(NULL),
      dismiss_button_(NULL) {
  set_close_on_deactivate(false);
  set_move_with_anchor(true);

  UMA_HISTOGRAM_COUNTS_100("DisabledExtension.SideloadWipeoutCount",
                           wiped_out->size());
}

DisabledExtensionsView::~DisabledExtensionsView() {
}

void DisabledExtensionsView::DismissBubble() {
  IntegerPrefMember sideload_wipeout_bubble_shown;
  sideload_wipeout_bubble_shown.Init(
      prefs::kExtensionsSideloadWipeoutBubbleShown,
      browser_->profile()->GetPrefs());
  sideload_wipeout_bubble_shown.SetValue(kShowSideloadWipeoutBubbleMax);

  GetWidget()->Close();
  content::RecordAction(
      UserMetricsAction("DisabledExtensionNotificationDismissed"));
}

void DisabledExtensionsView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == settings_button_) {
    UMA_HISTOGRAM_ENUMERATION("DisabledExtension.UserSelection",
                              ACTION_SETTINGS_PAGE, ACTION_BOUNDARY);
    browser_->OpenURL(
        content::OpenURLParams(GURL(chrome::kChromeUIExtensionsURL),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               content::PAGE_TRANSITION_LINK,
                               false));
  } else {
    DCHECK_EQ(dismiss_button_, sender);
    UMA_HISTOGRAM_ENUMERATION("DisabledExtension.UserSelection",
                              ACTION_DISMISS, ACTION_BOUNDARY);
  }

  DismissBubble();
}

void DisabledExtensionsView::LinkClicked(views::Link* source,
                                         int event_flags) {
  UMA_HISTOGRAM_ENUMERATION("DisabledExtension.UserSelection",
                            ACTION_LEARN_MORE, ACTION_BOUNDARY);
  browser_->OpenURL(
      content::OpenURLParams(GURL(chrome::kSideloadWipeoutHelpURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_LINK,
                             false));

  DismissBubble();
}

void DisabledExtensionsView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

void DisabledExtensionsView::ViewHierarchyChanged(bool is_add,
                                                  View* parent,
                                                  View* child) {
  if (is_add && child == this) {
    GetWidget()->NotifyAccessibilityEvent(
        this, ui::AccessibilityTypes::EVENT_ALERT, true);
  }
}

void DisabledExtensionsView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(kInsetTopLeft, kInsetTopLeft,
                    kInsetBottomRight, kInsetBottomRight);
  SetLayoutManager(layout);

  const int headline_column_set_id = 0;
  views::ColumnSet* top_columns = layout->AddColumnSet(headline_column_set_id);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(0, kImagePadding);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(1, 0);
  layout->StartRow(0, headline_column_set_id);

  views::ImageView* image = new views::ImageView();
  const gfx::ImageSkia* puzzle_piece =
      rb.GetImageSkiaNamed(IDR_EXTENSIONS_PUZZLE_PIECE);
  image->SetImage(puzzle_piece);
  layout->AddView(image);
  int left_padding_column = puzzle_piece->width() + kLeftColumnPadding;

  headline_ = new views::Label();
  headline_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  headline_->SetText(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SIDELOAD_WIPEOUT_BUBBLE_HEADLINE));
  layout->AddView(headline_);

  layout->AddPaddingRow(0, kHeadlineRowPadding);

  const int text_column_set_id = 1;
  views::ColumnSet* upper_columns = layout->AddColumnSet(text_column_set_id);
  upper_columns->AddPaddingColumn(0, left_padding_column);
  upper_columns->AddPaddingColumn(0, kColumnPadding);
  upper_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, text_column_set_id);

  views::Label* message = new views::Label();
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetText(
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_WHAT_HAPPENED));
  message->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_DISABLED_EXTENSIONS_BUBBLE_WIDTH_CHARS));
  layout->AddView(message);

  const int extension_list_column_set_id = 2;
  views::ColumnSet* middle_columns =
      layout->AddColumnSet(extension_list_column_set_id);
  middle_columns->AddPaddingColumn(0, left_padding_column);
  middle_columns->AddPaddingColumn(0, kExtensionListPadding);
  middle_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRowWithPadding(0, extension_list_column_set_id,
      0, kHeadlineMessagePadding);
  views::Label* extensions = new views::Label();
  extensions->SetMultiLine(true);
  extensions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  extensions->SetFont(extensions->font().DeriveFont(0, gfx::Font::ITALIC));

  std::vector<string16> extension_list;
  int count = 0;
  for (ExtensionSet::const_iterator iter = wiped_out_->begin();
       iter != wiped_out_->end() && count < kMaxExtensionsToShow; ++iter) {
    const extensions::Extension* extension = *iter;
    extension_list.push_back(ASCIIToUTF16("- ") +
                             UTF8ToUTF16(extension->name()));
    count++;
  }

  if (count == kMaxExtensionsToShow) {
    string16 difference =
        base::IntToString16(wiped_out_->size() - kMaxExtensionsToShow);
    extension_list.push_back(ASCIIToUTF16("- ") +
        l10n_util::GetStringFUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_OVERFLOW,
                                   difference));
  }

  extensions->SetText(JoinString(extension_list, ASCIIToUTF16("\n")));
  extensions->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_DISABLED_EXTENSIONS_BUBBLE_WIDTH_CHARS));
  layout->AddView(extensions);

  layout->StartRowWithPadding(
      0, text_column_set_id, 0, kHeadlineMessagePadding);
  views::Label* recourse = new views::Label();
  recourse->SetMultiLine(true);
  recourse->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  recourse->SetText(
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_RECOURSE));
  recourse->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_DISABLED_EXTENSIONS_BUBBLE_WIDTH_CHARS));
  layout->AddView(recourse);

  const int action_row_column_set_id = 3;
  views::ColumnSet* bottom_columns =
      layout->AddColumnSet(action_row_column_set_id);
  bottom_columns->AddPaddingColumn(0, left_padding_column);
  bottom_columns->AddPaddingColumn(0, kColumnPadding);
  bottom_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(1, views::kRelatedButtonHSpacing);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRowWithPadding(0, action_row_column_set_id,
                              0, kMessageBubblePadding);

  learn_more_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_->set_listener(this);
  layout->AddView(learn_more_);

  settings_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_SETTINGS));
  settings_button_->SetIsDefault(true);
  layout->AddView(settings_button_);
  dismiss_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_DISMISS));
  dismiss_button_->SetFont(
      dismiss_button_->font().DeriveFont(0, gfx::Font::BOLD));
  layout->AddView(dismiss_button_);

  content::RecordAction(
      UserMetricsAction("DisabledExtensionNotificationShown"));
}
