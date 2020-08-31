// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace attestation {

//========================= TpmChallengeKeyFactory =============================

TpmChallengeKey* TpmChallengeKeyFactory::next_result_for_testing_ = nullptr;

// static
std::unique_ptr<TpmChallengeKey> TpmChallengeKeyFactory::Create() {
  if (UNLIKELY(next_result_for_testing_)) {
    std::unique_ptr<TpmChallengeKey> result(next_result_for_testing_);
    next_result_for_testing_ = nullptr;
    return result;
  }

  return std::make_unique<TpmChallengeKeyImpl>();
}

// static
void TpmChallengeKeyFactory::SetForTesting(
    std::unique_ptr<TpmChallengeKey> next_result) {
  // unique_ptr itself cannot be stored in a static variable because of its
  // complex destructor.
  next_result_for_testing_ = next_result.release();
}

//=========================== TpmChallengeKeyImpl ==============================

void TpmChallengeKey::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAttestationEnabled, false);
}

TpmChallengeKeyImpl::TpmChallengeKeyImpl() {
  tpm_challenge_key_subtle_ = TpmChallengeKeySubtleFactory::Create();
}

TpmChallengeKeyImpl::TpmChallengeKeyImpl(
    AttestationFlow* attestation_flow_for_testing) {
  tpm_challenge_key_subtle_ =
      std::make_unique<TpmChallengeKeySubtleImpl>(attestation_flow_for_testing);
}

TpmChallengeKeyImpl::~TpmChallengeKeyImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeyImpl::BuildResponse(AttestationKeyType key_type,
                                        Profile* profile,
                                        TpmChallengeKeyCallback callback,
                                        const std::string& challenge,
                                        bool register_key,
                                        const std::string& key_name_for_spkac) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  // For device key: if |register_key| is true, |key_name_for_spkac| should not
  // be empty; if |register_key| is false, |key_name_for_spkac| is not used.
  DCHECK((key_type != KEY_DEVICE) ||
         (register_key == !key_name_for_spkac.empty()))
      << "Invalid arguments: " << register_key << " "
      << !key_name_for_spkac.empty();

  register_key_ = register_key;
  challenge_ = challenge;
  callback_ = std::move(callback);

  // Empty |key_name| means that some default name will be used.
  tpm_challenge_key_subtle_->StartPrepareKeyStep(
      key_type, /*key_name=*/std::string(), profile, key_name_for_spkac,
      base::BindOnce(&TpmChallengeKeyImpl::OnPrepareKeyDone,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::OnPrepareKeyDone(
    const TpmChallengeKeyResult& prepare_key_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prepare_key_result.IsSuccess()) {
    std::move(callback_).Run(prepare_key_result);
    return;
  }

  tpm_challenge_key_subtle_->StartSignChallengeStep(
      challenge_, /*include_signed_public_key=*/register_key_,
      base::BindOnce(&TpmChallengeKeyImpl::OnSignChallengeDone,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::OnSignChallengeDone(
    const TpmChallengeKeyResult& sign_challenge_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!register_key_ || !sign_challenge_result.IsSuccess()) {
    std::move(callback_).Run(sign_challenge_result);
    return;
  }

  tpm_challenge_key_subtle_->StartRegisterKeyStep(
      base::BindOnce(&TpmChallengeKeyImpl::OnRegisterKeyDone,
                     weak_factory_.GetWeakPtr(), sign_challenge_result));
}

void TpmChallengeKeyImpl::OnRegisterKeyDone(
    const TpmChallengeKeyResult& challenge_response,
    const TpmChallengeKeyResult& register_key_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If StartRegisterKeyStep failed, |register_key_result| contains an error
  // about it.
  if (!register_key_result.IsSuccess()) {
    std::move(callback_).Run(register_key_result);
    return;
  }

  // All steps succeeded, return the final result. The challenge response that
  // is expected from |BuildResponse| was received in |OnSignChallengeDone|, so
  // return it now.
  std::move(callback_).Run(challenge_response);
}

}  // namespace attestation
}  // namespace chromeos
