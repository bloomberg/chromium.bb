// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_save_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_comparator.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/identity_provider.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// If |name| consists of three whitespace-separated parts and the second of the
// three parts is a single character or a single character followed by a period,
// returns the result of joining the first and third parts with a space.
// Otherwise, returns |name|.
//
// Note that a better way to do this would be to use SplitName from
// src/components/autofill/core/browser/contact_info.cc. However, for now we
// want the logic of which variations of names are considered to be the same to
// exactly match the logic applied on the Payments server.
base::string16 RemoveMiddleInitial(const base::string16& name) {
  std::vector<base::StringPiece16> parts =
      base::SplitStringPiece(name, base::kWhitespaceUTF16,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 3 && (parts[1].length() == 1 ||
                            (parts[1].length() == 2 &&
                             base::EndsWith(parts[1], base::ASCIIToUTF16("."),
                                            base::CompareCase::SENSITIVE)))) {
    parts.erase(parts.begin() + 1);
    return base::JoinString(parts, base::ASCIIToUTF16(" "));
  }
  return name;
}

}  // namespace

CreditCardSaveManager::CreditCardSaveManager(
    AutofillClient* client,
    payments::PaymentsClient* payments_client,
    const std::string& app_locale,
    PersonalDataManager* personal_data_manager)
    : client_(client),
      payments_client_(payments_client),
      app_locale_(app_locale),
      personal_data_manager_(personal_data_manager),
      weak_ptr_factory_(this) {
  if (payments_client_) {
    payments_client_->SetSaveDelegate(this);
  }
}

CreditCardSaveManager::~CreditCardSaveManager() {}

void CreditCardSaveManager::OfferCardLocalSave(const CreditCard& card) {
  client_->ConfirmSaveCreditCardLocally(
      card, base::Bind(base::IgnoreResult(
                           &PersonalDataManager::SaveImportedCreditCard),
                       base::Unretained(personal_data_manager_), card));
}

void CreditCardSaveManager::AttemptToOfferCardUploadSave(
    const FormStructure& submitted_form,
    const CreditCard& card) {
  upload_request_ = payments::PaymentsClient::UploadRequestDetails();
  upload_request_.card = card;

  // In order to prompt the user to upload their card, we must have both:
  //  1) Card with CVC
  //  2) 1+ recently-used or modified addresses that meet the client-side
  //     validation rules
  // Here we perform both checks before returning or logging anything,
  // because if only one of the two is missing, it may be fixable.

  // Check for a CVC to determine whether we can prompt the user to upload
  // their card. If no CVC is present and the experiment is off, do nothing.
  // We could fall back to a local save but we believe that sometimes offering
  // upload and sometimes offering local save is a confusing user experience.
  // If no CVC and the experiment is on, request CVC from the user in the
  // bubble and save using the provided value.
  found_cvc_field_ = false;
  found_value_in_cvc_field_ = false;
  found_cvc_value_in_non_cvc_field_ = false;

  for (const auto& field : submitted_form) {
    const bool is_valid_cvc = IsValidCreditCardSecurityCode(
        field->value, upload_request_.card.network());
    if (field->Type().GetStorableType() == CREDIT_CARD_VERIFICATION_CODE) {
      found_cvc_field_ = true;
      if (!field->value.empty())
        found_value_in_cvc_field_ = true;
      if (is_valid_cvc) {
        upload_request_.cvc = field->value;
        break;
      }
    } else if (is_valid_cvc &&
               field->Type().GetStorableType() == UNKNOWN_TYPE) {
      found_cvc_value_in_non_cvc_field_ = true;
    }
  }

  // Upload requires that recently used or modified addresses meet the
  // client-side validation rules.
  int upload_decision_metrics =
      SetProfilesForCreditCardUpload(card, &upload_request_);

  pending_upload_request_origin_ = submitted_form.main_frame_origin();

  should_cvc_be_requested_ = false;
  if (upload_request_.cvc.empty()) {
    should_cvc_be_requested_ =
        (!upload_decision_metrics &&
         IsAutofillUpstreamRequestCvcIfMissingExperimentEnabled());
    if (should_cvc_be_requested_) {
      upload_request_.active_experiments.push_back(
          kAutofillUpstreamRequestCvcIfMissing.name);
    } else {
      upload_decision_metrics |= GetCVCCardUploadDecisionMetric();
    }
  }
  if (upload_decision_metrics) {
    LogCardUploadDecisions(upload_decision_metrics);
    pending_upload_request_origin_ = url::Origin();
    return;
  }

  if (IsAutofillUpstreamShowNewUiExperimentEnabled()) {
    upload_request_.active_experiments.push_back(
        kAutofillUpstreamShowNewUi.name);
  }
  if (IsAutofillUpstreamShowGoogleLogoExperimentEnabled()) {
    upload_request_.active_experiments.push_back(
        kAutofillUpstreamShowGoogleLogo.name);
  }

  // All required data is available, start the upload process.
  payments_client_->GetUploadDetails(upload_request_.profiles,
                                     upload_request_.active_experiments,
                                     app_locale_);
}

bool CreditCardSaveManager::IsCreditCardUploadEnabled() {
  return ::autofill::IsCreditCardUploadEnabled(
      client_->GetPrefs(), client_->GetSyncService(),
      client_->GetIdentityProvider()->GetActiveUsername());
}

void CreditCardSaveManager::OnDidUploadCard(
    AutofillClient::PaymentsRpcResult result,
    const std::string& server_id) {
  // We don't do anything user-visible if the upload attempt fails. If the
  // upload succeeds and we can store unmasked cards on this OS, we will keep a
  // copy of the card as a full server card on the device.
  if (result == AutofillClient::SUCCESS && !server_id.empty() &&
      OfferStoreUnmaskedCards()) {
    upload_request_.card.set_record_type(CreditCard::FULL_SERVER_CARD);
    upload_request_.card.SetServerStatus(CreditCard::OK);
    upload_request_.card.set_server_id(server_id);
    DCHECK(personal_data_manager_);
    if (personal_data_manager_)
      personal_data_manager_->AddFullServerCreditCard(upload_request_.card);
  }
}

void CreditCardSaveManager::OnDidGetUploadDetails(
    AutofillClient::PaymentsRpcResult result,
    const base::string16& context_token,
    std::unique_ptr<base::DictionaryValue> legal_message) {
  int upload_decision_metrics;
  if (result == AutofillClient::SUCCESS) {
    // Do *not* call payments_client_->Prepare() here. We shouldn't send
    // credentials until the user has explicitly accepted a prompt to upload.
    upload_request_.context_token = context_token;
    user_did_accept_upload_prompt_ = false;
    client_->ConfirmSaveCreditCardToCloud(
        upload_request_.card, std::move(legal_message),
        should_cvc_be_requested_,
        base::Bind(&CreditCardSaveManager::OnUserDidAcceptUpload,
                   weak_ptr_factory_.GetWeakPtr()));
    client_->LoadRiskData(
        base::Bind(&CreditCardSaveManager::OnDidGetUploadRiskData,
                   weak_ptr_factory_.GetWeakPtr()));
    upload_decision_metrics = AutofillMetrics::UPLOAD_OFFERED;
  } else {
    // If the upload details request failed, fall back to a local save. The
    // reasoning here is as follows:
    // - This will sometimes fail intermittently, in which case it might be
    // better to not fall back, because sometimes offering upload and sometimes
    // offering local save is a poor user experience.
    // - However, in some cases, our local configuration limiting the feature to
    // countries that Payments is known to support will not match Payments' own
    // determination of what country the user is located in. In these cases,
    // the upload details request will consistently fail and if we don't fall
    // back to a local save then the user will never be offered any kind of
    // credit card save.
    client_->ConfirmSaveCreditCardLocally(
        upload_request_.card,
        base::Bind(
            base::IgnoreResult(&PersonalDataManager::SaveImportedCreditCard),
            base::Unretained(personal_data_manager_), upload_request_.card));
    upload_decision_metrics =
        AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED;
  }

  if (!found_cvc_field_ || !found_value_in_cvc_field_)
    DCHECK(should_cvc_be_requested_);

  if (should_cvc_be_requested_)
    upload_decision_metrics |= GetCVCCardUploadDecisionMetric();
  else
    DCHECK(found_cvc_field_ && found_value_in_cvc_field_);

  LogCardUploadDecisions(upload_decision_metrics);
  pending_upload_request_origin_ = url::Origin();
}

int CreditCardSaveManager::SetProfilesForCreditCardUpload(
    const CreditCard& card,
    payments::PaymentsClient::UploadRequestDetails* upload_request) const {
  std::vector<AutofillProfile> candidate_profiles;
  const base::Time now = AutofillClock::Now();
  const base::TimeDelta fifteen_minutes = base::TimeDelta::FromMinutes(15);
  int upload_decision_metrics = 0;
  bool has_profile = false;
  bool has_modified_profile = false;

  // First, collect all of the addresses used or modified recently.
  for (AutofillProfile* profile : personal_data_manager_->GetProfiles()) {
    has_profile = true;
    if ((now - profile->modification_date()) < fifteen_minutes) {
      has_modified_profile = true;
      candidate_profiles.push_back(*profile);
    } else if ((now - profile->use_date()) < fifteen_minutes) {
      candidate_profiles.push_back(*profile);
    }
  }

  AutofillMetrics::LogHasModifiedProfileOnCreditCardFormSubmission(
      has_modified_profile);

  if (candidate_profiles.empty()) {
    upload_decision_metrics |=
        has_profile
            ? AutofillMetrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS
            : AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE;
  }

  std::unique_ptr<AutofillProfileComparator> comparator;
  if (!candidate_profiles.empty() &&
      (base::FeatureList::IsEnabled(
          kAutofillUpstreamUseAutofillProfileComparator)))
    comparator = std::make_unique<AutofillProfileComparator>(app_locale_);

  // If any of the names on the card or the addresses don't match the
  // candidate set is invalid. This matches the rules for name matching applied
  // server-side by Google Payments and ensures that we don't send upload
  // requests that are guaranteed to fail.
  const base::string16 card_name =
      card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), app_locale_);
  base::string16 verified_name;
  if (candidate_profiles.empty()) {
    verified_name = card_name;
  } else {
    bool found_conflicting_names = false;
    if (comparator) {
      upload_request->active_experiments.push_back(
          kAutofillUpstreamUseAutofillProfileComparator.name);
      verified_name = comparator->NormalizeForComparison(card_name);
      for (const AutofillProfile& profile : candidate_profiles) {
        const base::string16 address_name = comparator->NormalizeForComparison(
            profile.GetInfo(NAME_FULL, app_locale_));
        if (address_name.empty())
          continue;
        if (verified_name.empty() ||
            comparator->IsNameVariantOf(address_name, verified_name)) {
          verified_name = address_name;
        } else if (!comparator->IsNameVariantOf(verified_name, address_name)) {
          found_conflicting_names = true;
          break;
        }
      }
    } else {
      verified_name = RemoveMiddleInitial(card_name);
      for (const AutofillProfile& profile : candidate_profiles) {
        const base::string16 address_name =
            RemoveMiddleInitial(profile.GetInfo(NAME_FULL, app_locale_));
        if (address_name.empty())
          continue;
        if (verified_name.empty()) {
          verified_name = address_name;
        } else if (!base::EqualsCaseInsensitiveASCII(verified_name,
                                                     address_name)) {
          found_conflicting_names = true;
          break;
        }
      }
    }
    if (found_conflicting_names) {
      upload_decision_metrics |=
          AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES;
    }
  }

  // If neither the card nor any of the addresses have a name associated with
  // them, the candidate set is invalid.
  if (verified_name.empty()) {
    upload_decision_metrics |= AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME;
  }

  // If any of the candidate addresses have a non-empty zip that doesn't match
  // any other non-empty zip, then the candidate set is invalid.
  base::string16 verified_zip;
  base::string16 normalized_verified_zip;
  const AutofillType kZipCode(ADDRESS_HOME_ZIP);
  for (const AutofillProfile& profile : candidate_profiles) {
    const base::string16 zip = comparator
                                   ? profile.GetInfo(kZipCode, app_locale_)
                                   : profile.GetRawInfo(ADDRESS_HOME_ZIP);
    const base::string16 normalized_zip =
        comparator ? comparator->NormalizeForComparison(
                         zip, AutofillProfileComparator::DISCARD_WHITESPACE)
                   : base::string16();
    if (!zip.empty()) {
      if (verified_zip.empty()) {
        verified_zip = zip;
        normalized_verified_zip = normalized_zip;
      } else if (comparator) {
        if (normalized_zip.find(normalized_verified_zip) ==
                base::string16::npos &&
            normalized_verified_zip.find(normalized_zip) ==
                base::string16::npos) {
          upload_decision_metrics |=
              AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS;
          break;
        }
      } else {
        // To compare two zips, we check to see if either is a prefix of the
        // other. This allows us to consider a 5-digit zip and a zip+4 to be a
        // match if the first 5 digits are the same without hardcoding any
        // specifics of how postal codes are represented. (They can be numeric
        // or alphanumeric and vary from 3 to 10 digits long by country. See
        // https://en.wikipedia.org/wiki/Postal_code#Presentation.) The Payments
        // backend will apply a more sophisticated address-matching procedure.
        // This check is simply meant to avoid offering upload in cases that are
        // likely to fail.
        if (!(StartsWith(verified_zip, zip, base::CompareCase::SENSITIVE) ||
              StartsWith(zip, verified_zip, base::CompareCase::SENSITIVE))) {
          upload_decision_metrics |=
              AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS;
          break;
        }
      }
    }
  }

  // If none of the candidate addresses have a zip, the candidate set is
  // invalid.
  if (verified_zip.empty() && !candidate_profiles.empty())
    upload_decision_metrics |= AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE;

  if (!upload_decision_metrics) {
    upload_request->profiles.assign(candidate_profiles.begin(),
                                    candidate_profiles.end());
    if (!has_modified_profile)
      for (const AutofillProfile& profile : candidate_profiles)
        UMA_HISTOGRAM_COUNTS_1000(
            "Autofill.DaysSincePreviousUseAtSubmission.Profile",
            (profile.use_date() - profile.previous_use_date()).InDays());
  }
  return upload_decision_metrics;
}

void CreditCardSaveManager::OnUserDidAcceptUpload() {
  user_did_accept_upload_prompt_ = true;
  // Populating risk data and offering upload occur asynchronously.
  // If |risk_data| has already been loaded, send the upload card request.
  // Otherwise, continue to wait and let OnDidGetUploadRiskData handle it.
  if (!upload_request_.risk_data.empty())
    SendUploadCardRequest();
}

void CreditCardSaveManager::OnDidGetUploadRiskData(
    const std::string& risk_data) {
  upload_request_.risk_data = risk_data;
  // Populating risk data and offering upload occur asynchronously.
  // If the dialog has already been accepted, send the upload card request.
  // Otherwise, continue to wait for the user to accept the save dialog.
  if (user_did_accept_upload_prompt_)
    SendUploadCardRequest();
}

void CreditCardSaveManager::SendUploadCardRequest() {
  upload_request_.app_locale = app_locale_;
  // If the upload request does not have card CVC, populate it with the value
  // provided by the user:
  if (upload_request_.cvc.empty() &&
      IsAutofillUpstreamRequestCvcIfMissingExperimentEnabled()) {
    DCHECK(client_->GetSaveCardBubbleController());
    upload_request_.cvc =
        client_->GetSaveCardBubbleController()->GetCvcEnteredByUser();
  }
  if (IsAutofillSendBillingCustomerNumberExperimentEnabled()) {
    upload_request_.billing_customer_number =
        static_cast<int64_t>(payments_client_->GetPrefService()->GetDouble(
            prefs::kAutofillBillingCustomerNumber));
  }
  payments_client_->UploadCard(upload_request_);
}

AutofillMetrics::CardUploadDecisionMetric
CreditCardSaveManager::GetCVCCardUploadDecisionMetric() const {
  if (found_cvc_field_) {
    return found_value_in_cvc_field_ ? AutofillMetrics::INVALID_CVC_VALUE
                                     : AutofillMetrics::CVC_VALUE_NOT_FOUND;
  }
  return found_cvc_value_in_non_cvc_field_
             ? AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD
             : AutofillMetrics::CVC_FIELD_NOT_FOUND;
}

void CreditCardSaveManager::LogCardUploadDecisions(
    int upload_decision_metrics) {
  AutofillMetrics::LogCardUploadDecisionMetrics(upload_decision_metrics);
  AutofillMetrics::LogCardUploadDecisionsUkm(
      client_->GetUkmRecorder(), pending_upload_request_origin_.GetURL(),
      upload_decision_metrics);
}

}  // namespace autofill
