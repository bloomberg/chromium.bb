// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

const int kMaxContentWidth = 600;
const int kMinColumnWidth = 120;

}  // namespace

SadTabView::SadTabView(content::WebContents* web_contents,
                       chrome::SadTabKind kind)
    : SadTab(web_contents, kind) {
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
      gfx::CreateVectorIcon(kCrashedTabIcon, 48, gfx::kChromeIconGrey));
  layout->AddPaddingRow(1, views::kPanelVerticalSpacing);
  layout->StartRow(0, column_set_id);
  layout->AddView(image, 2, 1);

  title_ = CreateLabel(l10n_util::GetStringUTF16(GetTitle()));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetFontList(rb.GetFontList(ui::ResourceBundle::LargeFont));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, column_set_id, 0,
                              views::kPanelVerticalSpacing);
  layout->AddView(title_, 2, 1);

  const SkColor text_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor);

  message_ = CreateLabel(l10n_util::GetStringUTF16(GetMessage()));

  message_->SetMultiLine(true);
  message_->SetEnabledColor(text_color);
  message_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_->SetLineHeight(views::kPanelSubVerticalSpacing);

  layout->StartRowWithPadding(0, column_set_id, 0, views::kPanelVertMargin);
  layout->AddView(message_, 2, 1, views::GridLayout::LEADING,
                  views::GridLayout::LEADING);

  action_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(GetButtonTitle()));
  help_link_ =
      CreateLink(l10n_util::GetStringUTF16(GetHelpLinkTitle()), text_color);
  layout->StartRowWithPadding(0, column_set_id, 0,
                              views::kPanelVerticalSpacing);
  layout->AddView(help_link_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  layout->AddView(action_button_, 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::LEADING);

  layout->AddPaddingRow(2, views::kPanelSubVerticalSpacing);

  views::Widget::InitParams sad_tab_params(
      views::Widget::InitParams::TYPE_CONTROL);

  // It is not possible to create a native_widget_win that has no parent in
  // and later re-parent it.
  // TODO(avi): This is a cheat. Can this be made cleaner?
  sad_tab_params.parent = web_contents->GetNativeView();

  set_owned_by_client();

  views::Widget* sad_tab = new views::Widget;
  sad_tab->Init(sad_tab_params);
  sad_tab->SetContentsView(this);

  views::Widget::ReparentNativeView(sad_tab->GetNativeView(),
                                    web_contents->GetNativeView());
  gfx::Rect bounds = web_contents->GetContainerBounds();
  sad_tab->SetBounds(gfx::Rect(bounds.size()));
}

SadTabView::~SadTabView() {
  if (GetWidget())
    GetWidget()->Close();
}

void SadTabView::LinkClicked(views::Link* source, int event_flags) {
  PerformAction(Action::HELP_LINK);
}

void SadTabView::ButtonPressed(views::Button* sender,
                               const ui::Event& event) {
  DCHECK_EQ(action_button_, sender);
  PerformAction(Action::BUTTON);
}

void SadTabView::Layout() {
  // Specify the maximum message width explicitly.
  const int max_width =
      std::min(width() - views::kPanelSubVerticalSpacing * 2, kMaxContentWidth);
  message_->SizeToFit(max_width);
  title_->SizeToFit(max_width);

  View::Layout();
}

void SadTabView::OnPaint(gfx::Canvas* canvas) {
  if (!painted_) {
    RecordFirstPaint();
    painted_ = true;
  }
  View::OnPaint(canvas);
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
