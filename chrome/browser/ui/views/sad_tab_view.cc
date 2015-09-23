// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/metrics/histogram.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
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
const int kCrashesBeforeFeedbackIsDisplayed = 1;

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

int SadTabView::total_crashes_ = 0;

SadTabView::SadTabView(WebContents* web_contents, chrome::SadTabKind kind)
    : web_contents_(web_contents),
      kind_(kind),
      painted_(false),
      message_(nullptr),
      help_link_(nullptr),
      action_button_(nullptr),
      title_(nullptr),
      help_message_(nullptr) {
  DCHECK(web_contents);

  // These stats should use the same counting approach and bucket size used for
  // tab discard events in memory::OomPriorityManager so they can be directly
  // compared.
  // TODO(jamescook): Maybe track time between sad tabs?
  total_crashes_++;

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

  image->SetImage(gfx::CreateVectorIcon(gfx::VectorIconId::CRASHED_TAB, 48,
                                        gfx::kChromeIconGrey));
  layout->AddPaddingRow(1, views::kPanelVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(image, 2, 1);

  title_ = CreateLabel(l10n_util::GetStringUTF16(IDS_SAD_TAB_TITLE));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetFontList(rb.GetFontList(ui::ResourceBundle::LargeFont));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, column_set_id, 0,
                              views::kPanelVerticalSpacing);
  layout->AddView(title_, 2, 1);

  const SkColor text_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor);

  int message_id = IDS_SAD_TAB_MESSAGE;
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
    // In the cases of multiple crashes in a session the 'Feedback' button
    // replaces the 'Reload' button as primary action.
    int button_type = total_crashes_ > kCrashesBeforeFeedbackIsDisplayed ?
        SAD_TAB_BUTTON_FEEDBACK : SAD_TAB_BUTTON_RELOAD;
    action_button_ = new views::BlueButton(this,
        l10n_util::GetStringUTF16(button_type == SAD_TAB_BUTTON_FEEDBACK
                                  ? IDS_CRASHED_TAB_FEEDBACK_LINK
                                  : IDS_SAD_TAB_RELOAD_LABEL));
    action_button_->set_tag(button_type);
    help_link_ =
        CreateLink(l10n_util::GetStringUTF16(IDS_LEARN_MORE), text_color);
    layout->StartRowWithPadding(0, column_set_id, 0,
                                views::kPanelVerticalSpacing);
    layout->AddView(help_link_, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
    layout->AddView(action_button_, 1, 1, views::GridLayout::TRAILING,
                    views::GridLayout::LEADING);
  }
  layout->AddPaddingRow(2, views::kPanelSubVerticalSpacing);
}

SadTabView::~SadTabView() {}

void SadTabView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK(web_contents_);
  OpenURLParams params(GURL(total_crashes_ > kCrashesBeforeFeedbackIsDisplayed ?
                       chrome::kCrashReasonFeedbackDisplayedURL :
                       chrome::kCrashReasonURL), content::Referrer(),
                       CURRENT_TAB, ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
}

void SadTabView::ButtonPressed(views::Button* sender,
                               const ui::Event& event) {
  DCHECK(web_contents_);
  DCHECK_EQ(action_button_, sender);

  if (action_button_->tag() == SAD_TAB_BUTTON_FEEDBACK) {
    chrome::ShowFeedbackPage(
        chrome::FindBrowserWithWebContents(web_contents_),
        l10n_util::GetStringUTF8(kind_ == chrome::SAD_TAB_KIND_CRASHED ?
            IDS_CRASHED_TAB_FEEDBACK_MESSAGE : IDS_KILLED_TAB_FEEDBACK_MESSAGE),
        std::string(kCategoryTagCrash));
  } else {
    web_contents_->GetController().Reload(true);
  }
}

void SadTabView::Layout() {
  // Specify the maximum message width explicitly.
  const int max_width =
      std::min(width() - views::kPanelSubVerticalSpacing * 2, kMaxContentWidth);
  message_->SizeToFit(max_width);
  title_->SizeToFit(max_width);

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
