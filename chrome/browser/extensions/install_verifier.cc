// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_verifier.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/install_signer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "extensions/common/manifest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum VerifyStatus {
  NONE,       // Do not request install signatures, and do not enforce them.
  BOOTSTRAP,  // Request install signatures, but do not enforce them.
  ENFORCE     // Request install signatures, and enforce them.
};

VerifyStatus GetStatus() {
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

void InstallVerifier::Init() {
  const DictionaryValue* pref = prefs_->GetInstallSignature();
  if (pref) {
    scoped_ptr<InstallSignature> signature_from_prefs =
        InstallSignature::FromValue(*pref);
    if (!signature_from_prefs.get()) {
      UMA_HISTOGRAM_BOOLEAN("InstallVerifier.InitUnparseablePref", true);
    } else if (!InstallSigner::VerifySignature(*signature_from_prefs.get())) {
      UMA_HISTOGRAM_BOOLEAN("InstallVerifier.InitInvalidSignature", true);
      DVLOG(1) << "Init - ignoring invalid signature";
    } else {
      signature_ = signature_from_prefs.Pass();
      GarbageCollect();
    }
  }

  if (!signature_.get() && ShouldFetchSignature()) {
    // We didn't have any signature but are in fetch mode, so do a request for
    // a signature if needed.
    scoped_ptr<ExtensionPrefs::ExtensionsInfo> all_info =
        prefs_->GetInstalledExtensionsInfo();
    ExtensionIdSet to_add;
    if (all_info.get()) {
      for (ExtensionPrefs::ExtensionsInfo::const_iterator i = all_info->begin();
           i != all_info->end(); ++i) {
        const ExtensionInfo& info = **i;
        const base::DictionaryValue* manifest = info.extension_manifest.get();
        if (manifest && ManifestURL::UpdatesFromGallery(manifest))
          to_add.insert(info.extension_id);
      }
    }
    if (!to_add.empty())
      AddMany(to_add, AddResultCallback());
  }
}

void InstallVerifier::Add(const std::string& id,
                          const AddResultCallback& callback) {
  ExtensionIdSet ids;
  ids.insert(id);
  AddMany(ids, callback);
}

void InstallVerifier::AddMany(const ExtensionIdSet& ids,
                              const AddResultCallback& callback) {
  if (!ShouldFetchSignature())
    return;

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

static bool FromStore(const Extension* extension) {
  bool updates_from_store = ManifestURL::UpdatesFromGallery(extension);
  return extension->from_webstore() || updates_from_store;
}

bool InstallVerifier::MustRemainDisabled(const Extension* extension,
                                         Extension::DisableReason* reason,
                                         string16* error) const {
  if (!ShouldEnforce() || !extension->is_extension() ||
      Manifest::IsUnpackedLocation(extension->location()) ||
      AllowedByEnterprisePolicy(extension->id()))
    return false;

  bool verified =
      FromStore(extension) &&
      IsVerified(extension->id()) &&
      !ContainsKey(InstallSigner::GetForcedNotFromWebstore(), extension->id());

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
  if (pref_service->IsManagedPreference(prefs::kExtensionInstallAllowList)) {
    const base::ListValue* whitelist =
        pref_service->GetList(prefs::kExtensionInstallAllowList);
    base::StringValue id_value(id);
    if (whitelist && whitelist->Find(id_value) != whitelist->end())
      return true;
  }
  if (pref_service->IsManagedPreference(prefs::kExtensionInstallForceList)) {
    const base::DictionaryValue* forcelist =
        pref_service->GetDictionary(prefs::kExtensionInstallForceList);
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
    DictionaryValue pref;
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

void InstallVerifier::SignatureCallback(
    scoped_ptr<InstallSignature> signature) {

  linked_ptr<PendingOperation> operation = operation_queue_.front();
  operation_queue_.pop();

  bool success = false;
  if (!signature.get()) {
    UMA_HISTOGRAM_BOOLEAN("InstallVerifier.CallbackNoSignature", true);
  } else if (!InstallSigner::VerifySignature(*signature)) {
    UMA_HISTOGRAM_BOOLEAN("InstallVerifier.CallbackInvalidSignature", true);
  } else {
    UMA_HISTOGRAM_BOOLEAN("InstallVerifier.CallbackValidSignature", true);
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

    // See if we were able to sign all of |ids|.
    ExtensionIdSet not_allowed =
        base::STLSetDifference<ExtensionIdSet>(operation->ids,
                                               signature_->ids);

    UMA_HISTOGRAM_COUNTS_100("InstallVerifier.CallbackNotAllowed",
                             not_allowed.size());

    if (!operation->callback.is_null())
      operation->callback.Run(not_allowed.empty());
  }

  if (!operation_queue_.empty())
    BeginFetch();
}


}  // namespace extensions
