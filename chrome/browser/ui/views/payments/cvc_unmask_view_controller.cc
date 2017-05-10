// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/cvc_unmask_view_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/grit/components_scaled_resources.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/payment_request_delegate.h"
#include "components/signin/core/browser/profile_identity_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

enum class Tags {
  CONFIRM_TAG = static_cast<int>(PaymentRequestCommonTags::PAY_BUTTON_TAG),
};

CvcUnmaskViewController::CvcUnmaskViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate,
    content::WebContents* web_contents)
    : PaymentRequestSheetController(spec, state, dialog),
      credit_card_(credit_card),
      web_contents_(web_contents),
      payments_client_(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetRequestContext(),
          this),
      full_card_request_(this,
                         &payments_client_,
                         state->GetPersonalDataManager()),
      weak_ptr_factory_(this) {
  full_card_request_.GetFullCard(
      credit_card,
      autofill::AutofillClient::UnmaskCardReason::UNMASK_FOR_PAYMENT_REQUEST,
      result_delegate, weak_ptr_factory_.GetWeakPtr());
}

CvcUnmaskViewController::~CvcUnmaskViewController() {}

IdentityProvider* CvcUnmaskViewController::GetIdentityProvider() {
  if (!identity_provider_) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext())
            ->GetOriginalProfile();
    identity_provider_.reset(new ProfileIdentityProvider(
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        base::Closure()));
  }

  return identity_provider_.get();
}

void CvcUnmaskViewController::OnDidGetRealPan(
    autofill::AutofillClient::PaymentsRpcResult result,
    const std::string& real_pan) {
  full_card_request_.OnDidGetRealPan(result, real_pan);
}

void CvcUnmaskViewController::OnDidGetUploadDetails(
    autofill::AutofillClient::PaymentsRpcResult result,
    const base::string16& context_token,
    std::unique_ptr<base::DictionaryValue> legal_message) {
  NOTIMPLEMENTED();
}

void CvcUnmaskViewController::OnDidUploadCard(
    autofill::AutofillClient::PaymentsRpcResult result,
    const std::string& server_id) {
  NOTIMPLEMENTED();
}

void CvcUnmaskViewController::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {
  autofill::LoadRiskData(0, web_contents_, callback);
}

void CvcUnmaskViewController::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    autofill::AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  unmask_delegate_ = delegate;
}

void CvcUnmaskViewController::OnUnmaskVerificationResult(
    autofill::AutofillClient::PaymentsRpcResult result) {
  // TODO(crbug.com/716020): Handle FullCardRequest errors with more
  // granularity and display an error in the UI.
}

base::string16 CvcUnmaskViewController::GetSheetTitle() {
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_TITLE,
                                    credit_card_.NetworkAndLastFourDigits());
}

void CvcUnmaskViewController::FillContentView(views::View* content_view) {
  std::unique_ptr<views::GridLayout> layout =
      base::MakeUnique<views::GridLayout>(content_view);
  content_view->SetBorder(views::CreateEmptyBorder(
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets, kPaymentRequestRowHorizontalInsets));

  views::ColumnSet* instructions_columns = layout->AddColumnSet(0);
  instructions_columns->AddColumn(views::GridLayout::Alignment::LEADING,
                                  views::GridLayout::Alignment::LEADING, 1,
                                  views::GridLayout::SizeType::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  std::unique_ptr<views::Label> instructions = base::MakeUnique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_PROMPT_INSTRUCTIONS));
  instructions->SetMultiLine(true);
  instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(instructions.release());

  layout->AddPaddingRow(0, 22);

  views::ColumnSet* cvc_field_columns = layout->AddColumnSet(1);
  cvc_field_columns->AddColumn(views::GridLayout::Alignment::LEADING,
                               views::GridLayout::Alignment::BASELINE, 0,
                               views::GridLayout::SizeType::FIXED, 32, 32);
  cvc_field_columns->AddPaddingColumn(0, 16);
  cvc_field_columns->AddColumn(views::GridLayout::Alignment::FILL,
                               views::GridLayout::Alignment::BASELINE, 0,
                               views::GridLayout::SizeType::FIXED, 80, 80);

  layout->StartRow(0, 1);
  std::unique_ptr<views::ImageView> cvc_image =
      base::MakeUnique<views::ImageView>();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // TODO(anthonyvd): Consider using
  // CardUnmaskPromptControllerImpl::GetCvcImageRid.
  cvc_image->SetImage(rb.GetImageSkiaNamed(
      credit_card_.network() == autofill::kAmericanExpressCard
          ? IDR_CREDIT_CARD_CVC_HINT_AMEX
          : IDR_CREDIT_CARD_CVC_HINT));
  layout->AddView(cvc_image.release());

  std::unique_ptr<views::Textfield> cvc_field =
      base::MakeUnique<views::Textfield>();
  cvc_field->set_id(static_cast<int>(DialogViewID::CVC_PROMPT_TEXT_FIELD));
  cvc_field_ = cvc_field.get();
  layout->AddView(cvc_field.release());

  content_view->SetLayoutManager(layout.release());
}

std::unique_ptr<views::Button> CvcUnmaskViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_CONFIRM)));
  button->set_id(static_cast<int>(DialogViewID::CVC_PROMPT_CONFIRM_BUTTON));
  button->set_tag(static_cast<int>(Tags::CONFIRM_TAG));
  return button;
}

void CvcUnmaskViewController::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(Tags::CONFIRM_TAG):
      CvcConfirmed();
      break;
    case static_cast<int>(PaymentRequestCommonTags::BACK_BUTTON_TAG):
      unmask_delegate_->OnUnmaskPromptClosed();
      dialog()->GoBack();
      break;
    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
  }
}

void CvcUnmaskViewController::CvcConfirmed() {
  const base::string16& cvc = cvc_field_->text();
  if (unmask_delegate_) {
    autofill::CardUnmaskDelegate::UnmaskResponse response;
    response.cvc = cvc;
    unmask_delegate_->OnUnmaskResponse(response);
  }

  dialog()->ShowProcessingSpinner();
}

bool CvcUnmaskViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::CVC_UNMASK_SHEET;
  return true;
}

views::View* CvcUnmaskViewController::GetFirstFocusedView() {
  return cvc_field_;
}

}  // namespace payments
