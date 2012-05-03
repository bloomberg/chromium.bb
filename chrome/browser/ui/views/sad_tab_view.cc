// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"

using content::OpenURLParams;
using content::WebContents;

namespace {

const int kPadding = 20;
const float kMessageSize = 0.65f;
const SkColor kTextColor = SK_ColorWHITE;
const SkColor kCrashColor = SkColorSetRGB(35, 48, 64);
const SkColor kKillColor = SkColorSetRGB(57, 48, 88);

const char kCategoryTagCrash[] = "Crash";

// Font size correction.
#if defined(CROS_FONTS_USING_BCI)
const int kTitleFontSizeDelta = 1;
const int kMessageFontSizeDelta = 0;
#else
const int kTitleFontSizeDelta = 2;
const int kMessageFontSizeDelta = 1;
#endif

// Name of the experiment to run.
const char kExperiment[] = "LowMemoryMargin";

#define EXPERIMENT_CUSTOM_COUNTS(name, sample, min, max, buckets)          \
    UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, buckets);          \
    if (base::FieldTrialList::TrialExists(kExperiment))                    \
      UMA_HISTOGRAM_CUSTOM_COUNTS(                                         \
          base::FieldTrial::MakeName(name, kExperiment),                   \
          sample, min, max, buckets);

}  // namespace

SadTabView::SadTabView(WebContents* web_contents, Kind kind)
    : web_contents_(web_contents),
      kind_(kind),
      painted_(false),
      base_font_(ui::ResourceBundle::GetSharedInstance().GetFont(
          ui::ResourceBundle::BaseFont)),
      message_(NULL),
      help_link_(NULL),
      feedback_link_(NULL),
      reload_button_(NULL) {
  DCHECK(web_contents);

  // Sometimes the user will never see this tab, so keep track of the total
  // number of creation events to compare to display events.
  // TODO(jamescook): Remove this after R20 stable.  Keep it for now so we can
  // compare R20 to earlier versions.
  UMA_HISTOGRAM_COUNTS("SadTab.Created", kind_);

  // These stats should use the same counting approach and bucket size used for
  // tab discard events in chrome/browser/oom_priority_manager.cc so they can be
  // directly compared.
  // TODO(jamescook): Maybe track time between sad tabs?
  switch (kind_) {
    case CRASHED: {
      static int crashed = 0;
      crashed++;
      EXPERIMENT_CUSTOM_COUNTS(
          "Tabs.SadTab.CrashCreated", crashed, 1, 1000, 50);
      break;
    }
    case KILLED: {
      static int killed = 0;
      killed++;
      EXPERIMENT_CUSTOM_COUNTS(
          "Tabs.SadTab.KillCreated", killed, 1, 1000, 50);
      break;
    }
    default:
      NOTREACHED();
  }

  // Set the background color.
  set_background(views::Background::CreateSolidBackground(
      (kind_ == CRASHED) ? kCrashColor : kKillColor));
}

SadTabView::~SadTabView() {}

void SadTabView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK(web_contents_);
  if (source == help_link_) {
    GURL help_url(
        kind_ == CRASHED ? chrome::kCrashReasonURL : chrome::kKillReasonURL);
    OpenURLParams params(
        help_url, content::Referrer(), CURRENT_TAB,
        content::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params);
  } else if (source == feedback_link_) {
    browser::ShowWebFeedbackView(
        Browser::GetBrowserForController(&web_contents_->GetController(), NULL),
        l10n_util::GetStringUTF8(IDS_KILLED_TAB_FEEDBACK_MESSAGE),
        std::string(kCategoryTagCrash));
  }
}

void SadTabView::ButtonPressed(views::Button* source,
                               const views::Event& event) {
  DCHECK(web_contents_);
  DCHECK(source == reload_button_);
  web_contents_->GetController().Reload(true);
}

void SadTabView::Layout() {
  // Specify the maximum message width explicitly.
  message_->SizeToFit(static_cast<int>(width() * kMessageSize));
  View::Layout();
}

void SadTabView::ViewHierarchyChanged(bool is_add,
                                      views::View* parent,
                                      views::View* child) {
  if (child != this || !is_add)
    return;

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddPaddingColumn(1, kPadding);
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, kPadding);

  views::ImageView* image = new views::ImageView();
  image->SetImage(ui::ResourceBundle::GetSharedInstance().GetBitmapNamed(
      (kind_ == CRASHED) ? IDR_SAD_TAB : IDR_KILLED_TAB));
  layout->StartRowWithPadding(0, column_set_id, 1, kPadding);
  layout->AddView(image);

  views::Label* title = CreateLabel(l10n_util::GetStringUTF16(
      (kind_ == CRASHED) ? IDS_SAD_TAB_TITLE : IDS_KILLED_TAB_TITLE));
  title->SetFont(base_font_.DeriveFont(kTitleFontSizeDelta, gfx::Font::BOLD));
  layout->StartRowWithPadding(0, column_set_id, 0, kPadding);
  layout->AddView(title);

  message_ = CreateLabel(l10n_util::GetStringUTF16(
      (kind_ == CRASHED) ? IDS_SAD_TAB_MESSAGE : IDS_KILLED_TAB_MESSAGE));
  message_->SetMultiLine(true);
  layout->StartRowWithPadding(0, column_set_id, 0, kPadding);
  layout->AddView(message_);

  if (web_contents_) {
    layout->StartRowWithPadding(0, column_set_id, 0, kPadding);
    reload_button_ = new views::TextButton(
        this,
        l10n_util::GetStringUTF16(IDS_SAD_TAB_RELOAD_LABEL));
    reload_button_->set_border(new views::TextButtonNativeThemeBorder(
        reload_button_));
    layout->AddView(reload_button_);

    help_link_ = CreateLink(l10n_util::GetStringUTF16(
        (kind_ == CRASHED) ? IDS_SAD_TAB_HELP_LINK : IDS_LEARN_MORE));

    if (kind_ == CRASHED) {
      size_t offset = 0;
      string16 help_text(l10n_util::GetStringFUTF16(IDS_SAD_TAB_HELP_MESSAGE,
                                                    string16(), &offset));
      views::Label* help_prefix = CreateLabel(help_text.substr(0, offset));
      views::Label* help_suffix = CreateLabel(help_text.substr(offset));

      const int help_column_set_id = 1;
      views::ColumnSet* help_columns = layout->AddColumnSet(help_column_set_id);
      help_columns->AddPaddingColumn(1, kPadding);
      // Center three middle columns for the help's [prefix][link][suffix].
      for (size_t column = 0; column < 3; column++)
        help_columns->AddColumn(views::GridLayout::CENTER,
            views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
      help_columns->AddPaddingColumn(1, kPadding);

      layout->StartRowWithPadding(0, help_column_set_id, 0, kPadding);
      layout->AddView(help_prefix);
      layout->AddView(help_link_);
      layout->AddView(help_suffix);
    } else {
      layout->StartRowWithPadding(0, column_set_id, 0, kPadding);
      layout->AddView(help_link_);

      feedback_link_ = CreateLink(
          l10n_util::GetStringUTF16(IDS_KILLED_TAB_FEEDBACK_LINK));
      layout->StartRowWithPadding(0, column_set_id, 0, kPadding);
      layout->AddView(feedback_link_);
    }
  }
  layout->AddPaddingRow(1, kPadding);
}

void SadTabView::OnPaint(gfx::Canvas* canvas) {
  if (!painted_) {
    // User actually saw the error, keep track for user experience stats.
    // TODO(jamescook): Remove this after R20 stable.  Keep it for now so we can
    // compare R20 to earlier versions.
    UMA_HISTOGRAM_COUNTS("SadTab.Displayed", kind_);

    // These stats should use the same counting approach and bucket size used
    // for tab discard events in chrome/browser/oom_priority_manager.cc so they
    // can be directly compared.
    switch (kind_) {
      case CRASHED: {
        static int crashed = 0;
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Tabs.SadTab.CrashDisplayed", ++crashed, 1, 1000, 50);
        break;
      }
      case KILLED: {
        static int killed = 0;
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Tabs.SadTab.KillDisplayed", ++killed, 1, 1000, 50);
        break;
      }
      default:
        NOTREACHED();
    }
    painted_ = true;
  }
  View::OnPaint(canvas);
}

views::Label* SadTabView::CreateLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetFont(base_font_.DeriveFont(kMessageFontSizeDelta));
  label->SetBackgroundColor(background()->get_color());
  label->SetEnabledColor(kTextColor);
  return label;
}

views::Link* SadTabView::CreateLink(const string16& text) {
  views::Link* link = new views::Link(text);
  link->SetFont(base_font_.DeriveFont(kMessageFontSizeDelta));
  link->SetBackgroundColor(background()->get_color());
  link->SetEnabledColor(kTextColor);
  link->set_listener(this);
  return link;
}
