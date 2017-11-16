// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sad_tab_view.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/harmony/bulleted_label_list_view.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
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
#include "ui/views/widget/widget.h"

namespace {

constexpr int kMaxContentWidth = 600;
constexpr int kMinColumnWidth = 120;
constexpr int kTitleBottomSpacing = 13;

views::Label* CreateFormattedLabel(const base::string16& message) {
  views::Label* label =
      new views::Label(message, views::style::CONTEXT_LABEL, STYLE_SECONDARY);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetLineHeight(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  return label;
}

}  // namespace

SadTabView::SadTabView(content::WebContents* web_contents, SadTabKind kind)
    : SadTab(web_contents, kind) {
  SetBackground(views::CreateThemedSolidBackground(
      this, ui::NativeTheme::kColorId_DialogBackground));

  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);

  const int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);

  // TODO(ananta)
  // This view should probably be styled as web UI.
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int unrelated_horizontal_spacing = provider->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  columns->AddPaddingColumn(1, unrelated_horizontal_spacing);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, kMinColumnWidth);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, kMinColumnWidth);
  columns->AddPaddingColumn(1, unrelated_horizontal_spacing);

  views::ImageView* image = new views::ImageView();

  image->SetImage(
      gfx::CreateVectorIcon(kCrashedTabIcon, 48, gfx::kChromeIconGrey));

  const int unrelated_vertical_spacing_large = provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  layout->AddPaddingRow(1, unrelated_vertical_spacing_large);
  layout->StartRow(0, column_set_id);
  layout->AddView(image, 2, 1);

  title_ = new views::Label(l10n_util::GetStringUTF16(GetTitle()));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_->SetFontList(rb.GetFontList(ui::ResourceBundle::LargeFont));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRowWithPadding(0, column_set_id, 0,
      unrelated_vertical_spacing_large);
  layout->AddView(title_, 2, 1);

  message_ = CreateFormattedLabel(l10n_util::GetStringUTF16(GetMessage()));
  layout->StartRowWithPadding(0, column_set_id, 0, kTitleBottomSpacing);
  layout->AddView(message_, 2, 1, views::GridLayout::LEADING,
                  views::GridLayout::LEADING);

  std::vector<int> bullet_string_ids = GetSubMessages();
  if (!bullet_string_ids.empty()) {
    auto list_view = std::make_unique<BulletedLabelListView>();
    for (const auto& id : bullet_string_ids)
      list_view->AddLabel(l10n_util::GetStringUTF16(id));

    layout->StartRow(0, column_set_id);
    layout->AddView(list_view.release(), 2, 1);
  }

  action_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(GetButtonTitle()));
  help_link_ = new views::Link(l10n_util::GetStringUTF16(GetHelpLinkTitle()));
  help_link_->set_listener(this);
  layout->StartRowWithPadding(0, column_set_id, 0,
      unrelated_vertical_spacing_large);
  layout->AddView(help_link_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  layout->AddView(action_button_, 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::LEADING);

  layout->AddPaddingRow(2, provider->GetDistanceMetric(
                               views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

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
      std::min(width() - ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_HORIZONTAL) * 2, kMaxContentWidth);

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

SadTab* SadTab::Create(content::WebContents* web_contents,
                       SadTabKind kind) {
  return new SadTabView(web_contents, kind);
}
