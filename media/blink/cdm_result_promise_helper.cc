// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/cdm_result_promise_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace media {

CdmResultForUMA ConvertCdmExceptionToResultForUMA(
    CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case CdmPromise::NOT_SUPPORTED_ERROR:
      return NOT_SUPPORTED_ERROR;
    case CdmPromise::INVALID_STATE_ERROR:
      return INVALID_STATE_ERROR;
    case CdmPromise::INVALID_ACCESS_ERROR:
      return INVALID_ACCESS_ERROR;
    case CdmPromise::QUOTA_EXCEEDED_ERROR:
      return QUOTA_EXCEEDED_ERROR;
    case CdmPromise::UNKNOWN_ERROR:
      return UNKNOWN_ERROR;
    case CdmPromise::CLIENT_ERROR:
      return CLIENT_ERROR;
    case CdmPromise::OUTPUT_ERROR:
      return OUTPUT_ERROR;
  }
  NOTREACHED();
  return UNKNOWN_ERROR;
}

blink::WebContentDecryptionModuleException ConvertCdmException(
    CdmPromise::Exception exception_code) {
  switch (exception_code) {
    case CdmPromise::NOT_SUPPORTED_ERROR:
      return blink::kWebContentDecryptionModuleExceptionNotSupportedError;
    case CdmPromise::INVALID_STATE_ERROR:
      return blink::kWebContentDecryptionModuleExceptionInvalidStateError;

    // TODO(jrummell): Since InvalidAccess is not returned, thus should be
    // renamed to TYPE_ERROR. http://crbug.com/570216#c11.
    case CdmPromise::INVALID_ACCESS_ERROR:
      return blink::kWebContentDecryptionModuleExceptionTypeError;
    case CdmPromise::QUOTA_EXCEEDED_ERROR:
      return blink::kWebContentDecryptionModuleExceptionQuotaExceededError;
    case CdmPromise::UNKNOWN_ERROR:
      return blink::kWebContentDecryptionModuleExceptionUnknownError;

    // These are deprecated, and should be removed.
    // http://crbug.com/570216#c11.
    case CdmPromise::CLIENT_ERROR:
    case CdmPromise::OUTPUT_ERROR:
      break;
  }
  NOTREACHED();
  return blink::kWebContentDecryptionModuleExceptionUnknownError;
}

blink::WebEncryptedMediaKeyInformation::KeyStatus ConvertCdmKeyStatus(
    media::CdmKeyInformation::KeyStatus key_status) {
  switch (key_status) {
    case media::CdmKeyInformation::USABLE:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kUsable;
    case media::CdmKeyInformation::INTERNAL_ERROR:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kInternalError;
    case media::CdmKeyInformation::EXPIRED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kExpired;
    case media::CdmKeyInformation::OUTPUT_RESTRICTED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::
          kOutputRestricted;
    case media::CdmKeyInformation::OUTPUT_DOWNSCALED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::
          kOutputDownscaled;
    case media::CdmKeyInformation::KEY_STATUS_PENDING:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kStatusPending;
    case media::CdmKeyInformation::RELEASED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::kReleased;
  }
  NOTREACHED();
  return blink::WebEncryptedMediaKeyInformation::KeyStatus::kInternalError;
}

void ReportCdmResultUMA(const std::string& uma_name, CdmResultForUMA result) {
  if (uma_name.empty())
    return;

  base::LinearHistogram::FactoryGet(
      uma_name,
      1,
      NUM_RESULT_CODES,
      NUM_RESULT_CODES + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)->Add(result);
}

}  // namespace media
