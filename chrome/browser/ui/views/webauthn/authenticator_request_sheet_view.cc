// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

using views::BoxLayout;

AuthenticatorRequestSheetView::AuthenticatorRequestSheetView(
    std::unique_ptr<AuthenticatorRequestSheetModel> model)
    : model_(std::move(model)) {}

AuthenticatorRequestSheetView::~AuthenticatorRequestSheetView() = default;

void AuthenticatorRequestSheetView::InitChildViews() {
  // No need to add further spacing between the upper and lower half. The image
  // is designed to fill the dialog's top half without any border/margins, and
  // the |lower_half| will already contain the standard dialog borders.
  SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::kVertical, gfx::Insets(), 0 /* between_child_spacing */));

  std::unique_ptr<views::View> upper_half = CreateIllustrationWithOverlays();
  std::unique_ptr<views::View> lower_half = CreateContentsBelowIllustration();
  AddChildView(upper_half.release());
  AddChildView(lower_half.release());
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::BuildStepSpecificContent() {
  return nullptr;
}

void AuthenticatorRequestSheetView::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  DCHECK_EQ(sender, back_arrow_button_);
  model()->OnBack();
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateIllustrationWithOverlays() {
  auto image_with_overlays = std::make_unique<views::View>();
  image_with_overlays->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(model()->GetStepIllustration());
  image_with_overlays->AddChildView(image_view.release());

  if (model()->IsBackButtonVisible()) {
    std::unique_ptr<views::ImageButton> back_arrow(
        views::CreateVectorImageButton(this));
    back_arrow->SetFocusForPlatform();
    back_arrow->SetAccessibleName(l10n_util::GetStringUTF16(IDS_BACK_BUTTON));

    auto color_reference = std::make_unique<views::Label>(
        base::string16(), views::style::CONTEXT_DIALOG_TITLE,
        views::style::STYLE_PRIMARY);
    views::SetImageFromVectorIcon(
        back_arrow.get(), vector_icons::kBackArrowIcon,
        color_utils::DeriveDefaultIconColor(color_reference->enabled_color()));
    back_arrow_button_ = back_arrow.get();

    // The |overlaly_container| will be stretched to fill |image_with_overlays|
    // while allowing |back_arrow| to be absolutely positioned inside it, and
    // rendered on top of the image due to being higher in the z-order.
    auto overlay_container = std::make_unique<views::View>();
    overlay_container->AddChildView(back_arrow.release());
    image_with_overlays->AddChildView(overlay_container.release());

    // Position the back button so that there is the standard amount of padding
    // between the top/left side of the back button and the dialog borders.
    gfx::Insets dialog_insets =
        views::LayoutProvider::Get()->GetDialogInsetsForContentType(
            views::CONTROL, views::CONTROL);
    back_arrow_button_->SizeToPreferredSize();
    back_arrow_button_->SetX(dialog_insets.left());
    back_arrow_button_->SetY(dialog_insets.top());
  }

  return image_with_overlays;
}

std::unique_ptr<views::View>
AuthenticatorRequestSheetView::CreateContentsBelowIllustration() {
  auto contents = std::make_unique<views::View>();
  BoxLayout* box_layout =
      contents->SetLayoutManager(std::make_unique<BoxLayout>(
          BoxLayout::kVertical, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  contents->SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::CONTROL)));

  auto title_label = std::make_unique<views::Label>(
      model()->GetStepTitle(), views::style::CONTEXT_DIALOG_TITLE,
      views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  contents->AddChildView(title_label.release());

  auto description_label = std::make_unique<views::Label>(
      model()->GetStepDescription(),
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  contents->AddChildView(description_label.release());

  std::unique_ptr<views::View> step_specific_content =
      BuildStepSpecificContent();
  if (step_specific_content) {
    auto* step_specific_content_ptr = step_specific_content.get();
    contents->AddChildView(step_specific_content.release());
    box_layout->SetFlexForView(step_specific_content_ptr, 1);
  }

  return contents;
}
