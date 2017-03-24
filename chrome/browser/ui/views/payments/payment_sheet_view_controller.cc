// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_instrument.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
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

// Creates a clickable row to be displayed in the Payment Sheet. It contains
// a section name and some content, followed by a chevron as a clickability
// affordance. Both, either, or none of |content_view| and |extra_content_view|
// may be present, the difference between the two being that content is pinned
// to the left and extra_content is pinned to the right.
// The row also displays a light gray horizontal ruler on its lower boundary.
// The name column has a fixed width equal to |name_column_width|.
// +----------------------------+
// | Name | Content | Extra | > |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- ruler
std::unique_ptr<views::Button> CreatePaymentSheetRow(
    views::ButtonListener* listener,
    const base::string16& section_name,
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<views::View> extra_content_view,
    int name_column_width) {
  std::unique_ptr<PaymentRequestRowView> row =
      base::MakeUnique<PaymentRequestRowView>(listener);
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
  // A column for the chevron.
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

  views::ImageView* chevron = new views::ImageView();
  chevron->set_can_process_events_within_subtree(false);
  chevron->SetImage(gfx::CreateVectorIcon(
      views::kSubmenuArrowIcon,
      color_utils::DeriveDefaultIconColor(name_label->enabled_color())));
  layout->AddView(chevron);

  return std::move(row);
}

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

}  // namespace

PaymentSheetViewController::PaymentSheetViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(spec, state, dialog),
      pay_button_(nullptr),
      widest_name_column_view_width_(ComputeWidestNameColumnViewWidth()) {
  state->AddObserver(this);
}

PaymentSheetViewController::~PaymentSheetViewController() {
  state()->RemoveObserver(this);
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
  return l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_PAYMENT_SHEET_TITLE);
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
    layout->StartRow(0, 0);
    layout->AddView(CreateShippingOptionRow().release());
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
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
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
    item_summaries_layout->AddView(
        new views::Label(base::ASCIIToUTF16(items[i]->label)));

    item_amounts_layout->StartRow(0, 0);
    item_amounts_layout->AddView(new views::Label(
        spec()->GetFormattedCurrencyAmount(items[i]->amount->value)));
  }

  int hidden_item_count = items.size() - kMaxNumberOfItemsShown;
  if (hidden_item_count > 0) {
    item_summaries_layout->StartRow(0, 0);
    std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
        l10n_util::GetStringFUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_MORE_ITEMS,
                                   base::IntToString16(hidden_item_count)));
    label->SetDisabledColor(label->GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_LabelDisabledColor));
    label->SetEnabled(false);
    item_summaries_layout->AddView(label.release());

    item_amounts_layout->StartRow(0, 0);
    item_amounts_layout->AddView(new views::Label(base::ASCIIToUTF16("")));
  }

  item_summaries_layout->StartRow(0, 0);
  item_summaries_layout->AddView(
      CreateBoldLabel(base::ASCIIToUTF16(spec()->details().total->label))
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

  std::unique_ptr<views::Button> section = CreatePaymentSheetRow(
      this,
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_NAME),
      std::move(item_summaries), std::move(item_amounts),
      widest_name_column_view_width_);
  section->set_tag(static_cast<int>(
      PaymentSheetViewControllerTags::SHOW_ORDER_SUMMARY_BUTTON));
  section->set_id(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_SUMMARY_SECTION));
  return section;
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateShippingSectionContent() {
  auto* profile = state()->selected_shipping_profile();

  return profile ? payments::GetShippingAddressLabel(
                       AddressStyleType::SUMMARY,
                       state()->GetApplicationLocale(), *profile)
                 : base::MakeUnique<views::Label>(base::string16());
}

// Creates the Shipping row, which contains a "Shipping address" label, the
// user's selected shipping address, and a chevron.
// +----------------------------------------------+
// | Shipping Address   Barack Obama              |
// |                    1600 Pennsylvania Ave.  > |
// |                    1800MYPOTUS               |
// +----------------------------------------------+
std::unique_ptr<views::Button> PaymentSheetViewController::CreateShippingRow() {
  std::unique_ptr<views::Button> section = CreatePaymentSheetRow(
      this, GetShippingAddressSectionString(spec()->options().shipping_type),
      CreateShippingSectionContent(), std::unique_ptr<views::View>(nullptr),
      widest_name_column_view_width_);
  section->set_tag(
      static_cast<int>(PaymentSheetViewControllerTags::SHOW_SHIPPING_BUTTON));
  section->set_id(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_SHIPPING_SECTION));
  return section;
}

// Creates the Payment Method row, which contains a "Payment" label, the user's
// masked Credit Card details, the icon for the selected card, and a chevron.
// +----------------------------------------------+
// | Payment         Visa ****0000                |
// |                 John Smith        | VISA | > |
// |                                              |
// +----------------------------------------------+
std::unique_ptr<views::Button>
PaymentSheetViewController::CreatePaymentMethodRow() {
  PaymentInstrument* selected_instrument = state()->selected_instrument();

  std::unique_ptr<views::View> content_view;
  std::unique_ptr<views::ImageView> card_icon_view;
  if (selected_instrument) {
    content_view = base::MakeUnique<views::View>();

    views::GridLayout* layout = new views::GridLayout(content_view.get());
    content_view->SetLayoutManager(layout);
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       1, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    layout->AddView(new views::Label(selected_instrument->label()));
    layout->StartRow(0, 0);
    layout->AddView(new views::Label(selected_instrument->sublabel()));

    card_icon_view = CreateInstrumentIconView(
        selected_instrument->icon_resource_id(), selected_instrument->label());
    card_icon_view->SetImageSize(gfx::Size(32, 20));
  }

  std::unique_ptr<views::Button> section = CreatePaymentSheetRow(
      this,
      l10n_util::GetStringUTF16(
          IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME),
      std::move(content_view),
      std::move(card_icon_view),
      widest_name_column_view_width_);
  section->set_tag(static_cast<int>(
      PaymentSheetViewControllerTags::SHOW_PAYMENT_METHOD_BUTTON));
  section->set_id(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_PAYMENT_METHOD_SECTION));
  return section;
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateContactInfoSectionContent() {
  autofill::AutofillProfile* profile = state()->selected_contact_profile();
  return profile
             ? payments::GetContactInfoLabel(
                   AddressStyleType::SUMMARY, state()->GetApplicationLocale(),
                   *profile, spec()->request_payer_name(),
                   spec()->request_payer_phone(), spec()->request_payer_email())
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
  std::unique_ptr<views::Button> section = CreatePaymentSheetRow(
      this,
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME),
      CreateContactInfoSectionContent(), std::unique_ptr<views::View>(nullptr),
      widest_name_column_view_width_);
  section->set_tag(static_cast<int>(
      PaymentSheetViewControllerTags::SHOW_CONTACT_INFO_BUTTON));
  section->set_id(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_CONTACT_INFO_SECTION));
  return section;
}

std::unique_ptr<views::Button>
PaymentSheetViewController::CreateShippingOptionRow() {
  payments::mojom::PaymentShippingOption* selected_option =
      state()->selected_shipping_option();
  std::unique_ptr<views::View> option_label = CreateShippingOptionLabel(
      selected_option, selected_option ? spec()->GetFormattedCurrencyAmount(
                                             selected_option->amount->value)
                                       : base::ASCIIToUTF16(""));
  std::unique_ptr<views::Button> section = CreatePaymentSheetRow(
      this, GetShippingOptionSectionString(spec()->options().shipping_type),
      std::move(option_label), std::unique_ptr<views::View>(nullptr),
      widest_name_column_view_width_);
  section->set_tag(static_cast<int>(
      PaymentSheetViewControllerTags::SHOW_SHIPPING_OPTION_BUTTON));
  section->set_id(
      static_cast<int>(DialogViewID::PAYMENT_SHEET_SHIPPING_OPTION_SECTION));
  return section;
}

}  // namespace payments
