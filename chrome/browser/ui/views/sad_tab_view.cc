// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/memory/oom_memory_details.h"
#endif

using content::OpenURLParams;
using content::WebContents;

namespace {

const int kMaxContentWidth = 600;
const int kMinColumnWidth = 120;
const char kCategoryTagCrash[] = "Crash";

void RecordKillCreated() {
  static int killed = 0;
  killed++;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Tabs.SadTab.KillCreated", killed, 1, 1000, 50);
}

void RecordKillDisplayed() {
  static int killed = 0;
  killed++;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Tabs.SadTab.KillDisplayed", killed, 1, 1000, 50);
}

#if defined(OS_CHROMEOS)
void RecordKillCreatedOOM() {
  static int oom_killed = 0;
  oom_killed++;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Tabs.SadTab.KillCreated.OOM", oom_killed, 1, 1000, 50);
}

void RecordKillDisplayedOOM() {
  static int oom_killed = 0;
  oom_killed++;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Tabs.SadTab.KillDisplayed.OOM", oom_killed, 1, 1000, 50);
}
#endif

}  // namespace

SadTabView::SadTabView(WebContents* web_contents, chrome::SadTabKind kind)
    : web_contents_(web_contents),
      kind_(kind),
      painted_(false),
      message_(nullptr),
      help_link_(nullptr),
      feedback_link_(nullptr),
      reload_button_(nullptr),
      title_(nullptr),
      help_message_(nullptr) {
  DCHECK(web_contents);

  // These stats should use the same counting approach and bucket size used for
  // tab discard events in memory::OomPriorityManager so they can be directly
  // compared.
  // TODO(jamescook): Maybe track time between sad tabs?
  switch (kind_) {
    case chrome::SAD_TAB_KIND_CRASHED: {
      static int crashed = 0;
      crashed++;
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Tabs.SadTab.CrashCreated", crashed, 1, 1000, 50);
      break;
    }
    case chrome::SAD_TAB_KIND_KILLED: {
      RecordKillCreated();
      LOG(WARNING) << "Tab Killed: "
                   <<  web_contents->GetURL().GetOrigin().spec();
      break;
    }
#if defined(OS_CHROMEOS)
    case chrome::SAD_TAB_KIND_KILLED_BY_OOM: {
      RecordKillCreated();
      RecordKillCreatedOOM();
      const std::string spec = web_contents->GetURL().GetOrigin().spec();
      memory::OomMemoryDetails::Log(
          "Tab OOM-Killed Memory details: " + spec + ", ", base::Closure());
      break;
    }
#endif
  }

  // Set the background color.
  set_background(
      views::Background::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_DialogBackground)));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddPaddingColumn(1, views::kPanelSubVerticalSpacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, kMinColumnWidth);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, kMinColumnWidth);
  columns->AddPaddingColumn(1, views::kPanelSubVerticalSpacing);

  views::ImageView* image = new views::ImageView();
  image->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_SAD_TAB));
  layout->AddPaddingRow(1, views::kPanelVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(image, 2, 1);

  const bool is_crashed_type = kind_ == chrome::SAD_TAB_KIND_CRASHED;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  title_ = CreateLabel(l10n_util::GetStringUTF16(
      is_crashed_type ? IDS_SAD_TAB_TITLE : IDS_KILLED_TAB_TITLE));
  title_->SetFontList(rb.GetFontList(ui::ResourceBundle::LargeFont));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, column_set_id, 0,
                              views::kPanelVerticalSpacing);
  layout->AddView(title_, 2, 1);

  const SkColor text_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor);

  int message_id =
      is_crashed_type ? IDS_SAD_TAB_MESSAGE : IDS_KILLED_TAB_MESSAGE;

#if defined(OS_CHROMEOS)
  if (kind_ == chrome::SAD_TAB_KIND_KILLED_BY_OOM)
    message_id = IDS_KILLED_TAB_BY_OOM_MESSAGE;
#endif

  message_ = CreateLabel(l10n_util::GetStringUTF16(message_id));

  message_->SetMultiLine(true);
  message_->SetEnabledColor(text_color);
  message_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_->SetLineHeight(views::kPanelSubVerticalSpacing);

  layout->StartRowWithPadding(0, column_set_id, 0, views::kPanelVertMargin);
  layout->AddView(message_, 2, 1, views::GridLayout::LEADING,
                  views::GridLayout::LEADING);

  if (web_contents_) {
    reload_button_ = new views::BlueButton(
        this, l10n_util::GetStringUTF16(IDS_SAD_TAB_RELOAD_LABEL));

    if (is_crashed_type) {
      size_t offset = 0;
      base::string16 help_text(
          l10n_util::GetStringFUTF16(IDS_SAD_TAB_HELP_MESSAGE,
                                     base::string16(), &offset));

      base::string16 link_text =
          l10n_util::GetStringUTF16(IDS_SAD_TAB_HELP_LINK);

      base::string16 help_prefix = help_text.substr(0, offset);
      base::string16 help_suffix = help_text.substr(offset);
      base::string16 help_message_string = help_prefix;
      help_message_string.append(link_text).append(help_suffix);

      help_message_ = new views::StyledLabel(help_message_string, this);

      views::StyledLabel::RangeStyleInfo link_style =
          views::StyledLabel::RangeStyleInfo::CreateForLink();
      link_style.font_style = gfx::Font::UNDERLINE;
      link_style.color = text_color;

      views::StyledLabel::RangeStyleInfo normal_style =
          views::StyledLabel::RangeStyleInfo();
      normal_style.color = text_color;

      help_message_->SetDefaultStyle(normal_style);
      help_message_->SetLineHeight(views::kPanelSubVerticalSpacing);

      help_message_->AddStyleRange(
          gfx::Range(help_prefix.length(),
                     help_prefix.length() + link_text.length()),
          link_style);

      layout->StartRowWithPadding(0, column_set_id, 0, views::kPanelVertMargin);
      layout->AddView(help_message_, 2, 1, views::GridLayout::LEADING,
                      views::GridLayout::TRAILING);
      layout->StartRowWithPadding(0, column_set_id, 0,
                                  views::kPanelVerticalSpacing);
      layout->SkipColumns(1);
    } else {
      feedback_link_ = CreateLink(
          l10n_util::GetStringUTF16(IDS_KILLED_TAB_FEEDBACK_LINK), text_color);
      feedback_link_->SetLineHeight(views::kPanelSubVerticalSpacing);
      feedback_link_->SetMultiLine(true);
      feedback_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

      layout->StartRowWithPadding(0, column_set_id, 0,
                                  views::kPanelSubVerticalSpacing);
      layout->AddView(feedback_link_, 2, 1, views::GridLayout::LEADING,
                      views::GridLayout::LEADING);

      help_link_ =
          CreateLink(l10n_util::GetStringUTF16(IDS_LEARN_MORE), text_color);
      layout->StartRowWithPadding(0, column_set_id, 0,
                                  views::kPanelVerticalSpacing);
      layout->AddView(help_link_, 1, 1, views::GridLayout::LEADING,
                      views::GridLayout::CENTER);
    }
    layout->AddView(reload_button_, 1, 1, views::GridLayout::TRAILING,
                    views::GridLayout::LEADING);
  }
  layout->AddPaddingRow(2, views::kPanelSubVerticalSpacing);
}

SadTabView::~SadTabView() {}

void SadTabView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK(web_contents_);
  if (source == help_link_) {
    GURL help_url((kind_ == chrome::SAD_TAB_KIND_CRASHED) ?
        chrome::kCrashReasonURL : chrome::kKillReasonURL);
    OpenURLParams params(
        help_url, content::Referrer(), CURRENT_TAB,
        ui::PAGE_TRANSITION_LINK, false);
    web_contents_->OpenURL(params);
  } else if (source == feedback_link_) {
    chrome::ShowFeedbackPage(
        chrome::FindBrowserWithWebContents(web_contents_),
        l10n_util::GetStringUTF8(IDS_KILLED_TAB_FEEDBACK_MESSAGE),
        std::string(kCategoryTagCrash));
  }
}

void SadTabView::StyledLabelLinkClicked(const gfx::Range& range,
                                        int event_flags) {
  LinkClicked(help_link_, event_flags);
}

void SadTabView::ButtonPressed(views::Button* sender,
                               const ui::Event& event) {
  DCHECK(web_contents_);
  DCHECK_EQ(reload_button_, sender);
  web_contents_->GetController().Reload(true);
}

void SadTabView::Layout() {
  // Specify the maximum message width explicitly.
  const int max_width =
      std::min(width() - views::kPanelSubVerticalSpacing * 2, kMaxContentWidth);
  message_->SizeToFit(max_width);
  title_->SizeToFit(max_width);

  if (feedback_link_ != nullptr)
    feedback_link_->SizeToFit(max_width);

  if (help_message_ != nullptr)
    help_message_->SizeToFit(max_width);

  View::Layout();
}

void SadTabView::OnPaint(gfx::Canvas* canvas) {
  if (!painted_) {
    // These stats should use the same counting approach and bucket size used
    // for tab discard events in memory::OomPriorityManager so they can be
    // directly compared.
    switch (kind_) {
      case chrome::SAD_TAB_KIND_CRASHED: {
        static int crashed = 0;
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Tabs.SadTab.CrashDisplayed", ++crashed, 1, 1000, 50);
        break;
      }
      case chrome::SAD_TAB_KIND_KILLED:
        RecordKillDisplayed();
        break;
#if defined(OS_CHROMEOS)
      case chrome::SAD_TAB_KIND_KILLED_BY_OOM:
        RecordKillDisplayed();
        RecordKillDisplayedOOM();
        break;
#endif
    }
    painted_ = true;
  }
  View::OnPaint(canvas);
}

void SadTabView::Show() {
  views::Widget::InitParams sad_tab_params(
      views::Widget::InitParams::TYPE_CONTROL);

  // It is not possible to create a native_widget_win that has no parent in
  // and later re-parent it.
  // TODO(avi): This is a cheat. Can this be made cleaner?
  sad_tab_params.parent = web_contents_->GetNativeView();

  set_owned_by_client();

  views::Widget* sad_tab = new views::Widget;
  sad_tab->Init(sad_tab_params);
  sad_tab->SetContentsView(this);

  views::Widget::ReparentNativeView(sad_tab->GetNativeView(),
                                    web_contents_->GetNativeView());
  gfx::Rect bounds = web_contents_->GetContainerBounds();
  sad_tab->SetBounds(gfx::Rect(bounds.size()));
}

void SadTabView::Close() {
  if (GetWidget())
    GetWidget()->Close();
}

views::Label* SadTabView::CreateLabel(const base::string16& text) {
  views::Label* label = new views::Label(text);
  label->SetBackgroundColor(background()->get_color());
  return label;
}

views::Link* SadTabView::CreateLink(const base::string16& text,
                                    const SkColor& color) {
  views::Link* link = new views::Link(text);
  link->SetBackgroundColor(background()->get_color());
  link->SetEnabledColor(color);
  link->set_listener(this);
  return link;
}

namespace chrome {

SadTab* SadTab::Create(content::WebContents* web_contents,
                       SadTabKind kind) {
  return new SadTabView(web_contents, kind);
}

}  // namespace chrome
