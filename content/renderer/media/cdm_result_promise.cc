// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm_result_promise.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

// A superset of media::MediaKeys::Exception for UMA reporting. These values
// should never be changed as it will affect existing reporting, and must match
// the values for CdmPromiseResult in tools/metrics/histograms/histograms.xml.
enum ResultCodeForUMA {
  SUCCESS = 0,
  NOT_SUPPORTED_ERROR = 1,
  INVALID_STATE_ERROR = 2,
  INVALID_ACCESS_ERROR = 3,
  QUOTA_EXCEEDED_ERROR = 4,
  UNKNOWN_ERROR = 5,
  CLIENT_ERROR = 6,
  OUTPUT_ERROR = 7,
  NUM_RESULT_CODES
};

static blink::WebContentDecryptionModuleException ConvertException(
    media::MediaKeys::Exception exception_code) {
  switch (exception_code) {
    case media::MediaKeys::NOT_SUPPORTED_ERROR:
      return blink::WebContentDecryptionModuleExceptionNotSupportedError;
    case media::MediaKeys::INVALID_STATE_ERROR:
      return blink::WebContentDecryptionModuleExceptionInvalidStateError;
    case media::MediaKeys::INVALID_ACCESS_ERROR:
      return blink::WebContentDecryptionModuleExceptionInvalidAccessError;
    case media::MediaKeys::QUOTA_EXCEEDED_ERROR:
      return blink::WebContentDecryptionModuleExceptionQuotaExceededError;
    case media::MediaKeys::UNKNOWN_ERROR:
      return blink::WebContentDecryptionModuleExceptionUnknownError;
    case media::MediaKeys::CLIENT_ERROR:
      return blink::WebContentDecryptionModuleExceptionClientError;
    case media::MediaKeys::OUTPUT_ERROR:
      return blink::WebContentDecryptionModuleExceptionOutputError;
  }
  NOTREACHED();
  return blink::WebContentDecryptionModuleExceptionUnknownError;
}

static ResultCodeForUMA ConvertExceptionToUMAResult(
    media::MediaKeys::Exception exception_code) {
  switch (exception_code) {
    case media::MediaKeys::NOT_SUPPORTED_ERROR:
      return NOT_SUPPORTED_ERROR;
    case media::MediaKeys::INVALID_STATE_ERROR:
      return INVALID_STATE_ERROR;
    case media::MediaKeys::INVALID_ACCESS_ERROR:
      return INVALID_ACCESS_ERROR;
    case media::MediaKeys::QUOTA_EXCEEDED_ERROR:
      return QUOTA_EXCEEDED_ERROR;
    case media::MediaKeys::UNKNOWN_ERROR:
      return UNKNOWN_ERROR;
    case media::MediaKeys::CLIENT_ERROR:
      return CLIENT_ERROR;
    case media::MediaKeys::OUTPUT_ERROR:
      return OUTPUT_ERROR;
  }
  NOTREACHED();
  return UNKNOWN_ERROR;
}

static void ReportUMA(std::string uma_name, ResultCodeForUMA result) {
  if (uma_name.empty())
    return;

  base::LinearHistogram::FactoryGet(
      uma_name,
      1,
      NUM_RESULT_CODES,
      NUM_RESULT_CODES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)->Add(result);
}

}  // namespace

template <typename... T>
CdmResultPromise<T...>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& uma_name)
    : web_cdm_result_(result), uma_name_(uma_name) {
}

template <typename... T>
CdmResultPromise<T...>::~CdmResultPromise() {
}

template <>
void CdmResultPromise<>::resolve() {
  MarkPromiseSettled();
  ReportUMA(uma_name_, SUCCESS);
  web_cdm_result_.complete();
}

template <>
void CdmResultPromise<media::KeyIdsVector>::resolve(
    const media::KeyIdsVector& result) {
  // TODO(jrummell): Update blink::WebContentDecryptionModuleResult to
  // handle the set of keys.
  reject(media::MediaKeys::NOT_SUPPORTED_ERROR, 0, "Not implemented.");
}

template <typename... T>
void CdmResultPromise<T...>::reject(media::MediaKeys::Exception exception_code,
                                    uint32 system_code,
                                    const std::string& error_message) {
  MarkPromiseSettled();
  ReportUMA(uma_name_, ConvertExceptionToUMAResult(exception_code));
  web_cdm_result_.completeWithError(ConvertException(exception_code),
                                    system_code,
                                    blink::WebString::fromUTF8(error_message));
}

NewSessionCdmResultPromise::NewSessionCdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& uma_name,
    const SessionInitializedCB& new_session_created_cb)
    : web_cdm_result_(result),
      uma_name_(uma_name),
      new_session_created_cb_(new_session_created_cb) {
}

NewSessionCdmResultPromise::~NewSessionCdmResultPromise() {
}

void NewSessionCdmResultPromise::resolve(const std::string& web_session_id) {
  MarkPromiseSettled();
  ReportUMA(uma_name_, SUCCESS);
  blink::WebContentDecryptionModuleResult::SessionStatus status =
      new_session_created_cb_.Run(web_session_id);
  web_cdm_result_.completeWithSession(status);
}

void NewSessionCdmResultPromise::reject(
    media::MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  MarkPromiseSettled();
  ReportUMA(uma_name_, ConvertExceptionToUMAResult(exception_code));
  web_cdm_result_.completeWithError(ConvertException(exception_code),
                                    system_code,
                                    blink::WebString::fromUTF8(error_message));
}

// Explicit template instantiation for the templates needed.
template class CdmResultPromise<>;
template class CdmResultPromise<media::KeyIdsVector>;

}  // namespace content
