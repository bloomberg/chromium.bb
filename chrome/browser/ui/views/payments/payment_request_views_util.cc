// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"

#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/payments/core/payment_options_provider.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/vector_icons/vector_icons.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// |s1|, |s2|, and |s3| are lines identifying the profile. |s1| is the
// "headline" which may be emphasized depending on |type|. If |enabled| is
// false, the labels will look disabled.
std::unique_ptr<views::View> GetBaseProfileLabel(
    AddressStyleType type,
    const base::string16& s1,
    const base::string16& s2,
    const base::string16& s3,
    base::string16* accessible_content,
    bool enabled = true) {
  DCHECK(accessible_content);
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();
  std::unique_ptr<views::BoxLayout> layout = base::MakeUnique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  container->SetLayoutManager(layout.release());

  if (!s1.empty()) {
    const int text_style = type == AddressStyleType::DETAILED
                               ? static_cast<int>(STYLE_EMPHASIZED)
                               : static_cast<int>(views::style::STYLE_PRIMARY);
    std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
        s1, views::style::CONTEXT_LABEL, text_style);
    label->set_id(static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_1));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!enabled) {
      label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelDisabledColor));
    }
    container->AddChildView(label.release());
  }

  if (!s2.empty()) {
    std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(s2);
    label->set_id(static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_2));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!enabled) {
      label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelDisabledColor));
    }
    container->AddChildView(label.release());
  }

  if (!s3.empty()) {
    std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(s3);
    label->set_id(static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_3));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!enabled) {
      label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelDisabledColor));
    }
    container->AddChildView(label.release());
  }

  *accessible_content = l10n_util::GetStringFUTF16(
      IDS_PAYMENTS_PROFILE_LABELS_ACCESSIBLE_FORMAT, s1, s2, s3);

  return container;
}

// Returns a label representing the |profile| as a shipping address. See
// GetBaseProfileLabel() for more documentation.
std::unique_ptr<views::View> GetShippingAddressLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    base::string16* accessible_content,
    bool enabled) {
  DCHECK(accessible_content);
  base::string16 name =
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL), locale);

  base::string16 address =
      GetShippingAddressLabelFormAutofillProfile(profile, locale);

  base::string16 phone =
      data_util::GetFormattedPhoneNumberForDisplay(profile, locale);

  return GetBaseProfileLabel(type, name, address, phone, accessible_content,
                             enabled);
}

std::unique_ptr<views::Label> GetLabelForMissingInformation(
    const base::string16& missing_info) {
  std::unique_ptr<views::Label> label =
      base::MakeUnique<views::Label>(missing_info, CONTEXT_DEPRECATED_SMALL);
  label->set_id(static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  // Missing information typically has a nice shade of blue.
  label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LinkEnabled));
  return label;
}

// Paints the gray horizontal line that doesn't span the entire width of the
// dialog at the bottom of the view it borders.
class PaymentRequestRowBorderPainter : public views::Painter {
 public:
  explicit PaymentRequestRowBorderPainter(SkColor color) : color_(color) {}
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
        color_);
  }

 private:
  SkColor color_;
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRowBorderPainter);
};

}  // namespace

int GetActualDialogWidth() {
  static int actual_width =
      views::LayoutProvider::Get()->GetSnappedDialogWidth(kDialogMinWidth);
  return actual_width;
}

std::unique_ptr<views::View> CreateSheetHeaderView(
    bool show_back_arrow,
    const base::string16& title,
    views::ButtonListener* listener) {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();
  views::GridLayout* layout = new views::GridLayout(container.get());
  container->SetLayoutManager(layout);

  constexpr int kHeaderTopVerticalInset = 14;
  constexpr int kHeaderBottomVerticalInset = 8;
  constexpr int kHeaderHorizontalInset = 16;
  container->SetBorder(views::CreateEmptyBorder(
      kHeaderTopVerticalInset, kHeaderHorizontalInset,
      kHeaderBottomVerticalInset, kHeaderHorizontalInset));

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // A column for the optional back arrow.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  constexpr int kPaddingBetweenArrowAndTitle = 16;
  if (show_back_arrow)
    columns->AddPaddingColumn(0, kPaddingBetweenArrowAndTitle);

  // A column for the title.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  if (!show_back_arrow) {
    layout->SkipColumns(1);
  } else {
    views::ImageButton* back_arrow = views::CreateVectorImageButton(listener);
    views::SetImageFromVectorIcon(back_arrow, ui::kBackArrowIcon);
    constexpr int kBackArrowSize = 16;
    back_arrow->SetSize(gfx::Size(kBackArrowSize, kBackArrowSize));
    back_arrow->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    back_arrow->set_tag(
        static_cast<int>(PaymentRequestCommonTags::BACK_BUTTON_TAG));
    back_arrow->set_id(static_cast<int>(DialogViewID::BACK_BUTTON));
    back_arrow->SetAccessibleName(l10n_util::GetStringUTF16(IDS_PAYMENTS_BACK));
    layout->AddView(back_arrow);
  }

  views::Label* title_label =
      new views::Label(title, views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(title_label);

  return container;
}

std::unique_ptr<views::ImageView> CreateInstrumentIconView(
    int icon_resource_id,
    const base::string16& tooltip_text,
    float opacity) {
  std::unique_ptr<views::ImageView> card_icon_view =
      base::MakeUnique<views::ImageView>();
  card_icon_view->set_can_process_events_within_subtree(false);
  card_icon_view->SetImage(ResourceBundle::GetSharedInstance()
                               .GetImageNamed(icon_resource_id)
                               .AsImageSkia());
  card_icon_view->SetTooltipText(tooltip_text);
  card_icon_view->SetPaintToLayer();
  card_icon_view->layer()->SetFillsBoundsOpaquely(false);
  card_icon_view->layer()->SetOpacity(opacity);
  return card_icon_view;
}

std::unique_ptr<views::View> CreateProductLogoFooterView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(), 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  content_view->SetLayoutManager(layout);

  // Adds the Chrome logo image.
  std::unique_ptr<views::ImageView> chrome_logo =
      base::MakeUnique<views::ImageView>();
  chrome_logo->set_can_process_events_within_subtree(false);
  chrome_logo->SetImage(ResourceBundle::GetSharedInstance()
                            .GetImageNamed(IDR_PRODUCT_LOGO_NAME_22)
                            .AsImageSkia());
  chrome_logo->SetTooltipText(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  content_view->AddChildView(chrome_logo.release());

  return content_view;
}

std::unique_ptr<views::View> GetShippingAddressLabelWithMissingInfo(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentsProfileComparator& comp,
    base::string16* accessible_content,
    bool enabled) {
  DCHECK(accessible_content);
  std::unique_ptr<views::View> base_label = GetShippingAddressLabel(
      type, locale, profile, accessible_content, enabled);

  base::string16 missing = comp.GetStringForMissingShippingFields(profile);
  if (!missing.empty()) {
    base_label->AddChildView(GetLabelForMissingInformation(missing).release());
  }
  return base_label;
}

// TODO(anthonyvd): unit test the label layout.
std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentOptionsProvider& options,
    const PaymentsProfileComparator& comp,
    base::string16* accessible_content) {
  DCHECK(accessible_content);
  base::string16 name =
      options.request_payer_name()
          ? profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL), locale)
          : base::string16();

  base::string16 phone =
      options.request_payer_phone()
          ? data_util::GetFormattedPhoneNumberForDisplay(profile, locale)
          : base::string16();

  base::string16 email =
      options.request_payer_email()
          ? profile.GetInfo(autofill::AutofillType(autofill::EMAIL_ADDRESS),
                            locale)
          : base::string16();

  std::unique_ptr<views::View> base_label =
      GetBaseProfileLabel(type, name, phone, email, accessible_content);

  base::string16 missing = comp.GetStringForMissingContactFields(profile);
  if (!missing.empty()) {
    base_label->AddChildView(GetLabelForMissingInformation(missing).release());
  }
  return base_label;
}

std::unique_ptr<views::Border> CreatePaymentRequestRowBorder(
    SkColor color,
    const gfx::Insets& insets) {
  return views::CreateBorderPainter(
      base::MakeUnique<PaymentRequestRowBorderPainter>(color), insets);
}

std::unique_ptr<views::Label> CreateBoldLabel(const base::string16& text) {
  return base::MakeUnique<views::Label>(text, views::style::CONTEXT_LABEL,
                                        STYLE_EMPHASIZED);
}

std::unique_ptr<views::Label> CreateMediumLabel(const base::string16& text) {
  // TODO(tapted): This should refer to a style in the Chrome typography spec.
  // Also, it needs to handle user setups where the default font is BOLD already
  // since asking for a MEDIUM font will give a lighter font.
  std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(text);
  label->SetFontList(ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      ui::kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  return label;
}

std::unique_ptr<views::Label> CreateHintLabel(
    const base::string16& text,
    gfx::HorizontalAlignment alignment) {
  std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
      text, views::style::CONTEXT_LABEL, STYLE_HINT);
  label->SetHorizontalAlignment(alignment);
  return label;
}

std::unique_ptr<views::View> CreateShippingOptionLabel(
    payments::mojom::PaymentShippingOption* shipping_option,
    const base::string16& formatted_amount,
    bool emphasize_label,
    base::string16* accessible_content) {
  DCHECK(accessible_content);
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();

  std::unique_ptr<views::BoxLayout> layout = base::MakeUnique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  container->SetLayoutManager(layout.release());

  if (shipping_option) {
    const base::string16& text = base::UTF8ToUTF16(shipping_option->label);
    std::unique_ptr<views::Label> shipping_label =
        emphasize_label ? CreateMediumLabel(text)
                        : base::MakeUnique<views::Label>(text);
    shipping_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    shipping_label->set_id(
        static_cast<int>(DialogViewID::SHIPPING_OPTION_DESCRIPTION));
    container->AddChildView(shipping_label.release());

    std::unique_ptr<views::Label> amount_label =
        base::MakeUnique<views::Label>(formatted_amount);
    amount_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    amount_label->set_id(
        static_cast<int>(DialogViewID::SHIPPING_OPTION_AMOUNT));
    container->AddChildView(amount_label.release());

    *accessible_content = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_PROFILE_LABELS_ACCESSIBLE_FORMAT, text, formatted_amount,
        base::string16());
  }

  return container;
}

}  // namespace payments
