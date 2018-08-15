// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/view_util.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

// Dimensions of the Google Pay logo.
constexpr int kGooglePayLogoWidth = 40;
constexpr int kGooglePayLogoHeight = 16;

constexpr int kGooglePayLogoSeparatorHeight = 12;

constexpr SkColor kTitleSeparatorColor = SkColorSetRGB(0x9E, 0x9E, 0x9E);

}  // namespace

TitleWithIconAndSeparatorView::TitleWithIconAndSeparatorView(
    const base::string16& window_title) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* columns = layout->AddColumnSet(0);

  // Add columns for the Google Pay icon, the separator, and the title label.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1.f,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  // kGooglePayLogoIcon is square, and CreateTiledImage() will clip it whereas
  // setting the icon size would rescale it incorrectly.
  gfx::ImageSkia image = gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(kGooglePayLogoIcon, gfx::kPlaceholderColor),
      /*x=*/0, /*y=*/0, kGooglePayLogoWidth, kGooglePayLogoHeight);
  auto* icon_view = new views::ImageView();
  icon_view->SetImage(&image);
  layout->AddView(icon_view);

  auto* separator = new views::Separator();
  separator->SetColor(kTitleSeparatorColor);
  separator->SetPreferredHeight(kGooglePayLogoSeparatorHeight);
  layout->AddView(separator);

  auto* title_label =
      new views::Label(window_title, views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  layout->AddView(title_label);

  // Add vertical padding to the icon and the separator so they are aligned with
  // the first line of title label. This needs to be done after we create the
  // title label, so that we can use its preferred size.
  const int title_label_height = title_label->GetPreferredSize().height();
  icon_view->SetBorder(views::CreateEmptyBorder(
      /*top=*/(title_label_height - kGooglePayLogoHeight) / 2,
      /*left=*/0, /*bottom=*/0, /*right=*/0));
  // TODO(crbug.com/873140): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right
  //                         choice here, but INSETS_DIALOG_TITLE gives too much
  //                         padding. Create a new Harmony DistanceMetric?
  const int separator_horizontal_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  separator->SetBorder(views::CreateEmptyBorder(
      /*top=*/(title_label_height - kGooglePayLogoSeparatorHeight) / 2,
      /*left=*/separator_horizontal_padding, /*bottom=*/0,
      /*right=*/separator_horizontal_padding));
}

gfx::Size TitleWithIconAndSeparatorView::GetMinimumSize() const {
  // View::GetMinumumSize() defaults to GridLayout::GetPreferredSize(), but that
  // gives a larger frame width, so the dialog will become wider than it should.
  // To avoid that, just return 0x0.
  return gfx::Size(0, 0);
}

views::Textfield* CreateCvcTextfield() {
  views::Textfield* textfield = new views::Textfield();
  textfield->set_placeholder_text(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC));
  textfield->SetDefaultWidthInChars(8);
  textfield->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  return textfield;
}

}  // namespace autofill
