// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_verifier.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/install_signer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum VerifyStatus {
  NONE = 0,   // Do not request install signatures, and do not enforce them.
  BOOTSTRAP,  // Request install signatures, but do not enforce them.
  ENFORCE,    // Request install signatures, and enforce them.

  // This is used in histograms - do not remove or reorder entries above! Also
  // the "MAX" item below should always be the last element.

  VERIFY_STATUS_MAX
};

#if defined(GOOGLE_CHROME_BUILD)
const char kExperimentName[] = "ExtensionInstallVerification";
#endif  // defined(GOOGLE_CHROME_BUILD)

VerifyStatus GetExperimentStatus() {
#if defined(GOOGLE_CHROME_BUILD)
  const std::string group = base::FieldTrialList::FindFullName(
      kExperimentName);

  std::string forced_trials = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kForceFieldTrials);
  if (forced_trials.find(kExperimentName) != std::string::npos) {
    // We don't want to allow turning off enforcement by forcing the field
    // trial group to something other than enforcement.
    return ENFORCE;
  }

  VerifyStatus default_status = NONE;

  if (group == "Enforce")
    return ENFORCE;
  else if (group == "Bootstrap")
    return BOOTSTRAP;
  else if (group == "None" || group == "Control")
    return NONE;
  else
    return default_status;
#endif  // defined(GOOGLE_CHROME_BUILD)

  return NONE;
}

VerifyStatus GetCommandLineStatus() {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (!extensions::InstallSigner::GetForcedNotFromWebstore().empty())
    return ENFORCE;

  if (cmdline->HasSwitch(switches::kExtensionsInstallVerification)) {
    std::string value = cmdline->GetSwitchValueASCII(
        switches::kExtensionsInstallVerification);
    if (value == "bootstrap")
      return BOOTSTRAP;
    else
      return ENFORCE;
  }

  return NONE;
}

VerifyStatus GetStatus() {
  return std::max(GetExperimentStatus(), GetCommandLineStatus());
}

bool ShouldFetchSignature() {
  VerifyStatus status = GetStatus();
  return (status == BOOTSTRAP || status == ENFORCE);
}

bool ShouldEnforce() {
  return GetStatus() == ENFORCE;
}

}  // namespace

namespace extensions {

InstallVerifier::InstallVerifier(ExtensionPrefs* prefs,
                                 net::URLRequestContextGetter* context_getter)
    : prefs_(prefs), context_getter_(context_getter) {
}

InstallVerifier::~InstallVerifier() {}

namespace {

enum InitResult {
  INIT_NO_PREF = 0,
  INIT_UNPARSEABLE_PREF,
  INIT_INVALID_SIGNATURE,
  INIT_VALID_SIGNATURE,

  // This is used in histograms - do not remove or reorder entries above! Also
  // the "MAX" item below should always be the last element.

  INIT_RESULT_MAX
};

void LogInitResultHistogram(InitResult result) {
  UMA_HISTOGRAM_ENUMERATION("ExtensionInstallVerifier.InitResult",
                            result, INIT_RESULT_MAX);
}

bool FromStore(const Extension& extension) {
  bool updates_from_store = ManifestURL::UpdatesFromGallery(&extension);
  return extension.from_webstore() || updates_from_store;
}

bool CanUseExtensionApis(const Extension& extension) {
  return extension.is_extension() || extension.is_legacy_packaged_app();
}

}  // namespace

// static
bool InstallVerifier::NeedsVerification(const Extension& extension) {
  return FromStore(extension) && CanUseExtensionApis(extension);
}

void InstallVerifier::Init() {
  UMA_HISTOGRAM_ENUMERATION("ExtensionInstallVerifier.ExperimentStatus",
                            GetExperimentStatus(), VERIFY_STATUS_MAX);
  UMA_HISTOGRAM_ENUMERATION("ExtensionInstallVerifier.ActualStatus",
                            GetStatus(), VERIFY_STATUS_MAX);

  const base::DictionaryValue* pref = prefs_->GetInstallSignature();
  if (pref) {
    scoped_ptr<InstallSignature> signature_from_prefs =
        InstallSignature::FromValue(*pref);
    if (!signature_from_prefs.get()) {
      LogInitResultHistogram(INIT_UNPARSEABLE_PREF);
    } else if (!InstallSigner::VerifySignature(*signature_from_prefs.get())) {
      LogInitResultHistogram(INIT_INVALID_SIGNATURE);
      DVLOG(1) << "Init - ignoring invalid signature";
    } else {
      signature_ = signature_from_prefs.Pass();
      LogInitResultHistogram(INIT_VALID_SIGNATURE);
      UMA_HISTOGRAM_COUNTS_100("ExtensionInstallVerifier.InitSignatureCount",
                               signature_->ids.size());
      GarbageCollect();
    }
  } else {
    LogInitResultHistogram(INIT_NO_PREF);
  }
}

bool InstallVerifier::NeedsBootstrap() {
  return signature_.get() == NULL && ShouldFetchSignature();
}

base::Time InstallVerifier::SignatureTimestamp() {
  if (signature_.get())
    return signature_->timestamp;
  else
    return base::Time();
}

void InstallVerifier::Add(const std::string& id,
                          const AddResultCallback& callback) {
  ExtensionIdSet ids;
  ids.insert(id);
  AddMany(ids, callback);
}

void InstallVerifier::AddMany(const ExtensionIdSet& ids,
                              const AddResultCallback& callback) {
  if (!ShouldFetchSignature()) {
    if (!callback.is_null())
      callback.Run(true);
    return;
  }

  if (signature_.get()) {
    ExtensionIdSet not_allowed_yet =
        base::STLSetDifference<ExtensionIdSet>(ids, signature_->ids);
    if (not_allowed_yet.empty()) {
      if (!callback.is_null())
        callback.Run(true);
      return;
    }
  }

  InstallVerifier::PendingOperation* operation =
    new InstallVerifier::PendingOperation();
  operation->type = InstallVerifier::ADD;
  operation->ids.insert(ids.begin(), ids.end());
  operation->callback = callback;

  operation_queue_.push(linked_ptr<PendingOperation>(operation));

  // If there are no ongoing pending requests, we need to kick one off.
  if (operation_queue_.size() == 1)
    BeginFetch();
}

void InstallVerifier::AddProvisional(const ExtensionIdSet& ids) {
  provisional_.insert(ids.begin(), ids.end());
  AddMany(ids, AddResultCallback());
}

void InstallVerifier::Remove(const std::string& id) {
  ExtensionIdSet ids;
  ids.insert(id);
  RemoveMany(ids);
}

void InstallVerifier::RemoveMany(const ExtensionIdSet& ids) {
  if (!signature_.get() || !ShouldFetchSignature())
    return;

  bool found_any = false;
  for (ExtensionIdSet::const_iterator i = ids.begin(); i != ids.end(); ++i) {
    if (ContainsKey(signature_->ids, *i)) {
      found_any = true;
      break;
    }
  }
  if (!found_any)
    return;

  InstallVerifier::PendingOperation* operation =
    new InstallVerifier::PendingOperation();
  operation->type = InstallVerifier::REMOVE;
  operation->ids = ids;

  operation_queue_.push(linked_ptr<PendingOperation>(operation));
  if (operation_queue_.size() == 1)
    BeginFetch();
}

std::string InstallVerifier::GetDebugPolicyProviderName() const {
  return std::string("InstallVerifier");
}

namespace {

enum MustRemainDisabledOutcome {
  VERIFIED = 0,
  NOT_EXTENSION,
  UNPACKED,
  ENTERPRISE_POLICY_ALLOWED,
  FORCED_NOT_VERIFIED,
  NOT_FROM_STORE,
  NO_SIGNATURE,
  NOT_VERIFIED_BUT_NOT_ENFORCING,
  NOT_VERIFIED,
  NOT_VERIFIED_BUT_INSTALL_TIME_NEWER_THAN_SIGNATURE,

  // This is used in histograms - do not remove or reorder entries above! Also
  // the "MAX" item below should always be the last element.
  MUST_REMAIN_DISABLED_OUTCOME_MAX
};

void MustRemainDisabledHistogram(MustRemainDisabledOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("ExtensionInstallVerifier.MustRemainDisabled",
                            outcome, MUST_REMAIN_DISABLED_OUTCOME_MAX);
}

}  // namespace

bool InstallVerifier::MustRemainDisabled(const Extension* extension,
                                         Extension::DisableReason* reason,
                                         base::string16* error) const {
  CHECK(extension);
  if (!CanUseExtensionApis(*extension)) {
    MustRemainDisabledHistogram(NOT_EXTENSION);
    return false;
  }
  if (Manifest::IsUnpackedLocation(extension->location())) {
    MustRemainDisabledHistogram(UNPACKED);
    return false;
  }
  if (AllowedByEnterprisePolicy(extension->id())) {
    MustRemainDisabledHistogram(ENTERPRISE_POLICY_ALLOWED);
    return false;
  }

  bool verified = true;
  MustRemainDisabledOutcome outcome = VERIFIED;
  if (ContainsKey(InstallSigner::GetForcedNotFromWebstore(), extension->id())) {
    verified = false;
    outcome = FORCED_NOT_VERIFIED;
  } else if (!FromStore(*extension)) {
    verified = false;
    outcome = NOT_FROM_STORE;
  } else if (signature_.get() == NULL) {
    // If we don't have a signature yet, we'll temporarily consider every
    // extension from the webstore verified to avoid false positives on existing
    // profiles hitting this code for the first time, and rely on consumers of
    // this class to check NeedsBootstrap() and schedule a first check so we can
    // get a signature.
    outcome = NO_SIGNATURE;
  } else if (!IsVerified(extension->id())) {
    if (WasInstalledAfterSignature(extension->id())) {
      outcome = NOT_VERIFIED_BUT_INSTALL_TIME_NEWER_THAN_SIGNATURE;
    } else {
      verified = false;
      outcome = NOT_VERIFIED;
    }
  }
  if (!verified && !ShouldEnforce()) {
    verified = true;
    outcome = NOT_VERIFIED_BUT_NOT_ENFORCING;
  }
  MustRemainDisabledHistogram(outcome);

  if (!verified) {
    if (reason)
      *reason = Extension::DISABLE_NOT_VERIFIED;
    if (error)
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
  }
  return !verified;
}

InstallVerifier::PendingOperation::PendingOperation() {
  type = InstallVerifier::ADD;
}

InstallVerifier::PendingOperation::~PendingOperation() {
}

void InstallVerifier::GarbageCollect() {
  if (!ShouldFetchSignature()) {
    return;
  }
  CHECK(signature_.get());
  ExtensionIdSet leftovers = signature_->ids;
  ExtensionIdList all_ids;
  prefs_->GetExtensions(&all_ids);
  for (ExtensionIdList::const_iterator i = all_ids.begin();
       i != all_ids.end(); ++i) {
    ExtensionIdSet::iterator found = leftovers.find(*i);
    if (found != leftovers.end())
      leftovers.erase(found);
  }
  if (!leftovers.empty()) {
    RemoveMany(leftovers);
  }
}

bool InstallVerifier::AllowedByEnterprisePolicy(const std::string& id) const {
  PrefService* pref_service = prefs_->pref_service();
  if (pref_service->IsManagedPreference(pref_names::kInstallAllowList)) {
    const base::ListValue* whitelist =
        pref_service->GetList(pref_names::kInstallAllowList);
    base::StringValue id_value(id);
    if (whitelist && whitelist->Find(id_value) != whitelist->end())
      return true;
  }
  if (pref_service->IsManagedPreference(pref_names::kInstallForceList)) {
    const base::DictionaryValue* forcelist =
        pref_service->GetDictionary(pref_names::kInstallForceList);
    if (forcelist && forcelist->HasKey(id))
      return true;
  }
  return false;
}

bool InstallVerifier::IsVerified(const std::string& id) const {
  return ((signature_.get() && ContainsKey(signature_->ids, id)) ||
          ContainsKey(provisional_, id));
}

bool InstallVerifier::WasInstalledAfterSignature(const std::string& id) const {
  if (!signature_.get() || signature_->timestamp.is_null())
    return true;

  base::Time install_time = prefs_->GetInstallTime(id);
  // If the extension install time is in the future, just assume it isn't
  // newer than the signature. (Either the clock went backwards, or
  // an attacker changed the install time in the preferences).
  if (install_time >= base::Time::Now())
    return false;
  return install_time > signature_->timestamp;
}

void InstallVerifier::BeginFetch() {
  DCHECK(ShouldFetchSignature());

  // TODO(asargent) - It would be possible to coalesce all operations in the
  // queue into one fetch - we'd probably just need to change the queue to
  // hold (set of ids, list of callbacks) pairs.
  CHECK(!operation_queue_.empty());
  const PendingOperation& operation = *operation_queue_.front();

  ExtensionIdSet ids_to_sign;
  if (signature_.get()) {
    ids_to_sign.insert(signature_->ids.begin(), signature_->ids.end());
  }
  if (operation.type == InstallVerifier::ADD) {
    ids_to_sign.insert(operation.ids.begin(), operation.ids.end());
  } else {
    for (ExtensionIdSet::const_iterator i = operation.ids.begin();
         i != operation.ids.end(); ++i) {
      if (ContainsKey(ids_to_sign, *i))
        ids_to_sign.erase(*i);
    }
  }

  signer_.reset(new InstallSigner(context_getter_, ids_to_sign));
  signer_->GetSignature(base::Bind(&InstallVerifier::SignatureCallback,
                                   base::Unretained(this)));
}

void InstallVerifier::SaveToPrefs() {
  if (signature_.get())
    DCHECK(InstallSigner::VerifySignature(*signature_));

  if (!signature_.get() || signature_->ids.empty()) {
    DVLOG(1) << "SaveToPrefs - saving NULL";
    prefs_->SetInstallSignature(NULL);
  } else {
    base::DictionaryValue pref;
    signature_->ToValue(&pref);
    if (VLOG_IS_ON(1)) {
      DVLOG(1) << "SaveToPrefs - saving";

      DCHECK(InstallSigner::VerifySignature(*signature_.get()));
      scoped_ptr<InstallSignature> rehydrated =
          InstallSignature::FromValue(pref);
      DCHECK(InstallSigner::VerifySignature(*rehydrated.get()));
    }
    prefs_->SetInstallSignature(&pref);
  }
}

namespace {

enum CallbackResult {
  CALLBACK_NO_SIGNATURE = 0,
  CALLBACK_INVALID_SIGNATURE,
  CALLBACK_VALID_SIGNATURE,

  // This is used in histograms - do not remove or reorder entries above! Also
  // the "MAX" item below should always be the last element.

  CALLBACK_RESULT_MAX
};

void GetSignatureResultHistogram(CallbackResult result) {
  UMA_HISTOGRAM_ENUMERATION("ExtensionInstallVerifier.GetSignatureResult",
                            result, CALLBACK_RESULT_MAX);
}

}  // namespace

void InstallVerifier::SignatureCallback(
    scoped_ptr<InstallSignature> signature) {

  linked_ptr<PendingOperation> operation = operation_queue_.front();
  operation_queue_.pop();

  bool success = false;
  if (!signature.get()) {
    GetSignatureResultHistogram(CALLBACK_NO_SIGNATURE);
  } else if (!InstallSigner::VerifySignature(*signature)) {
    GetSignatureResultHistogram(CALLBACK_INVALID_SIGNATURE);
  } else {
    GetSignatureResultHistogram(CALLBACK_VALID_SIGNATURE);
    success = true;
  }

  if (!success) {
    if (!operation->callback.is_null())
      operation->callback.Run(false);

    // TODO(asargent) - if this was something like a network error, we need to
    // do retries with exponential back off.
  } else {
    signature_ = signature.Pass();
    SaveToPrefs();

    if (!provisional_.empty()) {
      // Update |provisional_| to remove ids that were successfully signed.
      provisional_ = base::STLSetDifference<ExtensionIdSet>(
          provisional_, signature_->ids);
    }

    if (!operation->callback.is_null())
      operation->callback.Run(success);
  }

  if (!operation_queue_.empty())
    BeginFetch();
}


}  // namespace extensions
