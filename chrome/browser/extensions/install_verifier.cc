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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_signer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/one_shot_event.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

enum VerifyStatus {
  NONE = 0,   // Do not request install signatures, and do not enforce them.
  BOOTSTRAP,  // Request install signatures, but do not enforce them.
  ENFORCE,    // Request install signatures, and enforce them.
  ENFORCE_STRICT,  // Same as ENFORCE, but hard fail if we can't fetch
                   // signatures.

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
    return ENFORCE_STRICT;
  }

  VerifyStatus default_status = NONE;

  if (group == "EnforceStrict")
    return ENFORCE_STRICT;
  else if (group == "Enforce")
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
  if (!InstallSigner::GetForcedNotFromWebstore().empty())
    return ENFORCE;

  if (cmdline->HasSwitch(switches::kExtensionsInstallVerification)) {
    std::string value = cmdline->GetSwitchValueASCII(
        switches::kExtensionsInstallVerification);
    if (value == "bootstrap")
      return BOOTSTRAP;
    else if (value == "enforce_strict")
      return ENFORCE_STRICT;
    else
      return ENFORCE;
  }

  return NONE;
}

VerifyStatus GetStatus() {
  return std::max(GetExperimentStatus(), GetCommandLineStatus());
}

bool ShouldFetchSignature() {
  return GetStatus() >= BOOTSTRAP;
}

bool ShouldEnforce() {
  return GetStatus() >= ENFORCE;
}

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
  if (extension.from_webstore() || ManifestURL::UpdatesFromGallery(&extension))
    return true;

  // If an extension has no update url, our autoupdate code will ask the
  // webstore about it (to aid in migrating to the webstore from self-hosting
  // or sideloading based installs). So we want to do verification checks on
  // such extensions too so that we don't accidentally disable old installs of
  // extensions that did migrate to the webstore.
  return (ManifestURL::GetUpdateURL(&extension).is_empty() &&
          Manifest::IsAutoUpdateableLocation(extension.location()));
}

bool CanUseExtensionApis(const Extension& extension) {
  return extension.is_extension() || extension.is_legacy_packaged_app();
}

enum VerifyAllSuccess {
  VERIFY_ALL_BOOTSTRAP_SUCCESS = 0,
  VERIFY_ALL_BOOTSTRAP_FAILURE,
  VERIFY_ALL_NON_BOOTSTRAP_SUCCESS,
  VERIFY_ALL_NON_BOOTSTRAP_FAILURE,

  // Used in histograms. Do not remove/reorder any entries above, and the below
  // MAX entry should always come last.
  VERIFY_ALL_SUCCESS_MAX
};

// Record the success or failure of verifying all extensions, and whether or
// not it was a bootstrapping.
void LogVerifyAllSuccessHistogram(bool bootstrap, bool success) {
  VerifyAllSuccess result;
  if (bootstrap && success)
    result = VERIFY_ALL_BOOTSTRAP_SUCCESS;
  else if (bootstrap && !success)
    result = VERIFY_ALL_BOOTSTRAP_FAILURE;
  else if (!bootstrap && success)
    result = VERIFY_ALL_NON_BOOTSTRAP_SUCCESS;
  else
    result = VERIFY_ALL_NON_BOOTSTRAP_FAILURE;

  // This used to be part of ExtensionService, but moved here. In order to keep
  // our histograms accurate, the name is unchanged.
  UMA_HISTOGRAM_ENUMERATION(
      "ExtensionService.VerifyAllSuccess", result, VERIFY_ALL_SUCCESS_MAX);
}

// Record the success or failure of a single verification.
void LogAddVerifiedSuccess(bool success) {
  // This used to be part of ExtensionService, but moved here. In order to keep
  // our histograms accurate, the name is unchanged.
  UMA_HISTOGRAM_BOOLEAN("ExtensionService.AddVerified", success);
}

}  // namespace

InstallVerifier::InstallVerifier(ExtensionPrefs* prefs,
                                 content::BrowserContext* context)
    : prefs_(prefs),
      context_(context),
      bootstrap_check_complete_(false),
      weak_factory_(this) {
}

InstallVerifier::~InstallVerifier() {}

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

  ExtensionSystem::Get(context_)->ready().Post(
      FROM_HERE,
      base::Bind(&InstallVerifier::MaybeBootstrapSelf,
                 weak_factory_.GetWeakPtr()));
}

void InstallVerifier::VerifyAllExtensions() {
  AddMany(GetExtensionsToVerify(), ADD_ALL);
}

base::Time InstallVerifier::SignatureTimestamp() {
  if (signature_.get())
    return signature_->timestamp;
  else
    return base::Time();
}

bool InstallVerifier::IsKnownId(const std::string& id) {
  return signature_.get() && (ContainsKey(signature_->ids, id) ||
                              ContainsKey(signature_->invalid_ids, id));
}

void InstallVerifier::VerifyExtension(const std::string& extension_id) {
  ExtensionIdSet ids;
  ids.insert(extension_id);
  AddMany(ids, ADD_SINGLE);
}

void InstallVerifier::AddMany(const ExtensionIdSet& ids, OperationType type) {
  if (!ShouldFetchSignature()) {
    OnVerificationComplete(true, type);  // considered successful.
    return;
  }

  if (signature_.get()) {
    ExtensionIdSet not_allowed_yet =
        base::STLSetDifference<ExtensionIdSet>(ids, signature_->ids);
    if (not_allowed_yet.empty()) {
      OnVerificationComplete(true, type);  // considered successful.
      return;
    }
  }

  InstallVerifier::PendingOperation* operation =
      new InstallVerifier::PendingOperation(type);
  operation->ids.insert(ids.begin(), ids.end());

  operation_queue_.push(linked_ptr<PendingOperation>(operation));

  // If there are no ongoing pending requests, we need to kick one off.
  if (operation_queue_.size() == 1)
    BeginFetch();
}

void InstallVerifier::AddProvisional(const ExtensionIdSet& ids) {
  provisional_.insert(ids.begin(), ids.end());
  AddMany(ids, ADD_PROVISIONAL);
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
    if (ContainsKey(signature_->ids, *i) ||
        ContainsKey(signature_->invalid_ids, *i)) {
      found_any = true;
      break;
    }
  }
  if (!found_any)
    return;

  InstallVerifier::PendingOperation* operation =
      new InstallVerifier::PendingOperation(InstallVerifier::REMOVE);
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
  NOT_VERIFIED_BUT_UNKNOWN_ID,
  COMPONENT,

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
  if (extension->location() == Manifest::COMPONENT) {
    MustRemainDisabledHistogram(COMPONENT);
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
  } else if (signature_.get() == NULL &&
             (!bootstrap_check_complete_ || GetStatus() < ENFORCE_STRICT)) {
    // If we don't have a signature yet, we'll temporarily consider every
    // extension from the webstore verified to avoid false positives on existing
    // profiles hitting this code for the first time. The InstallVerifier
    // will bootstrap itself once the ExtensionsSystem is ready.
    outcome = NO_SIGNATURE;
  } else if (!IsVerified(extension->id())) {
    if (signature_.get() &&
        !ContainsKey(signature_->invalid_ids, extension->id())) {
      outcome = NOT_VERIFIED_BUT_UNKNOWN_ID;
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

InstallVerifier::PendingOperation::PendingOperation(OperationType type)
    : type(type) {}

InstallVerifier::PendingOperation::~PendingOperation() {
}

ExtensionIdSet InstallVerifier::GetExtensionsToVerify() const {
  ExtensionIdSet result;
  scoped_ptr<ExtensionSet> extensions =
      ExtensionRegistry::Get(context_)->GenerateInstalledExtensionsSet();
  for (ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end();
       ++iter) {
    if (NeedsVerification(**iter))
      result.insert((*iter)->id());
  }
  return result;
}

void InstallVerifier::MaybeBootstrapSelf() {
  bool needs_bootstrap = false;

  ExtensionIdSet extension_ids = GetExtensionsToVerify();
  if (signature_.get() == NULL && ShouldFetchSignature()) {
    needs_bootstrap = true;
  } else {
    for (ExtensionIdSet::const_iterator iter = extension_ids.begin();
         iter != extension_ids.end();
         ++iter) {
      if (!IsKnownId(*iter)) {
        needs_bootstrap = true;
        break;
      }
    }
  }

  if (needs_bootstrap)
    AddMany(extension_ids, ADD_ALL_BOOTSTRAP);
  else
    bootstrap_check_complete_ = true;
}

void InstallVerifier::OnVerificationComplete(bool success, OperationType type) {
  switch (type) {
    case ADD_SINGLE:
      LogAddVerifiedSuccess(success);
      break;
    case ADD_ALL:
    case ADD_ALL_BOOTSTRAP:
      LogVerifyAllSuccessHistogram(type == ADD_ALL_BOOTSTRAP, success);
      bootstrap_check_complete_ = true;
      if (success) {
        // Iterate through the extensions and, if any are newly-verified and
        // should have the DISABLE_NOT_VERIFIED reason lifted, do so.
        const ExtensionSet& disabled_extensions =
            ExtensionRegistry::Get(context_)->disabled_extensions();
        for (ExtensionSet::const_iterator iter = disabled_extensions.begin();
             iter != disabled_extensions.end();
             ++iter) {
          int disable_reasons = prefs_->GetDisableReasons((*iter)->id());
          if (disable_reasons & Extension::DISABLE_NOT_VERIFIED &&
              !MustRemainDisabled(*iter, NULL, NULL)) {
            prefs_->RemoveDisableReason((*iter)->id(),
                                        Extension::DISABLE_NOT_VERIFIED);
          }
        }
      }
      if (success || GetStatus() == ENFORCE_STRICT) {
        ExtensionSystem::Get(context_)
            ->extension_service()
            ->CheckManagementPolicy();
      }
      break;
    // We don't need to check disable reasons or report UMA stats for
    // provisional adds or removals.
    case ADD_PROVISIONAL:
    case REMOVE:
      break;
  }
}

void InstallVerifier::GarbageCollect() {
  if (!ShouldFetchSignature()) {
    return;
  }
  CHECK(signature_.get());
  ExtensionIdSet leftovers = signature_->ids;
  leftovers.insert(signature_->invalid_ids.begin(),
                   signature_->invalid_ids.end());
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

void InstallVerifier::BeginFetch() {
  DCHECK(ShouldFetchSignature());

  // TODO(asargent) - It would be possible to coalesce all operations in the
  // queue into one fetch - we'd probably just need to change the queue to
  // hold (set of ids, list of operation type) pairs.
  CHECK(!operation_queue_.empty());
  const PendingOperation& operation = *operation_queue_.front();

  ExtensionIdSet ids_to_sign;
  if (signature_.get()) {
    ids_to_sign.insert(signature_->ids.begin(), signature_->ids.end());
  }
  if (operation.type == InstallVerifier::REMOVE) {
    for (ExtensionIdSet::const_iterator i = operation.ids.begin();
         i != operation.ids.end(); ++i) {
      if (ContainsKey(ids_to_sign, *i))
        ids_to_sign.erase(*i);
    }
  } else {  // All other operation types are some form of "ADD".
    ids_to_sign.insert(operation.ids.begin(), operation.ids.end());
  }

  signer_.reset(new InstallSigner(context_->GetRequestContext(), ids_to_sign));
  signer_->GetSignature(base::Bind(&InstallVerifier::SignatureCallback,
                                   weak_factory_.GetWeakPtr()));
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
    OnVerificationComplete(false, operation->type);

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

    OnVerificationComplete(success, operation->type);
  }

  if (!operation_queue_.empty())
    BeginFetch();
}

}  // namespace extensions
