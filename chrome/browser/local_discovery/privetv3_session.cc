// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_session.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_url_fetcher.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "crypto/hmac.h"
#include "crypto/p224_spake.h"
#include "url/gurl.h"

namespace local_discovery {

namespace {

const char kPrivetV3AuthAnonymous[] = "Privet anonymous";
const char kPrivetV3CryptoP224Spake2[] = "p224_spake2";
const char kPrivetV3Auto[] = "auto";

const char kPrivetV3InfoKeyAuth[] = "authentication";
const char kPrivetV3InfoKeyVersion[] = "version";
const char kPrivetV3InfoVersion[] = "3.0";

const char kPrivetV3KeyAccessToken[] = "accessToken";
const char kPrivetV3KeyAuthCode[] = "authCode";
const char kPrivetV3KeyCertFingerprint[] = "certFingerprint";
const char kPrivetV3KeyCertSignature[] = "certSignature";
const char kPrivetV3KeyClientCommitment[] = "clientCommitment";
const char kPrivetV3KeyCrypto[] = "crypto";
const char kPrivetV3KeyDeviceCommitment[] = "deviceCommitment";
const char kPrivetV3KeyMode[] = "mode";
const char kPrivetV3KeyPairing[] = "pairing";
const char kPrivetV3KeyRequestedScope[] = "requestedScope";
const char kPrivetV3KeyScope[] = "scope";
const char kPrivetV3KeySessionId[] = "sessionId";
const char kPrivetV3KeyTokenType[] = "tokenType";

const char kPrivetV3PairingStartPath[] = "/privet/v3/pairing/start";
const char kPrivetV3PairingConfirmPath[] = "/privet/v3/pairing/confirm";
const char kPrivetV3PairingCancelPath[] = "/privet/v3/pairing/cancel";
const char kPrivetV3AuthPath[] = "/privet/v3/auth";

const char kUrlPlaceHolder[] = "http://host/";

const int kUrlFetcherTimeoutSec = 30;

GURL CreatePrivetURL(const std::string& path) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

template <typename T>
class EnumToStringMap {
 public:
  static std::string FindNameById(T id) {
    for (const Element& m : kMap) {
      if (m.id == id) {
        DCHECK(m.name);
        return m.name;
      }
    }
    NOTREACHED();
    return std::string();
  }

  static bool FindIdByName(const std::string& name, T* id) {
    for (const Element& m : kMap) {
      if (m.name && m.name == name) {
        *id = m.id;
        return true;
      }
    }
    return false;
  }

 private:
  struct Element {
    const T id;
    const char* const name;
  };
  static const Element kMap[];
};

using PairingType = PrivetV3Session::PairingType;

template <>
const EnumToStringMap<PrivetV3Session::PairingType>::Element
    EnumToStringMap<PrivetV3Session::PairingType>::kMap[] = {
        {PairingType::PAIRING_TYPE_PINCODE, "pinCode"},
        {PairingType::PAIRING_TYPE_EMBEDDEDCODE, "embeddedCode"},
        {PairingType::PAIRING_TYPE_ULTRASOUND32, "ultrasound32"},
        {PairingType::PAIRING_TYPE_AUDIBLE32, "audible32"},
};

template <typename T>
std::string EnumToString(T id) {
  return EnumToStringMap<T>::FindNameById(id);
}

template <typename T>
bool StringToEnum(const std::string& name, T* id) {
  return EnumToStringMap<T>::FindIdByName(name, id);
}

bool GetDecodedString(const base::DictionaryValue& response,
                      const std::string& key,
                      std::string* value) {
  std::string base64;
  return response.GetString(key, &base64) && base::Base64Decode(base64, value);
}

bool ContainsString(const base::DictionaryValue& dictionary,
                    const std::string& key,
                    const std::string& expected_value) {
  const base::ListValue* list_of_string = nullptr;
  if (!dictionary.GetList(key, &list_of_string))
    return false;

  for (const base::Value* value : *list_of_string) {
    std::string string_value;
    if (value->GetAsString(&string_value) && string_value == expected_value)
      return true;
  }
  return false;
}

}  // namespace

class PrivetV3Session::FetcherDelegate : public PrivetURLFetcher::Delegate {
 public:
  FetcherDelegate(const base::WeakPtr<PrivetV3Session>& session,
                  const std::string& auth_token,
                  const PrivetV3Session::MessageCallback& callback);
  ~FetcherDelegate() override;

  // PrivetURLFetcher::Delegate methods.
  std::string GetAuthToken() override;
  void OnNeedPrivetToken(
      PrivetURLFetcher* fetcher,
      const PrivetURLFetcher::TokenCallback& callback) override;
  void OnError(PrivetURLFetcher* fetcher,
               PrivetURLFetcher::ErrorType error) override;
  void OnParsedJson(PrivetURLFetcher* fetcher,
                    const base::DictionaryValue& value,
                    bool has_error) override;

  PrivetURLFetcher* CreateURLFetcher(const GURL& url,
                                     net::URLFetcher::RequestType request_type,
                                     bool orphaned);

 private:
  void ReplyAndDestroyItself(Result result, const base::DictionaryValue& value);
  void OnTimeout();

  scoped_ptr<PrivetURLFetcher> url_fetcher_;

  base::WeakPtr<PrivetV3Session> session_;
  std::string auth_token_;
  MessageCallback callback_;

  base::WeakPtrFactory<FetcherDelegate> weak_ptr_factory_;
};

PrivetV3Session::FetcherDelegate::FetcherDelegate(
    const base::WeakPtr<PrivetV3Session>& session,
    const std::string& auth_token,
    const PrivetV3Session::MessageCallback& callback)
    : session_(session),
      auth_token_(auth_token),
      callback_(callback),
      weak_ptr_factory_(this) {
}

PrivetV3Session::FetcherDelegate::~FetcherDelegate() {
}

std::string PrivetV3Session::FetcherDelegate::GetAuthToken() {
  return auth_token_;
}

void PrivetV3Session::FetcherDelegate::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  NOTREACHED();
  OnError(fetcher, PrivetURLFetcher::URL_FETCH_ERROR);
}

void PrivetV3Session::FetcherDelegate::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  LOG(ERROR) << "PrivetURLFetcher url: " << fetcher->url()
             << ", error: " << error
             << ", response code: " << fetcher->response_code();
  ReplyAndDestroyItself(Result::STATUS_CONNECTIONERROR,
                        base::DictionaryValue());
}

void PrivetV3Session::FetcherDelegate::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue& value,
    bool has_error) {
  LOG_IF(ERROR, has_error) << "Response: " << value;
  ReplyAndDestroyItself(
      has_error ? Result::STATUS_DEVICEERROR : Result::STATUS_SUCCESS, value);
}

PrivetURLFetcher* PrivetV3Session::FetcherDelegate::CreateURLFetcher(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    bool orphaned) {
  DCHECK(!url_fetcher_);
  url_fetcher_ =
      session_->client_->CreateURLFetcher(url, request_type, this).Pass();
  url_fetcher_->V3Mode();

  auto timeout_task =
      orphaned ? base::Bind(&FetcherDelegate::OnTimeout, base::Owned(this))
               : base::Bind(&FetcherDelegate::OnTimeout,
                            weak_ptr_factory_.GetWeakPtr());
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, timeout_task,
      base::TimeDelta::FromSeconds(kUrlFetcherTimeoutSec));
  return url_fetcher_.get();
}

void PrivetV3Session::FetcherDelegate::ReplyAndDestroyItself(
    Result result,
    const base::DictionaryValue& value) {
  if (session_) {
    if (!callback_.is_null()) {
      callback_.Run(result, value);
      callback_.Reset();
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&PrivetV3Session::DeleteFetcher, session_,
                              base::Unretained(this)));
    session_.reset();
  }
}

void PrivetV3Session::FetcherDelegate::OnTimeout() {
  LOG(ERROR) << "PrivetURLFetcher timeout, url: " << url_fetcher_->url();
  ReplyAndDestroyItself(Result::STATUS_CONNECTIONERROR,
                        base::DictionaryValue());
}

PrivetV3Session::PrivetV3Session(scoped_ptr<PrivetHTTPClient> client)
    : client_(client.Pass()), weak_ptr_factory_(this) {
}

PrivetV3Session::~PrivetV3Session() {
  Cancel();
}

void PrivetV3Session::Init(const InitCallback& callback) {
  DCHECK(fetchers_.empty());
  DCHECK(fingerprint_.empty());
  DCHECK(session_id_.empty());
  DCHECK(privet_auth_token_.empty());

  privet_auth_token_ = kPrivetV3AuthAnonymous;

  StartGetRequest(kPrivetInfoPath,
                  base::Bind(&PrivetV3Session::OnInfoDone,
                             weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnInfoDone(const InitCallback& callback,
                                 Result result,
                                 const base::DictionaryValue& response) {
  std::vector<PairingType> pairing_types;
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result, pairing_types);

  std::string version;
  if (!response.GetString(kPrivetV3InfoKeyVersion, &version) ||
      version != kPrivetV3InfoVersion) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, pairing_types);
  }

  const base::DictionaryValue* authentication = nullptr;
  const base::ListValue* pairing = nullptr;
  if (!response.GetDictionary(kPrivetV3InfoKeyAuth, &authentication) ||
      !authentication->GetList(kPrivetV3KeyPairing, &pairing)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, pairing_types);
  }

  // The only supported crypto.
  if (!ContainsString(*authentication, kPrivetV3KeyCrypto,
                      kPrivetV3CryptoP224Spake2) ||
      !ContainsString(*authentication, kPrivetV3KeyMode, kPrivetV3KeyPairing)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR, pairing_types);
  }

  for (const base::Value* value : *pairing) {
    std::string pairing_string;
    PairingType pairing_type;
    if (!value->GetAsString(&pairing_string) ||
        !StringToEnum(pairing_string, &pairing_type)) {
      continue;  // Skip unknown pairing.
    }
    pairing_types.push_back(pairing_type);
  }

  callback.Run(Result::STATUS_SUCCESS, pairing_types);
}

void PrivetV3Session::StartPairing(PairingType pairing_type,
                                   const ResultCallback& callback) {
  base::DictionaryValue input;
  input.SetString(kPrivetV3KeyPairing, EnumToString(pairing_type));
  input.SetString(kPrivetV3KeyCrypto, kPrivetV3CryptoP224Spake2);

  StartPostRequest(kPrivetV3PairingStartPath, input,
                   base::Bind(&PrivetV3Session::OnPairingStartDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnPairingStartDone(
    const ResultCallback& callback,
    Result result,
    const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result);

  if (!response.GetString(kPrivetV3KeySessionId, &session_id_) ||
      !GetDecodedString(response, kPrivetV3KeyDeviceCommitment, &commitment_)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  return callback.Run(Result::STATUS_SUCCESS);
}

void PrivetV3Session::ConfirmCode(const std::string& code,
                                  const ResultCallback& callback) {
  if (session_id_.empty()) {
    LOG(ERROR) << "Pairing is not started";
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  spake_.reset(new crypto::P224EncryptedKeyExchange(
      crypto::P224EncryptedKeyExchange::kPeerTypeClient, code));

  base::DictionaryValue input;
  input.SetString(kPrivetV3KeySessionId, session_id_);

  std::string client_commitment;
  base::Base64Encode(spake_->GetNextMessage(), &client_commitment);
  input.SetString(kPrivetV3KeyClientCommitment, client_commitment);

  // Call ProcessMessage after GetNextMessage().
  crypto::P224EncryptedKeyExchange::Result result =
      spake_->ProcessMessage(commitment_);
  DCHECK_EQ(result, crypto::P224EncryptedKeyExchange::kResultPending);

  StartPostRequest(kPrivetV3PairingConfirmPath, input,
                   base::Bind(&PrivetV3Session::OnPairingConfirmDone,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnPairingConfirmDone(
    const ResultCallback& callback,
    Result result,
    const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result);

  std::string fingerprint;
  std::string signature;
  if (!GetDecodedString(response, kPrivetV3KeyCertFingerprint, &fingerprint) ||
      !GetDecodedString(response, kPrivetV3KeyCertSignature, &signature)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  // Key will be verified below, using HMAC.
  const std::string& key = spake_->GetUnverifiedKey();
  if (!hmac.Init(reinterpret_cast<const unsigned char*>(key.c_str()),
                 key.size()) ||
      !hmac.Verify(fingerprint, signature)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  std::string auth_code(hmac.DigestLength(), ' ');
  if (!hmac.Sign(session_id_,
                 reinterpret_cast<unsigned char*>(string_as_array(&auth_code)),
                 auth_code.size())) {
    LOG(FATAL) << "Signing failed";
    return callback.Run(Result::STATUS_SESSIONERROR);
  }
  // From now this is expected certificate.
  fingerprint_ = fingerprint;

  std::string auth_code_base64;
  base::Base64Encode(auth_code, &auth_code_base64);

  base::DictionaryValue input;
  input.SetString(kPrivetV3KeyAuthCode, auth_code_base64);
  input.SetString(kPrivetV3KeyMode, kPrivetV3KeyPairing);
  input.SetString(kPrivetV3KeyRequestedScope, kPrivetV3Auto);

  // Now we can use SendMessage with certificate validation.
  SendMessage(kPrivetV3AuthPath, input,
              base::Bind(&PrivetV3Session::OnAuthenticateDone,
                         weak_ptr_factory_.GetWeakPtr(), callback));
}

void PrivetV3Session::OnAuthenticateDone(
    const ResultCallback& callback,
    Result result,
    const base::DictionaryValue& response) {
  if (result != Result::STATUS_SUCCESS)
    return callback.Run(result);

  std::string access_token;
  std::string token_type;
  std::string scope;
  if (!response.GetString(kPrivetV3KeyAccessToken, &access_token) ||
      !response.GetString(kPrivetV3KeyTokenType, &token_type) ||
      !response.GetString(kPrivetV3KeyScope, &scope)) {
    LOG(ERROR) << "Response: " << response;
    return callback.Run(Result::STATUS_SESSIONERROR);
  }

  privet_auth_token_ = token_type + " " + access_token;

  return callback.Run(Result::STATUS_SUCCESS);
}

void PrivetV3Session::SendMessage(const std::string& api,
                                  const base::DictionaryValue& input,
                                  const MessageCallback& callback) {
  // TODO(vitalybuka): Implement validating HTTPS certificate using
  // fingerprint_.
  if (fingerprint_.empty()) {
    LOG(ERROR) << "Session is not paired";
    return callback.Run(Result::STATUS_SESSIONERROR, base::DictionaryValue());
  }

  StartPostRequest(api, input, callback);
}

void PrivetV3Session::RunCallback(const base::Closure& callback) {
  callback.Run();
}

void PrivetV3Session::StartGetRequest(const std::string& api,
                                      const MessageCallback& callback) {
  CreateFetcher(api, net::URLFetcher::RequestType::GET, callback)->Start();
}

void PrivetV3Session::StartPostRequest(const std::string& api,
                                       const base::DictionaryValue& input,
                                       const MessageCallback& callback) {
  if (!on_post_data_.is_null())
    on_post_data_.Run(input);
  std::string json;
  base::JSONWriter::WriteWithOptions(
      &input, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  PrivetURLFetcher* fetcher =
      CreateFetcher(api, net::URLFetcher::RequestType::POST, callback);
  fetcher->SetUploadData(cloud_print::kContentTypeJSON, json);
  fetcher->Start();
}

PrivetURLFetcher* PrivetV3Session::CreateFetcher(
    const std::string& api,
    net::URLFetcher::RequestType request_type,
    const MessageCallback& callback) {
  // Don't abort cancel requests after session object is destroyed.
  const bool orphaned = (api == kPrivetV3PairingCancelPath);
  FetcherDelegate* fetcher = new FetcherDelegate(weak_ptr_factory_.GetWeakPtr(),
                                                 privet_auth_token_, callback);
  if (!orphaned)
    fetchers_.push_back(fetcher);
  return fetcher->CreateURLFetcher(CreatePrivetURL(api), request_type,
                                   orphaned);
}

void PrivetV3Session::DeleteFetcher(const FetcherDelegate* fetcher) {
  fetchers_.erase(std::find(fetchers_.begin(), fetchers_.end(), fetcher));
}

void PrivetV3Session::Cancel() {
  // Cancel started unconfirmed sessions.
  if (session_id_.empty() || !fingerprint_.empty())
    return;
  base::DictionaryValue input;
  input.SetString(kPrivetV3KeySessionId, session_id_);
  StartPostRequest(kPrivetV3PairingCancelPath, input, MessageCallback());
}

}  // namespace local_discovery
