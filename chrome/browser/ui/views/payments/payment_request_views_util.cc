// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/vector_icon_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
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

}  // namespace

namespace payments {

std::unique_ptr<views::View> CreateSheetHeaderView(
    bool show_back_arrow,
    const base::string16& title,
    views::VectorIconButtonDelegate* delegate) {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();
  views::GridLayout* layout = new views::GridLayout(container.get());
  container->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // A column for the optional back arrow.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  // A column for the title.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  if (!show_back_arrow) {
    layout->SkipColumns(1);
  } else {
    views::VectorIconButton* back_arrow = new views::VectorIconButton(delegate);
    back_arrow->SetIcon(kNavigateBackIcon);
    back_arrow->SetSize(back_arrow->GetPreferredSize());
    back_arrow->set_tag(static_cast<int>(
        PaymentRequestCommonTags::BACK_BUTTON_TAG));
    layout->AddView(back_arrow);
  }

  views::Label* title_label = new views::Label(title);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(title_label);

  return container;
}


std::unique_ptr<views::View> CreatePaymentView(
    std::unique_ptr<views::View> header_view,
    std::unique_ptr<views::View> content_view) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();
  view->set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer();

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);

  constexpr int kTopInsetSize = 9;
  constexpr int kBottomInsetSize = 18;
  constexpr int kSideInsetSize = 14;
  layout->SetInsets(
      kTopInsetSize, kSideInsetSize, kBottomInsetSize, kSideInsetSize);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  // |header_view| will be deleted when |view| is.
  layout->AddView(header_view.release());

  layout->StartRow(0, 0);
  // |content_view| will be deleted when |view| is.
  layout->AddView(content_view.release());

  return view;
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

std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    bool show_payer_name,
    bool show_payer_email,
    bool show_payer_phone) {
  base::string16 name_value;
  base::string16 phone_value;
  base::string16 email_value;

  if (show_payer_name) {
    name_value =
        profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL), locale);

    // TODO(tmartino): Add bold styling for name in DETAILED style.
  }

  if (show_payer_phone) {
    phone_value = profile.GetInfo(
        autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER), locale);
  }

  if (show_payer_email) {
    email_value = profile.GetInfo(
        autofill::AutofillType(autofill::EMAIL_ADDRESS), locale);
  }

  std::vector<base::string16> values;
  if (!name_value.empty())
    values.push_back(name_value);
  if (!phone_value.empty())
    values.push_back(phone_value);
  if (!email_value.empty())
    values.push_back(email_value);

  return base::MakeUnique<views::StyledLabel>(
      base::JoinString(values, base::ASCIIToUTF16("\n")), nullptr);
}

}  // namespace payments
