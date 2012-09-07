// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/transport_security_persister.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/base/transport_security_state.h"
#include "net/base/x509_certificate.h"

using content::BrowserThread;
using net::HashValue;
using net::HashValueTag;
using net::HashValueVector;
using net::TransportSecurityState;

namespace {

ListValue* SPKIHashesToListValue(const HashValueVector& hashes) {
  ListValue* pins = new ListValue;

  for (HashValueVector::const_iterator i = hashes.begin();
       i != hashes.end(); ++i) {
    std::string hash_str(reinterpret_cast<const char*>(i->data()), i->size());
    std::string b64;
    if (base::Base64Encode(hash_str, &b64))
      pins->Append(new StringValue(TransportSecurityState::HashValueLabel(*i) +
                                   b64));
  }

  return pins;
}

void SPKIHashesFromListValue(const ListValue& pins, HashValueVector* hashes) {
  size_t num_pins = pins.GetSize();
  for (size_t i = 0; i < num_pins; ++i) {
    std::string type_and_base64;
    HashValue fingerprint;
    if (pins.GetString(i, &type_and_base64) &&
        TransportSecurityState::ParsePin(type_and_base64, &fingerprint)) {
      hashes->push_back(fingerprint);
    }
  }
}

// This function converts the binary hashes to a base64 string which we can
// include in a JSON file.
std::string HashedDomainToExternalString(const std::string& hashed) {
  std::string out;
  base::Base64Encode(hashed, &out);
  return out;
}

// This inverts |HashedDomainToExternalString|, above. It turns an external
// string (from a JSON file) into an internal (binary) string.
std::string ExternalStringToHashedDomain(const std::string& external) {
  std::string out;
  if (!base::Base64Decode(external, &out) ||
      out.size() != crypto::kSHA256Length) {
    return std::string();
  }

  return out;
}

const char kIncludeSubdomains[] = "include_subdomains";
const char kMode[] = "mode";
const char kExpiry[] = "expiry";
const char kDynamicSPKIHashesExpiry[] = "dynamic_spki_hashes_expiry";
const char kStaticSPKIHashes[] = "static_spki_hashes";
const char kPreloadedSPKIHashes[] = "preloaded_spki_hashes";
const char kDynamicSPKIHashes[] = "dynamic_spki_hashes";
const char kForceHTTPS[] = "force-https";
const char kStrict[] = "strict";
const char kDefault[] = "default";
const char kPinningOnly[] = "pinning-only";
const char kCreated[] = "created";

}  // namespace

class TransportSecurityPersister::Loader {
 public:
  Loader(const base::WeakPtr<TransportSecurityPersister>& persister,
         const FilePath& path)
      : persister_(persister),
        path_(path),
        state_valid_(false) {
  }

  void Load() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    state_valid_ = file_util::ReadFileToString(path_, &state_);
  }

  void CompleteLoad() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // Make sure we're deleted.
    scoped_ptr<Loader> deleter(this);

    if (!persister_ || !state_valid_)
      return;
    persister_->CompleteLoad(state_);
  }

 private:
  base::WeakPtr<TransportSecurityPersister> persister_;

  FilePath path_;

  std::string state_;
  bool state_valid_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

TransportSecurityPersister::TransportSecurityPersister(
    TransportSecurityState* state,
    const FilePath& profile_path,
    bool readonly)
    : transport_security_state_(state),
      writer_(profile_path.AppendASCII("TransportSecurity"),
              BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)),
      readonly_(readonly),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  transport_security_state_->SetDelegate(this);

  Loader* loader = new Loader(weak_ptr_factory_.GetWeakPtr(), writer_.path());
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&Loader::Load, base::Unretained(loader)),
      base::Bind(&Loader::CompleteLoad, base::Unretained(loader)));
}

TransportSecurityPersister::~TransportSecurityPersister() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();

  transport_security_state_->SetDelegate(NULL);
}

void TransportSecurityPersister::StateIsDirty(
    TransportSecurityState* state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(transport_security_state_, state);

  if (!readonly_)
    writer_.ScheduleWrite(this);
}

bool TransportSecurityPersister::SerializeData(std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DictionaryValue toplevel;
  base::Time now = base::Time::Now();
  TransportSecurityState::Iterator state(*transport_security_state_);
  for (; state.HasNext(); state.Advance()) {
    const std::string& hostname = state.hostname();
    const TransportSecurityState::DomainState& domain_state =
        state.domain_state();

    DictionaryValue* serialized = new DictionaryValue;
    serialized->SetBoolean(kIncludeSubdomains,
                           domain_state.include_subdomains);
    serialized->SetDouble(kCreated, domain_state.created.ToDoubleT());
    serialized->SetDouble(kExpiry, domain_state.upgrade_expiry.ToDoubleT());
    serialized->SetDouble(kDynamicSPKIHashesExpiry,
                          domain_state.dynamic_spki_hashes_expiry.ToDoubleT());

    switch (domain_state.upgrade_mode) {
      case TransportSecurityState::DomainState::MODE_FORCE_HTTPS:
        serialized->SetString(kMode, kForceHTTPS);
        break;
      case TransportSecurityState::DomainState::MODE_DEFAULT:
        serialized->SetString(kMode, kDefault);
        break;
      default:
        NOTREACHED() << "DomainState with unknown mode";
        delete serialized;
        continue;
    }

    serialized->Set(kStaticSPKIHashes,
                    SPKIHashesToListValue(domain_state.static_spki_hashes));

    if (now < domain_state.dynamic_spki_hashes_expiry) {
      serialized->Set(kDynamicSPKIHashes,
                      SPKIHashesToListValue(domain_state.dynamic_spki_hashes));
    }

    toplevel.Set(HashedDomainToExternalString(hostname), serialized);
  }

  base::JSONWriter::WriteWithOptions(&toplevel,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     output);
  return true;
}

bool TransportSecurityPersister::DeserializeFromCommandLine(
    const std::string& serialized) {
  // Purposefully ignore |dirty| because we do not want to persist entries
  // deserialized in this way.
  bool dirty;
  return Deserialize(serialized, true, &dirty, transport_security_state_);
}

bool TransportSecurityPersister::LoadEntries(const std::string& serialized,
                                             bool* dirty) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  transport_security_state_->Clear();
  return Deserialize(serialized, false, dirty, transport_security_state_);
}

// static
bool TransportSecurityPersister::Deserialize(const std::string& serialized,
                                             bool forced,
                                             bool* dirty,
                                             TransportSecurityState* state) {
  scoped_ptr<Value> value(base::JSONReader::Read(serialized));
  DictionaryValue* dict_value;
  if (!value.get() || !value->GetAsDictionary(&dict_value))
    return false;

  const base::Time current_time(base::Time::Now());
  bool dirtied = false;

  for (DictionaryValue::key_iterator i = dict_value->begin_keys();
       i != dict_value->end_keys(); ++i) {
    DictionaryValue* parsed;
    if (!dict_value->GetDictionaryWithoutPathExpansion(*i, &parsed)) {
      LOG(WARNING) << "Could not parse entry " << *i << "; skipping entry";
      continue;
    }

    std::string mode_string;
    double created;
    double expiry;
    double dynamic_spki_hashes_expiry = 0.0;
    TransportSecurityState::DomainState domain_state;

    if (!parsed->GetBoolean(kIncludeSubdomains,
                            &domain_state.include_subdomains) ||
        !parsed->GetString(kMode, &mode_string) ||
        !parsed->GetDouble(kExpiry, &expiry)) {
      LOG(WARNING) << "Could not parse some elements of entry " << *i
                   << "; skipping entry";
      continue;
    }

    // Don't fail if this key is not present.
    parsed->GetDouble(kDynamicSPKIHashesExpiry,
                      &dynamic_spki_hashes_expiry);

    ListValue* pins_list = NULL;
    // preloaded_spki_hashes is a legacy synonym for static_spki_hashes.
    if (parsed->GetList(kStaticSPKIHashes, &pins_list))
      SPKIHashesFromListValue(*pins_list, &domain_state.static_spki_hashes);
    else if (parsed->GetList(kPreloadedSPKIHashes, &pins_list))
      SPKIHashesFromListValue(*pins_list, &domain_state.static_spki_hashes);

    if (parsed->GetList(kDynamicSPKIHashes, &pins_list))
      SPKIHashesFromListValue(*pins_list, &domain_state.dynamic_spki_hashes);

    if (mode_string == kForceHTTPS || mode_string == kStrict) {
      domain_state.upgrade_mode =
          TransportSecurityState::DomainState::MODE_FORCE_HTTPS;
    } else if (mode_string == kDefault || mode_string == kPinningOnly) {
      domain_state.upgrade_mode =
          TransportSecurityState::DomainState::MODE_DEFAULT;
    } else {
      LOG(WARNING) << "Unknown TransportSecurityState mode string "
                   << mode_string << " found for entry " << *i
                   << "; skipping entry";
      continue;
    }

    domain_state.upgrade_expiry = base::Time::FromDoubleT(expiry);
    domain_state.dynamic_spki_hashes_expiry =
        base::Time::FromDoubleT(dynamic_spki_hashes_expiry);
    if (parsed->GetDouble(kCreated, &created)) {
      domain_state.created = base::Time::FromDoubleT(created);
    } else {
      // We're migrating an old entry with no creation date. Make sure we
      // write the new date back in a reasonable time frame.
      dirtied = true;
      domain_state.created = base::Time::Now();
    }

    if (domain_state.upgrade_expiry <= current_time &&
        domain_state.dynamic_spki_hashes_expiry <= current_time) {
      // Make sure we dirty the state if we drop an entry.
      dirtied = true;
      continue;
    }

    std::string hashed = ExternalStringToHashedDomain(*i);
    if (hashed.empty()) {
      dirtied = true;
      continue;
    }

    if (forced)
      state->AddOrUpdateForcedHosts(hashed, domain_state);
    else
      state->AddOrUpdateEnabledHosts(hashed, domain_state);
  }

  *dirty = dirtied;
  return true;
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool dirty = false;
  if (!LoadEntries(state, &dirty)) {
    LOG(ERROR) << "Failed to deserialize state: " << state;
    return;
  }
  if (dirty)
    StateIsDirty(transport_security_state_);
}
