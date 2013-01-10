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
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view_text_utils.h"
#include "ui/views/widget/widget.h"

using content::UserMetricsAction;

namespace {

// Layout constants.
const int kExtensionListPadding = 10;
const int kLeftColumnPadding = 3;
const int kInsetBottomRight = 13;
const int kInsetLeft = 14;
const int kInsetTop = 9;
const int kHeadlineMessagePadding = 4;
const int kHeadlineRowPadding = 10;
const int kMessageBubblePadding = 11;

// How often to show the disabled extension (sideload wipeout) bubble.
const int kShowSideloadWipeoutBubbleMax = 3;

// How many extensions to show in the bubble (max).
const size_t kMaxExtensionsToShow = 7;

// How long to wait until showing the bubble (in seconds).
const int kBubbleAppearanceWaitTime = 5;

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

DisabledExtensionsView::DisabledExtensionsView(
    views::View* anchor_view,
    Browser* browser,
    const ExtensionSet* wiped_out)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      browser_(browser),
      wiped_out_(wiped_out),
      headline_(NULL),
      learn_more_(NULL),
      settings_button_(NULL),
      dismiss_button_(NULL),
      recourse_(NULL) {
  set_close_on_deactivate(false);
  set_move_with_anchor(true);
  set_close_on_esc(true);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_insets(gfx::Insets(5, 0, 5, 0));

  UMA_HISTOGRAM_COUNTS_100("DisabledExtension.SideloadWipeoutCount",
                           wiped_out->size());
}

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
  if (sideload_wipeout_bubble_shown.GetValue() >= kShowSideloadWipeoutBubbleMax)
    return;

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
    bubble_delegate->ShowWhenReady();

    done_showing_ui = true;
  }
}

void DisabledExtensionsView::ShowWhenReady() {
  // Not showing the bubble right away (during startup) has a few benefits:
  // We don't have to worry about focus being lost due to the Omnibox (or to
  // other things that want focus at startup). This allows Esc to work to close
  // the bubble and also solves the keyboard accessibility problem that comes
  // with focus being lost (we don't have a good generic mechanism of injecting
  // bubbles into the focus cycle). Another benefit of delaying the show is
  // that fade-in works (the fade-in isn't apparent if the the bubble appears at
  // startup).
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DisabledExtensionsView::ShowBubble,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kBubbleAppearanceWaitTime));
}

////////////////////////////////////////////////////////////////////////////////
// DisabledExtensionsView - private.

DisabledExtensionsView::~DisabledExtensionsView() {
}

void DisabledExtensionsView::ShowBubble() {
  StartFade(true);

  IntegerPrefMember sideload_wipeout_bubble_shown;
  sideload_wipeout_bubble_shown.Init(
      prefs::kExtensionsSideloadWipeoutBubbleShown,
          browser_->profile()->GetPrefs());
  sideload_wipeout_bubble_shown.SetValue(
      sideload_wipeout_bubble_shown.GetValue() + 1);
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

void DisabledExtensionsView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(kInsetTop, kInsetLeft,
                    kInsetBottomRight, kInsetBottomRight);
  SetLayoutManager(layout);

  const int headline_column_set_id = 0;
  views::ColumnSet* top_columns = layout->AddColumnSet(headline_column_set_id);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(1, 0);
  layout->StartRow(0, headline_column_set_id);

  headline_ = new views::Label();
  headline_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  headline_->SetText(l10n_util::GetStringUTF16(
      IDS_OPTIONS_SIDELOAD_WIPEOUT_BUBBLE_HEADLINE));
  layout->AddView(headline_);

  layout->AddPaddingRow(0, kHeadlineRowPadding);

  const int text_column_set_id = 1;
  views::ColumnSet* upper_columns = layout->AddColumnSet(text_column_set_id);
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
  middle_columns->AddPaddingColumn(0, kExtensionListPadding);
  middle_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRowWithPadding(0, extension_list_column_set_id,
      0, kHeadlineMessagePadding);
  views::Label* extensions = new views::Label();
  extensions->SetMultiLine(true);
  extensions->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  std::vector<string16> extension_list;
  size_t count = 0;
  char16 bullet_point = 0x2022;
  for (ExtensionSet::const_iterator iter = wiped_out_->begin();
       iter != wiped_out_->end() && count < kMaxExtensionsToShow; ++iter) {
    const extensions::Extension* extension = *iter;
    // Add each extension with bullet point.
    extension_list.push_back(bullet_point + ASCIIToUTF16(" ") +
                             UTF8ToUTF16(extension->name()));
    count++;
  }

  if (wiped_out_->size() > kMaxExtensionsToShow) {
    string16 difference =
        base::IntToString16(wiped_out_->size() - kMaxExtensionsToShow);
    extension_list.push_back(bullet_point + ASCIIToUTF16(" ") +
        l10n_util::GetStringFUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_OVERFLOW,
                                   difference));
  }

  extensions->SetText(JoinString(extension_list, ASCIIToUTF16("\n")));
  extensions->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_DISABLED_EXTENSIONS_BUBBLE_WIDTH_CHARS));
  layout->AddView(extensions);

  layout->StartRowWithPadding(
      0, text_column_set_id, 0, kHeadlineMessagePadding);
  recourse_ = new views::Label();
  recourse_->SetMultiLine(true);
  recourse_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  recourse_->SetText(
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_RECOURSE));
  recourse_->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_DISABLED_EXTENSIONS_BUBBLE_WIDTH_CHARS));
  layout->AddView(recourse_);

  const int action_row_column_set_id = 3;
  views::ColumnSet* bottom_columns =
      layout->AddColumnSet(action_row_column_set_id);
  bottom_columns->AddPaddingColumn(1, 0);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, 7);
  layout->StartRowWithPadding(0, action_row_column_set_id,
                              0, kMessageBubblePadding);

  learn_more_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_->set_listener(this);
  // We need to directly control where this link appears, so we don't add it to
  // the layout manager.
  AddChildView(learn_more_);

  settings_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_SETTINGS));
  layout->AddView(settings_button_);
  dismiss_button_ = new views::NativeTextButton(this,
      l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_DISMISS));
  layout->AddView(dismiss_button_);

  content::RecordAction(
      UserMetricsAction("DisabledExtensionNotificationShown"));
}

void DisabledExtensionsView::Paint(gfx::Canvas* canvas) {
  views::View::Paint(canvas);

  if (!link_offset_.width() && !link_offset_.height()) {
    // The Learn More link should appear right after the recourse_ label. To
    // facilitate that, the offset of the Learn More link needs to be
    // calculated (once only, since the text in the bubble never changes). This
    // is done during the first Paint call since the label needs to be drawn
    // manually on the canvas to figure out where exactly it ends so that the
    // link can be placed right at its end.
    gfx::Font font =
        ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
    gfx::Size position;
    views::Label label;
    gfx::Rect bounds = gfx::Rect(
        views::Widget::GetLocalizedContentsWidth(
            IDS_DISABLED_EXTENSIONS_BUBBLE_WIDTH_CHARS),
        recourse_->height());
    view_text_utils::DrawTextAndPositionUrl(
        canvas,
        &label,
        l10n_util::GetStringUTF16(IDS_OPTIONS_SIDELOAD_WIPEOUT_RECOURSE),
        learn_more_,
        NULL,
        &link_offset_,
        base::i18n::IsRTL(),
        bounds,
        font);
    // Now that we have the offset, the layout needs to be recalculated.
    Layout();
  }
}

void DisabledExtensionsView::Layout() {
  views::View::Layout();

  // If the right link offset is known, place the link in the right location.
  if (link_offset_.width() || link_offset_.height()) {
    const gfx::Font font;
    const int space_width = font.GetStringWidth(ASCIIToUTF16(" "));
    gfx::Size sz = learn_more_->GetPreferredSize();
    learn_more_->SetBounds(recourse_->bounds().x() +
                               link_offset_.width() +
                               space_width,
                           recourse_->bounds().y() + link_offset_.height() - 1,
                           sz.width(),
                           sz.height());
  }
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
