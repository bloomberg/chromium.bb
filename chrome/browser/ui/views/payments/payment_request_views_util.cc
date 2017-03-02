// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/vector_icons/vector_icons.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/vector_icon_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace {

// TODO(tmartino): Consider combining this with the Android equivalent in
// PersonalDataManager.java
base::string16 GetAddressFromProfile(const autofill::AutofillProfile& profile,
                                     const std::string& locale) {
  std::vector<autofill::ServerFieldType> fields;
  fields.push_back(autofill::COMPANY_NAME);
  fields.push_back(autofill::ADDRESS_HOME_LINE1);
  fields.push_back(autofill::ADDRESS_HOME_LINE2);
  fields.push_back(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
  fields.push_back(autofill::ADDRESS_HOME_CITY);
  fields.push_back(autofill::ADDRESS_HOME_STATE);
  fields.push_back(autofill::ADDRESS_HOME_ZIP);
  fields.push_back(autofill::ADDRESS_HOME_SORTING_CODE);

  return profile.ConstructInferredLabel(fields, fields.size(), locale);
}

// Paints the gray horizontal line that doesn't span the entire width of the
// dialog at the bottom of the view it borders.
class PaymentRequestRowBorderPainter : public views::Painter {
 public:
  PaymentRequestRowBorderPainter() {}
  ~PaymentRequestRowBorderPainter() override {}

  // views::Painter:
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(2 * payments::kPaymentRequestRowHorizontalInsets, 1);
  }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    int line_height = size.height() - 1;
    canvas->DrawLine(
        gfx::PointF(payments::kPaymentRequestRowHorizontalInsets, line_height),
        gfx::PointF(size.width() - payments::kPaymentRequestRowHorizontalInsets,
                    line_height),
        SK_ColorLTGRAY);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRowBorderPainter);
};

}  // namespace

namespace payments {

std::unique_ptr<views::View> CreateSheetHeaderView(
    bool show_back_arrow,
    const base::string16& title,
    views::VectorIconButtonDelegate* delegate) {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();
  views::GridLayout* layout = new views::GridLayout(container.get());
  container->SetLayoutManager(layout);

  constexpr int kHeaderTopVerticalInset = 14;
  constexpr int kHeaderBottomVerticalInset = 8;
  constexpr int kHeaderHorizontalInset = 16;
  // Top, left, bottom, right.
  layout->SetInsets(kHeaderTopVerticalInset, kHeaderHorizontalInset,
                    kHeaderBottomVerticalInset, kHeaderHorizontalInset);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // A column for the optional back arrow.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  constexpr int kPaddingBetweenArrowAndTitle = 16;
  if (show_back_arrow)
    columns->AddPaddingColumn(0, kPaddingBetweenArrowAndTitle);

  // A column for the title.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  if (!show_back_arrow) {
    layout->SkipColumns(1);
  } else {
    views::VectorIconButton* back_arrow = new views::VectorIconButton(delegate);
    back_arrow->SetIcon(ui::kBackArrowIcon);
    constexpr int kBackArrowSize = 16;
    back_arrow->SetSize(gfx::Size(kBackArrowSize, kBackArrowSize));
    back_arrow->set_tag(static_cast<int>(
        PaymentRequestCommonTags::BACK_BUTTON_TAG));
    layout->AddView(back_arrow);
  }

  views::Label* title_label = new views::Label(title);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFontList(
      title_label->GetDefaultFontList().DeriveWithSizeDelta(2));
  layout->AddView(title_label);

  return container;
}

std::unique_ptr<views::ImageView> CreateCardIconView(
    const std::string& card_type) {
  std::unique_ptr<views::ImageView> card_icon_view =
      base::MakeUnique<views::ImageView>();
  card_icon_view->set_can_process_events_within_subtree(false);
  card_icon_view->SetImage(
      ResourceBundle::GetSharedInstance()
          .GetImageNamed(autofill::data_util::GetPaymentRequestData(card_type)
                             .icon_resource_id)
          .AsImageSkia());
  card_icon_view->SetTooltipText(
      autofill::CreditCard::TypeForDisplay(card_type));
  card_icon_view->SetBorder(views::CreateRoundedRectBorder(
      1, 3, card_icon_view->GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_UnfocusedBorderColor)));
  return card_icon_view;
}

std::unique_ptr<views::View> GetShippingAddressLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile) {
  base::string16 name_value =
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL), locale);

  // TODO(tmartino): Add bold styling for name in DETAILED style.

  base::string16 address_value = GetAddressFromProfile(profile, locale);

  base::string16 phone_value = profile.GetInfo(
      autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER), locale);

  std::vector<base::string16> values;
  if (!name_value.empty())
    values.push_back(name_value);
  if (!address_value.empty())
    values.push_back(address_value);
  if (!phone_value.empty())
    values.push_back(phone_value);

  return base::MakeUnique<views::StyledLabel>(
      base::JoinString(values, base::ASCIIToUTF16("\n")), nullptr);
}

// TODO(anthonyvd): unit test the label layout.
std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    bool show_payer_name,
    bool show_payer_email,
    bool show_payer_phone) {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();
  std::unique_ptr<views::BoxLayout> layout =
      base::MakeUnique<views::BoxLayout>(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  container->SetLayoutManager(layout.release());

  if (show_payer_name) {
    base::string16 name =
        profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL), locale);
    if (!name.empty()) {
      std::unique_ptr<views::Label> label =
          base::MakeUnique<views::Label>(name);
      if (type == AddressStyleType::DETAILED) {
        const gfx::FontList& font_list = label->font_list();
        label->SetFontList(font_list.DeriveWithWeight(gfx::Font::Weight::BOLD));
      }
      container->AddChildView(label.release());
    }
  }

  if (show_payer_phone) {
    base::string16 phone = profile.GetInfo(
        autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER), locale);
    if (!phone.empty()) {
      std::unique_ptr<views::Label> label =
          base::MakeUnique<views::Label>(phone);
      container->AddChildView(label.release());
    }
  }

  if (show_payer_email) {
    base::string16 email = profile.GetInfo(
        autofill::AutofillType(autofill::EMAIL_ADDRESS), locale);
    if (!email.empty()) {
      std::unique_ptr<views::Label> label =
          base::MakeUnique<views::Label>(email);
      container->AddChildView(label.release());
    }
  }

  // TODO(anthonyvd): add the error label

  return container;
}

std::unique_ptr<views::Border> CreatePaymentRequestRowBorder() {
  return views::CreateBorderPainter(
      base::MakeUnique<PaymentRequestRowBorderPainter>(),
      gfx::Insets());
}

}  // namespace payments
