// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_signer.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/process/process_info.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "crypto/random.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/constants.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace {

using extensions::ExtensionIdSet;

const char kExpireDateKey[] = "expire_date";
const char kExpiryKey[] = "expiry";
const char kHashKey[] = "hash";
const char kIdsKey[] = "ids";
const char kInvalidIdsKey[] = "invalid_ids";
const char kProtocolVersionKey[] = "protocol_version";
const char kSaltKey[] = "salt";
const char kSignatureKey[] = "signature";
const char kSignatureFormatVersionKey[] = "signature_format_version";
const char kTimestampKey[] = "timestamp";

// This allows us to version the format of what we write into the prefs,
// allowing for forward migration, as well as detecting forwards/backwards
// incompatabilities, etc.
const int kSignatureFormatVersion = 2;

const size_t kSaltBytes = 32;

const char kBackendUrl[] =
    "https://www.googleapis.com/chromewebstore/v1.1/items/verify";

const char kPublicKeyPEM[] =                                            \
    "-----BEGIN PUBLIC KEY-----"                                        \
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAj/u/XDdjlDyw7gHEtaaa"  \
    "sZ9GdG8WOKAyJzXd8HFrDtz2Jcuy7er7MtWvHgNDA0bwpznbI5YdZeV4UfCEsA4S"  \
    "rA5b3MnWTHwA1bgbiDM+L9rrqvcadcKuOlTeN48Q0ijmhHlNFbTzvT9W0zw/GKv8"  \
    "LgXAHggxtmHQ/Z9PP2QNF5O8rUHHSL4AJ6hNcEKSBVSmbbjeVm4gSXDuED5r0nwx"  \
    "vRtupDxGYp8IZpP5KlExqNu1nbkPc+igCTIB6XsqijagzxewUHCdovmkb2JNtskx"  \
    "/PMIEv+TvWIx2BzqGp71gSh/dV7SJ3rClvWd2xj8dtxG8FfAWDTIIi0qZXWn2Qhi"  \
    "zQIDAQAB"                                                          \
    "-----END PUBLIC KEY-----";

GURL GetBackendUrl() {
  return GURL(kBackendUrl);
}

// Hashes |salt| with the machine id, base64-encodes it and returns it in
// |result|.
bool HashWithMachineId(const std::string& salt, std::string* result) {
  std::string machine_id;
#if defined(ENABLE_RLZ)
  if (!rlz_lib::GetMachineId(&machine_id))
    return false;
#else
  machine_id = "unknown";
#endif

  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update(machine_id.data(), machine_id.size());
  hash->Update(salt.data(), salt.size());

  std::string result_bytes(crypto::kSHA256Length, 0);
  hash->Finish(string_as_array(&result_bytes), result_bytes.size());

  base::Base64Encode(result_bytes, result);
  return true;
}

// Validates that |input| is a string of the form "YYYY-MM-DD".
bool ValidateExpireDateFormat(const std::string& input) {
  if (input.length() != 10)
    return false;
  for (int i = 0; i < 10; i++) {
    if (i == 4 ||  i == 7) {
      if (input[i] != '-')
        return false;
    } else if (!IsAsciiDigit(input[i])) {
      return false;
    }
  }
  return true;
}

// Sets the value of |key| in |dictionary| to be a list with the contents of
// |ids|.
void SetExtensionIdSet(base::DictionaryValue* dictionary,
                       const char* key,
                       const ExtensionIdSet& ids) {
  base::ListValue* id_list = new base::ListValue();
  for (ExtensionIdSet::const_iterator i = ids.begin(); i != ids.end(); ++i)
    id_list->AppendString(*i);
  dictionary->Set(key, id_list);
}

// Tries to fetch a list of strings from |dictionay| for |key|, and inserts
// them into |ids|. The return value indicates success/failure. Note: on
// failure, |ids| might contain partial results, for instance if some of the
// members of the list were not strings.
bool GetExtensionIdSet(const base::DictionaryValue& dictionary,
                       const char* key,
                       ExtensionIdSet* ids) {
  const base::ListValue* id_list = NULL;
  if (!dictionary.GetList(key, &id_list))
    return false;
  for (base::ListValue::const_iterator i = id_list->begin();
       i != id_list->end();
       ++i) {
    std::string id;
    if (!(*i)->GetAsString(&id)) {
      return false;
    }
    ids->insert(id);
  }
  return true;
}

}  // namespace

namespace extensions {

InstallSignature::InstallSignature() {
}
InstallSignature::~InstallSignature() {
}

void InstallSignature::ToValue(base::DictionaryValue* value) const {
  CHECK(value);

  value->SetInteger(kSignatureFormatVersionKey, kSignatureFormatVersion);
  SetExtensionIdSet(value, kIdsKey, ids);
  SetExtensionIdSet(value, kInvalidIdsKey, invalid_ids);
  value->SetString(kExpireDateKey, expire_date);
  std::string salt_base64;
  std::string signature_base64;
  base::Base64Encode(salt, &salt_base64);
  base::Base64Encode(signature, &signature_base64);
  value->SetString(kSaltKey, salt_base64);
  value->SetString(kSignatureKey, signature_base64);
  value->SetString(kTimestampKey,
                   base::Int64ToString(timestamp.ToInternalValue()));
}

// static
scoped_ptr<InstallSignature> InstallSignature::FromValue(
    const base::DictionaryValue& value) {

  scoped_ptr<InstallSignature> result(new InstallSignature);

  // For now we don't want to support any backwards compability, but in the
  // future if we do, we would want to put the migration code here.
  int format_version = 0;
  if (!value.GetInteger(kSignatureFormatVersionKey, &format_version) ||
      format_version != kSignatureFormatVersion) {
    result.reset();
    return result.Pass();
  }

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

  // Note: earlier versions of the code did not write out a timestamp value
  // so older entries will not necessarily have this.
  if (value.HasKey(kTimestampKey)) {
    std::string timestamp;
    int64 timestamp_value = 0;
    if (!value.GetString(kTimestampKey, &timestamp) ||
        !base::StringToInt64(timestamp, &timestamp_value)) {
      result.reset();
      return result.Pass();
    }
    result->timestamp = base::Time::FromInternalValue(timestamp_value);
  }

  if (!GetExtensionIdSet(value, kIdsKey, &result->ids) ||
      !GetExtensionIdSet(value, kInvalidIdsKey, &result->invalid_ids)) {
    result.reset();
    return result.Pass();
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

  std::string signed_data;
  for (ExtensionIdSet::const_iterator i = signature.ids.begin();
       i != signature.ids.end(); ++i)
    signed_data.append(*i);

  std::string hash_base64;
  if (!HashWithMachineId(signature.salt, &hash_base64))
    return false;
  signed_data.append(hash_base64);

  signed_data.append(signature.expire_date);

  std::string public_key;
  if (!Extension::ParsePEMKeyBytes(kPublicKeyPEM, &public_key))
    return false;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(extension_misc::kSignatureAlgorithm,
                           sizeof(extension_misc::kSignatureAlgorithm),
                           reinterpret_cast<const uint8*>(
                               signature.signature.data()),
                           signature.signature.size(),
                           reinterpret_cast<const uint8*>(public_key.data()),
                           public_key.size()))
    return false;

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(signed_data.data()),
                        signed_data.size());
  return verifier.VerifyFinal();
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

namespace {

static int g_request_count = 0;

base::LazyInstance<base::TimeTicks> g_last_request_time =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ThreadChecker> g_single_thread_checker =
    LAZY_INSTANCE_INITIALIZER;

void LogRequestStartHistograms() {
  // Make sure we only ever call this from one thread, so that we don't have to
  // worry about race conditions setting g_last_request_time.
  DCHECK(g_single_thread_checker.Get().CalledOnValidThread());

  // CurrentProcessInfo::CreationTime is only defined on some platforms.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  const base::Time process_creation_time =
      base::CurrentProcessInfo::CreationTime();
  UMA_HISTOGRAM_COUNTS("ExtensionInstallSigner.UptimeAtTimeOfRequest",
                       (base::Time::Now() - process_creation_time).InSeconds());
#endif  // defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)

  base::TimeDelta delta;
  base::TimeTicks now = base::TimeTicks::Now();
  if (!g_last_request_time.Get().is_null())
    delta = now - g_last_request_time.Get();
  g_last_request_time.Get() = now;
  UMA_HISTOGRAM_COUNTS("ExtensionInstallSigner.SecondsSinceLastRequest",
                       delta.InSeconds());

  g_request_count += 1;
  UMA_HISTOGRAM_COUNTS_100("ExtensionInstallSigner.RequestCount",
                           g_request_count);
}

}  // namespace

void InstallSigner::GetSignature(const SignatureCallback& callback) {
  CHECK(!url_fetcher_.get());
  CHECK(callback_.is_null());
  CHECK(salt_.empty());
  callback_ = callback;

  // If the set of ids is empty, just return an empty signature and skip the
  // call to the server.
  if (ids_.empty()) {
    if (!callback_.is_null())
      callback_.Run(scoped_ptr<InstallSignature>(new InstallSignature()));
    return;
  }

  salt_ = std::string(kSaltBytes, 0);
  DCHECK_EQ(kSaltBytes, salt_.size());
  crypto::RandBytes(string_as_array(&salt_), salt_.size());

  std::string hash_base64;
  if (!HashWithMachineId(salt_, &hash_base64)) {
    ReportErrorViaCallback();
    return;
  }

  if (!context_getter_) {
    ReportErrorViaCallback();
    return;
  }

  base::Closure closure = base::Bind(&InstallSigner::ParseFetchResponse,
                                     base::Unretained(this));

  delegate_.reset(new FetcherDelegate(closure));
  url_fetcher_.reset(net::URLFetcher::Create(
      GetBackendUrl(), net::URLFetcher::POST, delegate_.get()));
  url_fetcher_->SetRequestContext(context_getter_);

  // The request protocol is JSON of the form:
  // {
  //   "protocol_version": "1",
  //   "hash": "<base64-encoded hash value here>",
  //   "ids": [ "<id1>", "id2" ]
  // }
  base::DictionaryValue dictionary;
  dictionary.SetInteger(kProtocolVersionKey, 1);
  dictionary.SetString(kHashKey, hash_base64);
  scoped_ptr<base::ListValue> id_list(new base::ListValue);
  for (ExtensionIdSet::const_iterator i = ids_.begin(); i != ids_.end(); ++i) {
    id_list->AppendString(*i);
  }
  dictionary.Set(kIdsKey, id_list.release());
  std::string json;
  base::JSONWriter::Write(&dictionary, &json);
  if (json.empty()) {
    ReportErrorViaCallback();
    return;
  }
  url_fetcher_->SetUploadData("application/json", json);
  LogRequestStartHistograms();
  request_start_time_ = base::Time::Now();
  VLOG(1) << "Sending request: " << json;
  url_fetcher_->Start();
}

void InstallSigner::ReportErrorViaCallback() {
  InstallSignature* null_signature = NULL;
  if (!callback_.is_null())
    callback_.Run(scoped_ptr<InstallSignature>(null_signature));
}

void InstallSigner::ParseFetchResponse() {
  bool fetch_success = url_fetcher_->GetStatus().is_success();
  UMA_HISTOGRAM_BOOLEAN("ExtensionInstallSigner.FetchSuccess", fetch_success);

  std::string response;
  if (fetch_success) {
    if (!url_fetcher_->GetResponseAsString(&response))
      response.clear();
  }
  UMA_HISTOGRAM_BOOLEAN("ExtensionInstallSigner.GetResponseSuccess",
                        !response.empty());
  if (!fetch_success || response.empty()) {
    ReportErrorViaCallback();
    return;
  }
  VLOG(1) << "Got response: " << response;

  // The response is JSON of the form:
  // {
  //   "protocol_version": "1",
  //   "signature": "<base64-encoded signature>",
  //   "expiry": "<date in YYYY-MM-DD form>",
  //   "invalid_ids": [ "<id3>", "<id4>" ]
  // }
  // where |invalid_ids| is a list of ids from the original request that
  // could not be verified to be in the webstore.

  base::DictionaryValue* dictionary = NULL;
  scoped_ptr<base::Value> parsed(base::JSONReader::Read(response));
  bool json_success = parsed.get() && parsed->GetAsDictionary(&dictionary);
  UMA_HISTOGRAM_BOOLEAN("ExtensionInstallSigner.ParseJsonSuccess",
                        json_success);
  if (!json_success) {
    ReportErrorViaCallback();
    return;
  }

  int protocol_version = 0;
  std::string signature_base64;
  std::string signature;
  std::string expire_date;

  dictionary->GetInteger(kProtocolVersionKey, &protocol_version);
  dictionary->GetString(kSignatureKey, &signature_base64);
  dictionary->GetString(kExpiryKey, &expire_date);

  bool fields_success =
      protocol_version == 1 && !signature_base64.empty() &&
      ValidateExpireDateFormat(expire_date) &&
      base::Base64Decode(signature_base64, &signature);
  UMA_HISTOGRAM_BOOLEAN("ExtensionInstallSigner.ParseFieldsSuccess",
                        fields_success);
  if (!fields_success) {
    ReportErrorViaCallback();
    return;
  }

  ExtensionIdSet invalid_ids;
  const base::ListValue* invalid_ids_list = NULL;
  if (dictionary->GetList(kInvalidIdsKey, &invalid_ids_list)) {
    for (size_t i = 0; i < invalid_ids_list->GetSize(); i++) {
      std::string id;
      if (!invalid_ids_list->GetString(i, &id)) {
        ReportErrorViaCallback();
        return;
      }
      invalid_ids.insert(id);
    }
  }

  HandleSignatureResult(signature, expire_date, invalid_ids);
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
    result->invalid_ids = invalid_ids;
    result->salt = salt_;
    result->signature = signature;
    result->expire_date = expire_date;
    result->timestamp = request_start_time_;
    bool verified = VerifySignature(*result);
    UMA_HISTOGRAM_BOOLEAN("ExtensionInstallSigner.ResultWasValid", verified);
    UMA_HISTOGRAM_COUNTS_100("ExtensionInstallSigner.InvalidCount",
                             invalid_ids.size());
    if (!verified)
      result.reset();
  }

  if (!callback_.is_null())
    callback_.Run(result.Pass());
}


}  // namespace extensions
