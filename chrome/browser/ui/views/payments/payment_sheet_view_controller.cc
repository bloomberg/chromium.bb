// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/message_formatter.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_row_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace payments {
namespace {

constexpr int kFirstTagValue = static_cast<int>(
    payments::PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX);

enum class PaymentSheetViewControllerTags {
  // The tag for the button that navigates to the Order Summary sheet.
  SHOW_ORDER_SUMMARY_BUTTON = kFirstTagValue,
  SHOW_SHIPPING_BUTTON,
  SHOW_PAYMENT_METHOD_BUTTON,
  SHOW_CONTACT_INFO_BUTTON,
  SHOW_SHIPPING_OPTION_BUTTON,
  PAY_BUTTON
};

// A class that ensures proper elision of labels in the form
// "[preview] and N more" where preview might be elided to allow "and N more" to
// be always visible.
class PreviewEliderLabel : public views::Label {
 public:
  // Creates a PreviewEliderLabel where |preview_text| might be elided,
  // |format_string| is the string with format argument numbers in ICU syntax
  // and |n| is the "N more" item count.
  PreviewEliderLabel(const base::string16& preview_text,
                     const base::string16& format_string,
                     int n)
      : views::Label(base::ASCIIToUTF16("")),
        preview_text_(preview_text),
        format_string_(format_string),
        n_(n) {}

 private:
  // Formats |preview_text_|, |format_string_|, and |n_| into a string that fits
  // inside of |pixel_width|, eliding |preview_text_| as required.
  base::string16 CreateElidedString(int pixel_width) {
    for (int preview_length = preview_text_.size(); preview_length > 0;
         --preview_length) {
      base::string16 elided_preview;
      gfx::ElideRectangleString(preview_text_, 1, preview_length,
                                /*strict=*/false, &elided_preview);
      base::string16 elided_string =
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              format_string_, "", elided_preview, n_);
      if (gfx::GetStringWidth(elided_string, font_list()) <= width())
        return elided_string;
    }

    // TODO(crbug.com/714776): Display something meaningful if the preview can't
    // be elided enough for the string to fit.
    return base::ASCIIToUTF16("");
  }

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    SetText(CreateElidedString(width()));
    views::Label::OnBoundsChanged(previous_bounds);
  }

  base::string16 preview_text_;
  base::string16 format_string_;
  int n_;

  DISALLOW_COPY_AND_ASSIGN(PreviewEliderLabel);
};

int ComputeWidestNameColumnViewWidth() {
  // The name colums in each row should all have the same width, large enough to
  // accomodate the longest piece of text they contain. Because of this, each
  // row's GridLayout requires its first column to have a fixed width of the
  // correct size. To measure the required size, layout a label with each
  // section name, measure its width, then initialize |widest_column_width|
  // with the largest value.
  std::vector<int> section_names{
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_NAME,
      IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME,
      IDS_PAYMENT_REQUEST_SHIPPING_SECTION_NAME};

  int widest_column_width = 0;

  views::Label label(base::ASCIIToUTF16(""));
  for (int name_id : section_names) {
    label.SetText(l10n_util::GetStringUTF16(name_id));
    widest_column_width = std::max(
        label.GetPreferredSize().width(),
        widest_column_width);
  }

  return widest_column_width;
}

std::unique_ptr<views::Button> CreatePaymentSheetRow(
    views::ButtonListener* listener,
    const base::string16& section_name,
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<views::View> extra_content_view,
    std::unique_ptr<views::View> trailing_button,
    bool clickable,
    int name_column_width) {
  std::unique_ptr<PaymentRequestRowView> row =
      base::MakeUnique<PaymentRequestRowView>(listener, clickable);
  views::GridLayout* layout = new views::GridLayout(row.get());

  // The rows have extra inset compared to the header so that their right edge
  // lines up with the close button's X rather than its invisible right edge.
  layout->SetInsets(
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets,
      kPaymentRequestRowHorizontalInsets + kPaymentRequestRowExtraRightInset);
  row->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // A column for the section name.
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     0,
                     views::GridLayout::FIXED,
                     name_column_width,
                     0);

  constexpr int kPaddingColumnsWidth = 25;
  columns->AddPaddingColumn(0, kPaddingColumnsWidth);

  // A column for the content.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                     1, views::GridLayout::USE_PREF, 0, 0);
  // A column for the extra content.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  columns->AddPaddingColumn(0, kPaddingColumnsWidth);
  // A column for the trailing_button.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  views::Label* name_label = new views::Label(section_name);
  layout->AddView(name_label);

  if (content_view) {
    content_view->set_can_process_events_within_subtree(false);
    layout->AddView(content_view.release());
  } else {
    layout->SkipColumns(1);
  }

  if (extra_content_view) {
    extra_content_view->set_can_process_events_within_subtree(false);
    layout->AddView(extra_content_view.release());
  } else {
    layout->SkipColumns(1);
  }

  layout->AddView(trailing_button.release());

  return std::move(row);
}

// A class used to build Payment Sheet Rows. Construct an instance of it, chain
// calls to argument-setting functions, then call one of the CreateWith*
// functions to create the row view.
class PaymentSheetRowBuilder {
 public:
  PaymentSheetRowBuilder(views::ButtonListener* listener,
                         const base::string16& section_name,
                         int name_column_width)
      : listener_(listener),
        section_name_(section_name),
        name_column_width_(name_column_width) {}

  PaymentSheetRowBuilder& Tag(PaymentSheetViewControllerTags tag) {
    tag_ = static_cast<int>(tag);
    return *this;
  }

  PaymentSheetRowBuilder& Id(DialogViewID id) {
    id_ = static_cast<int>(id);
    return *this;
  }

  // Creates a clickable row to be displayed in the Payment Sheet. It contains
  // a section name and some content, followed by a chevron as a clickability
  // affordance. Both, either, or none of |content_view| and
  // |extra_content_view| may be present, the difference between the two being
  // that content is pinned to the left and extra_content is pinned to the
  // right. The row also displays a light gray horizontal ruler on its lower
  // boundary. The name column has a fixed width equal to |name_column_width|.
  // +----------------------------+
  // | Name | Content | Extra | > |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- ruler
  std::unique_ptr<views::Button> CreateWithChevron(
      std::unique_ptr<views::View> content_view,
      std::unique_ptr<views::View> extra_content_view) {
    std::unique_ptr<views::ImageView> chevron =
        base::MakeUnique<views::ImageView>();
    chevron->set_can_process_events_within_subtree(false);
    std::unique_ptr<views::Label> label =
        base::MakeUnique<views::Label>(section_name_);
    chevron->SetImage(gfx::CreateVectorIcon(
        views::kSubmenuArrowIcon,
        color_utils::DeriveDefaultIconColor(label->enabled_color())));
    std::unique_ptr<views::Button> section =
        CreatePaymentSheetRow(listener_, section_name_, std::move(content_view),
                              std::move(extra_content_view), std::move(chevron),
                              /*clickable=*/true, name_column_width_);
    section->set_tag(tag_);
    section->set_id(id_);
    return section;
  }

  // Creates a row with a button in place of the chevron and |truncated_content|
  // between |section_name| and the button.
  // +------------------------------------------+
  // | Name | truncated_content | button_string |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  std::unique_ptr<views::Button> CreateWithButton(
      const base::string16& truncated_content,
      const base::string16& button_string,
      bool button_enabled) {
    std::unique_ptr<views::Label> content_view =
        base::MakeUnique<views::Label>(truncated_content);
    content_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return CreateWithButton(std::move(content_view), button_string,
                            button_enabled);
  }

  // Creates a row with a button in place of the chevron with the string between
  // |section_name| and the button built as "|preview|... and |n| more".
  // |format_string| is used to assemble the truncated preview and the rest of
  // the content string.
  // +----------------------------------------------+
  // | Name | preview... and N more | button_string |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  std::unique_ptr<views::Button> CreateWithButton(
      const base::string16& preview_text,
      const base::string16& format_string,
      int n,
      const base::string16& button_string,
      bool button_enabled) {
    std::unique_ptr<PreviewEliderLabel> content_view =
        base::MakeUnique<PreviewEliderLabel>(preview_text, format_string, n);
    content_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return CreateWithButton(std::move(content_view), button_string,
                            button_enabled);
  }

 private:
  // Creates a row with a button in place of the chevron.
  // +------------------------------------------+
  // | Name | content_view      | button_string |
  // +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
  std::unique_ptr<views::Button> CreateWithButton(
      std::unique_ptr<views::View> content_view,
      const base::string16& button_string,
      bool button_enabled) {
    std::unique_ptr<views::Button> button(
        views::MdTextButton::CreateSecondaryUiBlueButton(listener_,
                                                         button_string));
    button->set_tag(tag_);
    button->set_id(id_);
    button->SetEnabled(button_enabled);
    return CreatePaymentSheetRow(listener_, section_name_,
                                 std::move(content_view), nullptr,
                                 std::move(button),
                                 /*clickable=*/false, name_column_width_);
  }

  views::ButtonListener* listener_;
  base::string16 section_name_;
  int name_column_width_;
  int tag_;
  int id_;
  DISALLOW_COPY_AND_ASSIGN(PaymentSheetRowBuilder);
};

// Creates a GridLayout object to be used in the Order Summary section's list of
// items and the list of prices. |host| is the view that will be assigned the
// returned Layout Manager and |trailing| indicates whether the elements added
// to the manager should have trailing horizontal alignment. If trailing is
// |false|, their horizontal alignment is leading.
std::unique_ptr<views::GridLayout> CreateOrderSummarySectionContainerLayout(
    views::View* host,
    bool trailing) {
  std::unique_ptr<views::GridLayout> layout =
      base::MakeUnique<views::GridLayout>(host);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(
      trailing ? views::GridLayout::TRAILING : views::GridLayout::LEADING,
      views::GridLayout::LEADING, 1, views::GridLayout::USE_PREF, 0, 0);

  return layout;
}

std::unique_ptr<views::View> CreateCheckingSpinnerView() {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();

  std::unique_ptr<views::Throbber> throbber =
      base::MakeUnique<views::Throbber>();
  throbber->Start();
  std::unique_ptr<views::GridLayout> layout =
      base::MakeUnique<views::GridLayout>(container.get());
  views::ColumnSet* throbber_columns = layout->AddColumnSet(0);
  throbber_columns->AddPaddingColumn(0.5, 0);
  throbber_columns->AddColumn(views::GridLayout::Alignment::CENTER,
                              views::GridLayout::Alignment::TRAILING, 0,
                              views::GridLayout::SizeType::USE_PREF, 0, 0);
  throbber_columns->AddPaddingColumn(0.5, 0);

  views::ColumnSet* label_columns = layout->AddColumnSet(1);
  label_columns->AddPaddingColumn(0.5, 0);
  label_columns->AddColumn(views::GridLayout::Alignment::CENTER,
                           views::GridLayout::Alignment::LEADING, 0,
                           views::GridLayout::SizeType::USE_PREF, 0, 0);
  label_columns->AddPaddingColumn(0.5, 0);

  layout->StartRow(0.5, 0);
  layout->AddView(throbber.release());

  layout->StartRow(0.5, 1);
  layout->AddView(new views::Label(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_CHECKING_OPTION)));

  container->SetLayoutManager(layout.release());

  return container;
}

}  // namespace

PaymentSheetViewController::PaymentSheetViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      pay_button_(nullptr),
      widest_name_column_view_width_(ComputeWidestNameColumnViewWidth()),
      current_update_reason_(PaymentRequestSpec::UpdateReason::NONE) {
  spec->AddObserver(this);
  state->AddObserver(this);
}

PaymentSheetViewController::~PaymentSheetViewController() {
  spec()->RemoveObserver(this);
  state()->RemoveObserver(this);
}

void PaymentSheetViewController::OnStartUpdating(
    PaymentRequestSpec::UpdateReason reason) {
  current_update_reason_ = reason;
  UpdateContentView();
}

void PaymentSheetViewController::OnSpecUpdated() {
  current_update_reason_ = PaymentRequestSpec::UpdateReason::NONE;
  UpdateContentView();
}

void PaymentSheetViewController::OnSelectedInformationChanged() {
  UpdatePayButtonState(state()->is_ready_to_pay());
  UpdateContentView();
}

std::unique_ptr<views::Button>
PaymentSheetViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_PAYMENTS_PAY_BUTTON)));
  button->set_tag(static_cast<int>(PaymentRequestCommonTags::PAY_BUTTON_TAG));
  button->set_id(static_cast<int>(DialogViewID::PAY_BUTTON));
  pay_button_ = button.get();
  UpdatePayButtonState(state()->is_ready_to_pay());
  return button;
}

bool PaymentSheetViewController::ShouldShowHeaderBackArrow() {
  return false;
}

base::string16 PaymentSheetViewController::GetSheetTitle() {
  return l10n_util::GetStringUTF16(IDS_PAYMENTS_TITLE);
}

void PaymentSheetViewController::FillContentView(views::View* content_view) {
  views::GridLayout* layout = new views::GridLayout(content_view);
  content_view->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  // The shipping address and contact info rows are optional.
  layout->StartRow(0, 0);
  layout->AddView(CreatePaymentSheetSummaryRow().release());

  if (spec()->request_shipping()) {
    layout->StartRow(0, 0);
    layout->AddView(CreateShippingRow().release());
    // It's possible for requestShipping to be true and for there to be no
    // shipping options yet (they will come in updateWith).
    // TODO(crbug.com/707353): Put a better placeholder row, instead of no row.
    std::unique_ptr<views::Button> shipping_row = CreateShippingOptionRow();
    if (shipping_row) {
      layout->StartRow(0, 0);
      layout->AddView(shipping_row.release());
    }
  }
  layout->StartRow(0, 0);
  layout->AddView(CreatePaymentMethodRow().release());
  if (spec()->request_payer_name() || spec()->request_payer_email() ||
      spec()->request_payer_phone()) {
    layout->StartRow(0, 0);
    layout->AddView(CreateContactInfoRow().release());
  }
}

// Adds the product logo to the footer.
// +---------------------------------------------------------+
// | (•) chrome                               | PAY | CANCEL |
// +---------------------------------------------------------+
std::unique_ptr<views::View>
PaymentSheetViewController::CreateExtraFooterView() {
  return CreateProductLogoFooterView();
}

void PaymentSheetViewController::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(
        PaymentSheetViewControllerTags::SHOW_ORDER_SUMMARY_BUTTON):
      dialog()->ShowOrderSummary();
      break;

    case static_cast<int>(PaymentSheetViewControllerTags::SHOW_SHIPPING_BUTTON):
      dialog()->ShowShippingProfileSheet();
      break;

    case static_cast<int>(
        PaymentSheetViewControllerTags::SHOW_PAYMENT_METHOD_BUTTON):
      dialog()->ShowPaymentMethodSheet();
      break;

    case static_cast<int>(
        PaymentSheetViewControllerTags::SHOW_CONTACT_INFO_BUTTON):
      dialog()->ShowContactProfileSheet();
      break;

    case static_cast<int>(
        PaymentSheetViewControllerTags::SHOW_SHIPPING_OPTION_BUTTON):
      dialog()->ShowShippingOptionSheet();
      break;

    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
      break;
  }
}

void PaymentSheetViewController::UpdatePayButtonState(bool enabled) {
  pay_button_->SetEnabled(enabled);
}

// Creates the Order Summary row, which contains an "Order Summary" label,
// an inline list of display items, a Total Amount label, and a Chevron.
// +----------------------------------------------+
// | Order Summary   Item 1            $ 1.34     |
// |                 Item 2            $ 2.00   > |
// |                 2 more items...              |
// |                 Total         USD $12.34     |
// +----------------------------------------------+
std::unique_ptr<views::Button>
PaymentSheetViewController::CreatePaymentSheetSummaryRow() {
  std::unique_ptr<views::View> item_summaries = base::MakeUnique<views::View>();
  std::unique_ptr<views::GridLayout> item_summaries_layout =
      CreateOrderSummarySectionContainerLayout(item_summaries.get(),
                                               /* trailing =*/false);

  std::unique_ptr<views::View> item_amounts = base::MakeUnique<views::View>();
  std::unique_ptr<views::GridLayout> item_amounts_layout =
      CreateOrderSummarySectionContainerLayout(item_amounts.get(),
                                               /* trailing =*/true);

  const std::vector<mojom::PaymentItemPtr>& items =
      spec()->details().display_items;
  // The inline items section contains the first 2 display items of the
  // request's details, followed by a label indicating "N more items..." if
  // there are more than 2 items in the details. The total label and amount
  // always follow.
  constexpr int kMaxNumberOfItemsShown = 2;
  for (size_t i = 0; i < items.size() && i < kMaxNumberOfItemsShown; ++i) {
    item_summaries_layout->StartRow(0, 0);
    std::unique_ptr<views::Label> summary =
        base::MakeUnique<views::Label>(base::UTF8ToUTF16(items[i]->label));
    summary->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    item_summaries_layout->AddView(summary.release());

    item_amounts_layout->StartRow(0, 0);
    item_amounts_layout->AddView(new views::Label(
        spec()->GetFormattedCurrencyAmount(items[i]->amount->value)));
  }

  int hidden_item_count = items.size() - kMaxNumberOfItemsShown;
  if (hidden_item_count > 0) {
    item_summaries_layout->StartRow(0, 0);
    std::unique_ptr<views::Label> label =
        base::MakeUnique<views::Label>(l10n_util::GetPluralStringFUTF16(
            IDS_PAYMENT_REQUEST_ORDER_SUMMARY_MORE_ITEMS, hidden_item_count));
    label->SetDisabledColor(label->GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_LabelDisabledColor));
    label->SetEnabled(false);
    item_summaries_layout->AddView(label.release());

    item_amounts_layout->StartRow(0, 0);
    item_amounts_layout->AddView(new views::Label(base::ASCIIToUTF16("")));
  }

  item_summaries_layout->StartRow(0, 0);
  item_summaries_layout->AddView(
      CreateBoldLabel(base::UTF8ToUTF16(spec()->details().total->label))
          .release());

  item_amounts_layout->StartRow(0, 0);
  item_amounts_layout->AddView(
      CreateBoldLabel(l10n_util::GetStringFUTF16(
                          IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
                          base::UTF8ToUTF16(spec()->GetFormattedCurrencyCode()),
                          spec()->GetFormattedCurrencyAmount(
                              spec()->details().total->amount->value)))
          .release());

  item_summaries->SetLayoutManager(item_summaries_layout.release());
  item_amounts->SetLayoutManager(item_amounts_layout.release());

  PaymentSheetRowBuilder builder(
      this,
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_NAME),
      widest_name_column_view_width_);
  builder.Tag(PaymentSheetViewControllerTags::SHOW_ORDER_SUMMARY_BUTTON)
      .Id(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION);

  return builder.CreateWithChevron(std::move(item_summaries),
                                   std::move(item_amounts));
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateShippingSectionContent() {
  if (current_update_reason_ ==
      PaymentRequestSpec::UpdateReason::SHIPPING_ADDRESS) {
    return CreateCheckingSpinnerView();
  } else {
    auto* profile = state()->selected_shipping_profile();

    return profile
               ? GetShippingAddressLabel(
                     AddressStyleType::SUMMARY, state()->GetApplicationLocale(),
                     *profile, *spec(), *(state()->profile_comparator()))
               : base::MakeUnique<views::Label>(base::string16());
  }
}

// Creates the Shipping row, which contains a "Shipping address" label, the
// user's selected shipping address, and a chevron.
// +----------------------------------------------+
// | Shipping Address   Barack Obama              |
// |                    1600 Pennsylvania Ave.  > |
// |                    1800MYPOTUS               |
// +----------------------------------------------+
std::unique_ptr<views::Button> PaymentSheetViewController::CreateShippingRow() {
  std::unique_ptr<views::Button> section;
  PaymentSheetRowBuilder builder(
      this, GetShippingAddressSectionString(spec()->shipping_type()),
      widest_name_column_view_width_);
  builder.Tag(PaymentSheetViewControllerTags::SHOW_SHIPPING_BUTTON);
  if (state()->selected_shipping_profile()) {
    builder.Id(DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION);
    return builder.CreateWithChevron(CreateShippingSectionContent(), nullptr);
  } else {
    builder.Id(DialogViewID::PAYMENT_SHEET_SHIPPING_ADDRESS_SECTION_BUTTON);
    base::string16 button_string = state()->shipping_profiles().size()
                                       ? l10n_util::GetStringUTF16(IDS_CHOOSE)
                                       : l10n_util::GetStringUTF16(IDS_ADD);
    if (state()->shipping_profiles().empty()) {
      return builder.CreateWithButton(base::ASCIIToUTF16(""), button_string,
                                      /*button_enabled=*/true);
    } else if (state()->shipping_profiles().size() == 1) {
      base::string16 truncated_content =
          state()->shipping_profiles()[0]->ConstructInferredLabel(
              {
                  autofill::ADDRESS_HOME_LINE1, autofill::ADDRESS_HOME_LINE2,
                  autofill::ADDRESS_HOME_CITY, autofill::ADDRESS_HOME_STATE,
                  autofill::ADDRESS_HOME_COUNTRY,
              },
              6, state()->GetApplicationLocale());
      return builder.CreateWithButton(truncated_content, button_string,
                                      /*button_enabled=*/true);
    } else {
      base::string16 format = l10n_util::GetPluralStringFUTF16(
          IDS_PAYMENT_REQUEST_SHIPPING_ADDRESSES_PREVIEW,
          state()->shipping_profiles().size() - 1);
      base::string16 label =
          state()->shipping_profiles()[0]->ConstructInferredLabel(
              {
                  autofill::ADDRESS_HOME_LINE1, autofill::ADDRESS_HOME_LINE2,
                  autofill::ADDRESS_HOME_CITY, autofill::ADDRESS_HOME_STATE,
                  autofill::ADDRESS_HOME_COUNTRY,
              },
              6, state()->GetApplicationLocale());
      return builder.CreateWithButton(
          label, format, state()->shipping_profiles().size() - 1, button_string,
          /*button_enabled=*/true);
    }
  }
}

// Creates the Payment Method row, which contains a "Payment" label, the user's
// masked Credit Card details, the icon for the selected card, and a chevron.
// If no option is selected or none is available, the Chevron and icon are
// replaced with a button
// +----------------------------------------------+
// | Payment         Visa ****0000                |
// |                 John Smith        | VISA | > |
// |                                              |
// +----------------------------------------------+
std::unique_ptr<views::Button>
PaymentSheetViewController::CreatePaymentMethodRow() {
  PaymentInstrument* selected_instrument = state()->selected_instrument();

  PaymentSheetRowBuilder builder(
      this,
      l10n_util::GetStringUTF16(
          IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME),
      widest_name_column_view_width_);
  builder.Tag(PaymentSheetViewControllerTags::SHOW_PAYMENT_METHOD_BUTTON);

  if (selected_instrument) {
    std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

    views::GridLayout* layout = new views::GridLayout(content_view.get());
    content_view->SetLayoutManager(layout);
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       1, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    std::unique_ptr<views::Label> selected_instrument_label =
        base::MakeUnique<views::Label>(selected_instrument->label());
    selected_instrument_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(selected_instrument_label.release());

    layout->StartRow(0, 0);
    std::unique_ptr<views::Label> selected_instrument_sublabel =
        base::MakeUnique<views::Label>(selected_instrument->sublabel());
    selected_instrument_sublabel->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(selected_instrument_sublabel.release());

    std::unique_ptr<views::ImageView> card_icon_view = CreateInstrumentIconView(
        selected_instrument->icon_resource_id(), selected_instrument->label());
    card_icon_view->SetImageSize(gfx::Size(32, 20));

    return builder.Id(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION)
        .CreateWithChevron(std::move(content_view), std::move(card_icon_view));
  } else {
    builder.Id(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION_BUTTON);
    base::string16 button_string = state()->available_instruments().size()
                                       ? l10n_util::GetStringUTF16(IDS_CHOOSE)
                                       : l10n_util::GetStringUTF16(IDS_ADD);

    if (state()->available_instruments().empty()) {
      return builder.CreateWithButton(base::ASCIIToUTF16(""), button_string,
                                      /*button_enabled=*/true);
    } else if (state()->available_instruments().size() == 1) {
      return builder.CreateWithButton(
          state()->available_instruments()[0]->label(), button_string,
          /*button_enabled=*/true);
    } else {
      base::string16 format = l10n_util::GetPluralStringFUTF16(
          IDS_PAYMENT_REQUEST_PAYMENT_METHODS_PREVIEW,
          state()->available_instruments().size() - 1);
      return builder.CreateWithButton(
          state()->available_instruments()[0]->label(), format,
          state()->available_instruments().size() - 1, button_string,
          /*button_enabled=*/true);
    }
  }
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateContactInfoSectionContent() {
  autofill::AutofillProfile* profile = state()->selected_contact_profile();
  return profile
             ? payments::GetContactInfoLabel(
                   AddressStyleType::SUMMARY, state()->GetApplicationLocale(),
                   *profile, *spec(), *(state()->profile_comparator()))
             : base::MakeUnique<views::Label>(base::string16());
}

// Creates the Contact Info row, which contains a "Contact info" label; the
// name, email address, and/or phone number; and a chevron.
// +----------------------------------------------+
// | Contact info       Barack Obama              |
// |                    1800MYPOTUS             > |
// |                    potus@whitehouse.gov      |
// +----------------------------------------------+
std::unique_ptr<views::Button>
PaymentSheetViewController::CreateContactInfoRow() {
  PaymentSheetRowBuilder builder(
      this,
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME),
      widest_name_column_view_width_);
  builder.Tag(PaymentSheetViewControllerTags::SHOW_CONTACT_INFO_BUTTON);

  if (state()->selected_contact_profile()) {
    return builder.Id(DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION)
        .CreateWithChevron(CreateContactInfoSectionContent(), nullptr);
  } else {
    builder.Id(DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION_BUTTON);
    base::string16 button_string = state()->contact_profiles().size()
                                       ? l10n_util::GetStringUTF16(IDS_CHOOSE)
                                       : l10n_util::GetStringUTF16(IDS_ADD);

    if (state()->contact_profiles().empty()) {
      return builder.CreateWithButton(base::ASCIIToUTF16(""), button_string,
                                      /*button_enabled=*/true);
    } else if (state()->contact_profiles().size() == 1) {
      base::string16 truncated_content =
          state()->contact_profiles()[0]->ConstructInferredLabel(
              {
                  autofill::ADDRESS_HOME_LINE1, autofill::ADDRESS_HOME_LINE2,
                  autofill::ADDRESS_HOME_CITY, autofill::ADDRESS_HOME_STATE,
                  autofill::ADDRESS_HOME_COUNTRY,
              },
              6, state()->GetApplicationLocale());
      return builder.CreateWithButton(truncated_content, button_string,
                                      /*button_enabled=*/true);
    } else {
      base::string16 preview =
          state()->contact_profiles()[0]->ConstructInferredLabel(
              {
                  autofill::ADDRESS_HOME_LINE1, autofill::ADDRESS_HOME_LINE2,
                  autofill::ADDRESS_HOME_CITY, autofill::ADDRESS_HOME_STATE,
                  autofill::ADDRESS_HOME_COUNTRY,
              },
              6, state()->GetApplicationLocale());
      base::string16 format = l10n_util::GetPluralStringFUTF16(
          IDS_PAYMENT_REQUEST_CONTACTS_PREVIEW,
          state()->contact_profiles().size() - 1);
      return builder.CreateWithButton(preview, format,
                                      state()->contact_profiles().size() - 1,
                                      button_string, /*button_enabled=*/true);
    }
  }
}

std::unique_ptr<views::Button>
PaymentSheetViewController::CreateShippingOptionRow() {
  // The Shipping Options row has many different ways of being displayed
  // depending on the state of the dialog and Payment Request.
  // 1. There is a selected shipping address. The website updated the shipping
  //    options.
  //    1.1 There are no available shipping options: don't display the row.
  //    1.2 There are options and one is selected: display the row with the
  //        selection's label and a chevron.
  //    1.3 There are options and none is selected: display a row with a
  //        choose button and the string "|preview of first option| and N more"
  // 2. There is no selected shipping address: display a row with the string
  //    "Choose an address" and a disabled Choose button.
  mojom::PaymentShippingOption* selected_option =
      spec()->selected_shipping_option();
  // The shipping option section displays the currently selected option if there
  // is one. It's not possible to select an option without selecting an address
  // first.
  PaymentSheetRowBuilder builder(
      this, GetShippingOptionSectionString(spec()->shipping_type()),
      widest_name_column_view_width_);
  builder.Tag(PaymentSheetViewControllerTags::SHOW_SHIPPING_OPTION_BUTTON);

  if (state()->selected_shipping_profile()) {
    if (spec()->details().shipping_options.empty()) {
      // TODO(anthonyvd): Display placeholder if there's no available shipping
      // option.
      return nullptr;
    }

    if (selected_option) {
      std::unique_ptr<views::View> option_row_content =
          current_update_reason_ ==
                  PaymentRequestSpec::UpdateReason::SHIPPING_OPTION
              ? CreateCheckingSpinnerView()
              : CreateShippingOptionLabel(selected_option,
                                          spec()->GetFormattedCurrencyAmount(
                                              selected_option->amount->value));
      return builder.Id(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION)
          .CreateWithChevron(std::move(option_row_content), nullptr);
    } else {
      return builder
          .Id(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON)
          .CreateWithButton(
              base::UTF8ToUTF16(spec()->details().shipping_options[0]->label),
              l10n_util::GetPluralStringFUTF16(
                  IDS_PAYMENT_REQUEST_SHIPPING_OPTIONS_PREVIEW,
                  spec()->details().shipping_options.size() - 1),
              spec()->details().shipping_options.size() - 1,
              l10n_util::GetStringUTF16(IDS_CHOOSE),
              /*button_enabled=*/true);
    }
  } else {
    return builder
        .Id(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION_BUTTON)
        .CreateWithButton(
            l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_CHOOSE_AN_ADDRESS),
            l10n_util::GetStringUTF16(IDS_CHOOSE), /*button_enabled=*/false);
  }
}

}  // namespace payments
