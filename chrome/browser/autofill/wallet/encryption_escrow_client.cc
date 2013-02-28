// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/encryption_escrow_client.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/wallet/encryption_escrow_client_observer.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kEncryptOtpBodyFormat[] = "cvv=%s:%s";
const char kEscrowInstrumentInformationFormat[] = "gid=%s&cardNumber=%s&cvv=%s";
const char kEscrowCardVerficationNumberFormat[] = "gid=%s&cvv=%s";
const char kApplicationMimeType[] = "application/x-www-form-urlencoded";

// The maximum number of bits in the one time pad that the server is willing to
// accept.
const size_t kMaxBits = 56;

// The minimum number of bits in the one time pad that the server is willing to
// accept.
const size_t kMinBits = 40;

}  // anonymous namespace

namespace autofill {
namespace wallet {

EncryptionEscrowClient::EncryptionEscrowClient(
    net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter),
      request_type_(NO_PENDING_REQUEST) {
  DCHECK(context_getter);
}

EncryptionEscrowClient::~EncryptionEscrowClient() {}

void EncryptionEscrowClient::EncryptOneTimePad(
    const std::vector<uint8>& one_time_pad,
    base::WeakPtr<EncryptionEscrowClientObserver> observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  size_t num_bits = one_time_pad.size() * 8;
  DCHECK_LE(num_bits, kMaxBits);
  DCHECK_GE(num_bits, kMinBits);

  request_type_ = ENCRYPT_ONE_TIME_PAD;

  std::string post_body = StringPrintf(
      kEncryptOtpBodyFormat,
      base::HexEncode(&num_bits, 1).c_str(),
      base::HexEncode(&(one_time_pad[0]), one_time_pad.size()).c_str());

  MakeRequest(GetEncryptionUrl(), post_body, observer);
}

void EncryptionEscrowClient::EscrowInstrumentInformation(
    const Instrument& new_instrument,
    const std::string& obfuscated_gaia_id,
    base::WeakPtr<EncryptionEscrowClientObserver> observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ESCROW_INSTRUMENT_INFORMATION;

  const std::string& primary_account_number =
      net::EscapeUrlEncodedData(
          UTF16ToUTF8(new_instrument.primary_account_number()), true);
  const std::string& card_verification_number =
      net::EscapeUrlEncodedData(
          UTF16ToUTF8(new_instrument.card_verification_number()), true);

  std::string post_body = StringPrintf(
      kEscrowInstrumentInformationFormat,
      obfuscated_gaia_id.c_str(),
      primary_account_number.c_str(),
      card_verification_number.c_str());

  MakeRequest(GetEscrowUrl(), post_body, observer);
}

void EncryptionEscrowClient::EscrowCardVerificationNumber(
    const std::string& card_verification_number,
    const std::string& obfuscated_gaia_id,
    base::WeakPtr<EncryptionEscrowClientObserver> observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ESCROW_CARD_VERIFICATION_NUMBER;

  std::string post_body = StringPrintf(
      kEscrowCardVerficationNumberFormat,
      obfuscated_gaia_id.c_str(),
      card_verification_number.c_str());

  MakeRequest(GetEscrowUrl(), post_body, observer);
}

void EncryptionEscrowClient::MakeRequest(
    const GURL& url,
    const std::string& post_body,
    base::WeakPtr<EncryptionEscrowClientObserver> observer) {
  DCHECK(!request_.get());
  DCHECK(observer);

  observer_ = observer;

  request_.reset(net::URLFetcher::Create(
      1, url, net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_);
  DVLOG(1) << "url=" << url << ", post_body=" << post_body;
  request_->SetUploadData(kApplicationMimeType, post_body);
  request_->Start();
}

// TODO(ahutter): Add manual retry logic if it's necessary.
void EncryptionEscrowClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  scoped_ptr<net::URLFetcher> old_request = request_.Pass();
  DCHECK_EQ(source, old_request.get());

  DVLOG(1) << "Got response from " << source->GetOriginalURL();

  RequestType type = request_type_;
  request_type_ = NO_PENDING_REQUEST;

  std::string data;
  source->GetResponseAsString(&data);
  DVLOG(1) << "Response body: " << data;

  if (source->GetResponseCode() != net::HTTP_OK) {
    observer_->OnNetworkError(source->GetResponseCode());
    return;
  }

  if (data.empty()) {
    HandleMalformedResponse(old_request.get());
    return;
  }

  switch (type) {
    case ENCRYPT_ONE_TIME_PAD: {
      std::vector<std::string> splits;
      // The response from the server should be formatted as
      // "<session material>|<encrypted one time pad>".
      base::SplitString(data, '|', &splits);
      if (splits.size() == 2) {
        if (observer_)
          observer_->OnDidEncryptOneTimePad(splits[1], splits[0]);
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    }

    case ESCROW_INSTRUMENT_INFORMATION:
      if (observer_)
        observer_->OnDidEscrowInstrumentInformation(data);
      break;

    case ESCROW_CARD_VERIFICATION_NUMBER:
      if (observer_)
        observer_->OnDidEscrowCardVerificationNumber(data);
      break;

    case NO_PENDING_REQUEST:
      NOTREACHED();
  }
}

void EncryptionEscrowClient::HandleMalformedResponse(net::URLFetcher* request) {
  // Called to inform exponential backoff logic of the error.
  request->ReceivedContentWasMalformed();
  if (observer_)
    observer_->OnMalformedResponse();
}

}  // namespace wallet
}  // namespace autofill
