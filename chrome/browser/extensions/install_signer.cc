// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_signer.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "crypto/random.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace {

using extensions::ExtensionIdSet;

const char kExpireDateKey[] = "expire_date";
const char kIdsKey[] = "ids";
const char kSaltKey[] = "salt";
const char kSignatureKey[] = "signature";
const size_t kSaltBytes = 32;

// Returns a date 12 weeks from now in YYYY-MM-DD format.
std::string GetExpiryString() {
  base::Time later = base::Time::Now() + base::TimeDelta::FromDays(7*12);
  base::Time::Exploded exploded;
  later.LocalExplode(&exploded);
  return base::StringPrintf("%d-%02d-%02d",
                            exploded.year,
                            exploded.month,
                            exploded.day_of_month);
}

void FakeSignData(const ExtensionIdSet& ids,
                  const std::string& hash_base64,
                  const std::string& expire_date,
                  std::string* signature,
                  ExtensionIdSet* invalid_ids) {
  DCHECK(invalid_ids && invalid_ids->empty());
  DCHECK(IsStringASCII(hash_base64));

  ExtensionIdSet invalid =
      extensions::InstallSigner::GetForcedNotFromWebstore();

  std::string data_to_sign;
  for (ExtensionIdSet::const_iterator i = ids.begin(); i != ids.end(); ++i) {
    if (ContainsKey(invalid, (*i)))
      invalid_ids->insert(*i);
    else
      data_to_sign.append(*i);
  }
  data_to_sign.append(hash_base64);
  data_to_sign.append(expire_date);

  *signature = std::string("FakeSignature:") +
      crypto::SHA256HashString(data_to_sign);
}

void FakeSignDataWithCallback(
    const ExtensionIdSet& ids,
    const std::string& hash_base64,
    const base::Callback<void(const std::string&,
                              const std::string&,
                              const ExtensionIdSet&)>& callback) {
  std::string signature;
  std::string expire_date = GetExpiryString();
  ExtensionIdSet invalid_ids;
  FakeSignData(ids, hash_base64, expire_date, &signature, &invalid_ids);
  callback.Run(signature, expire_date, invalid_ids);
}

bool FakeCheckSignature(const extensions::ExtensionIdSet& ids,
                        const std::string& hash_base64,
                        const std::string& signature,
                        const std::string& expire_date) {
  std::string computed_signature;
  extensions::ExtensionIdSet invalid_ids;
  FakeSignData(ids, hash_base64, expire_date, &computed_signature,
               &invalid_ids);
  return invalid_ids.empty() && computed_signature == signature;
}

// Hashes |salt| with the machine id, base64-encodes it and returns it in
// |result|.
bool HashWithMachineId(const std::string& salt, std::string* result) {
  std::string machine_id;
#if defined(ENABLE_RLZ)
  if (!rlz_lib::GetMachineId(&machine_id))
    return false;
#endif

  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update(machine_id.data(), machine_id.size());
  hash->Update(salt.data(), salt.size());

  std::string result_bytes(crypto::kSHA256Length, 0);
  hash->Finish(string_as_array(&result_bytes), result_bytes.size());

  return base::Base64Encode(result_bytes, result);
}

}  // namespace

namespace extensions {

InstallSignature::InstallSignature() {
}
InstallSignature::~InstallSignature() {
}

void InstallSignature::ToValue(base::DictionaryValue* value) const {
  CHECK(value);
  DCHECK(!signature.empty());
  DCHECK(!salt.empty());

  base::ListValue* id_list = new base::ListValue();
  for (ExtensionIdSet::const_iterator i = ids.begin(); i != ids.end();
       ++i)
    id_list->AppendString(*i);

  value->Set(kIdsKey, id_list);
  value->SetString(kExpireDateKey, expire_date);
  std::string salt_base64;
  std::string signature_base64;
  base::Base64Encode(salt, &salt_base64);
  base::Base64Encode(signature, &signature_base64);
  value->SetString(kSaltKey, salt_base64);
  value->SetString(kSignatureKey, signature_base64);
}

// static
scoped_ptr<InstallSignature> InstallSignature::FromValue(
    const base::DictionaryValue& value) {

  scoped_ptr<InstallSignature> result(new InstallSignature);

  std::string salt_base64;
  std::string signature_base64;
  if (!value.GetString(kExpireDateKey, &result->expire_date) ||
      !value.GetString(kSaltKey, &salt_base64) ||
      !value.GetString(kSignatureKey, &signature_base64) ||
      !base::Base64Decode(salt_base64, &result->salt) ||
      !base::Base64Decode(signature_base64, &result->signature)) {
    result.reset();
    return result.Pass();
  }

  const base::ListValue* ids = NULL;
  if (!value.GetList(kIdsKey, &ids)) {
    result.reset();
    return result.Pass();
  }

  for (ListValue::const_iterator i = ids->begin(); i != ids->end(); ++i) {
    std::string id;
    if (!(*i)->GetAsString(&id)) {
      result.reset();
      return result.Pass();
    }
    result->ids.insert(id);
  }

  return result.Pass();
}


InstallSigner::InstallSigner(net::URLRequestContextGetter* context_getter,
                             const ExtensionIdSet& ids)
    : ids_(ids), context_getter_(context_getter) {
}

InstallSigner::~InstallSigner() {
}

// static
bool InstallSigner::VerifySignature(const InstallSignature& signature) {
  if (signature.ids.empty())
    return true;

  std::string hash_base64;
  if (!HashWithMachineId(signature.salt, &hash_base64))
    return false;

  return FakeCheckSignature(signature.ids, hash_base64, signature.signature,
                            signature.expire_date);
}


class InstallSigner::FetcherDelegate : public net::URLFetcherDelegate {
 public:
  explicit FetcherDelegate(const base::Closure& callback)
      : callback_(callback) {
  }

  virtual ~FetcherDelegate() {
  }

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    callback_.Run();
  }

 private:
  base::Closure callback_;
  DISALLOW_COPY_AND_ASSIGN(FetcherDelegate);
};

// static
ExtensionIdSet InstallSigner::GetForcedNotFromWebstore() {
  std::string value = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kExtensionsNotWebstore);
  if (value.empty())
    return ExtensionIdSet();

  std::vector<std::string> ids;
  base::SplitString(value, ',', &ids);
  return ExtensionIdSet(ids.begin(), ids.end());
}

void InstallSigner::GetSignature(const SignatureCallback& callback) {
  CHECK(!url_fetcher_.get());
  CHECK(callback_.is_null());
  CHECK(salt_.empty());
  callback_ = callback;

  // If the set of ids is empty, just return an empty signature and skip the
  // call to the server.
  if (ids_.empty()) {
    callback_.Run(scoped_ptr<InstallSignature>(new InstallSignature()));
    return;
  }

  salt_ = std::string(kSaltBytes, 0);
  DCHECK_EQ(kSaltBytes, salt_.size());
  crypto::RandBytes(string_as_array(&salt_), salt_.size());

  std::string hash_base64;
  if (!HashWithMachineId(salt_, &hash_base64)) {
    if (!callback_.is_null())
      callback_.Run(scoped_ptr<InstallSignature>());
    return;
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeSignDataWithCallback,
                 ids_,
                 hash_base64,
                 base::Bind(&InstallSigner::HandleSignatureResult,
                            base::Unretained(this))));
}

void InstallSigner::HandleSignatureResult(const std::string& signature,
                                          const std::string& expire_date,
                                          const ExtensionIdSet& invalid_ids) {
  ExtensionIdSet valid_ids =
      base::STLSetDifference<ExtensionIdSet>(ids_, invalid_ids);

  scoped_ptr<InstallSignature> result;
  if (!signature.empty()) {
    result.reset(new InstallSignature);
    result->ids = valid_ids;
    result->salt = salt_;
    result->signature = signature;
    result->expire_date = expire_date;
    DCHECK(VerifySignature(*result));
  }

  if (!callback_.is_null())
    callback_.Run(result.Pass());
}


}  // namespace extensions
