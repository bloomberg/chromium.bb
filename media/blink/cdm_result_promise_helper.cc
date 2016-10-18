// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/cdm_result_promise_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace media {

CdmResultForUMA ConvertCdmExceptionToResultForUMA(
    MediaKeys::Exception exception_code) {
  switch (exception_code) {
    case MediaKeys::NOT_SUPPORTED_ERROR:
      return NOT_SUPPORTED_ERROR;
    case MediaKeys::INVALID_STATE_ERROR:
      return INVALID_STATE_ERROR;
    case MediaKeys::INVALID_ACCESS_ERROR:
      return INVALID_ACCESS_ERROR;
    case MediaKeys::QUOTA_EXCEEDED_ERROR:
      return QUOTA_EXCEEDED_ERROR;
    case MediaKeys::UNKNOWN_ERROR:
      return UNKNOWN_ERROR;
    case MediaKeys::CLIENT_ERROR:
      return CLIENT_ERROR;
    case MediaKeys::OUTPUT_ERROR:
      return OUTPUT_ERROR;
  }
  NOTREACHED();
  return UNKNOWN_ERROR;
}

blink::WebContentDecryptionModuleException ConvertCdmException(
    MediaKeys::Exception exception_code) {
  switch (exception_code) {
    case MediaKeys::NOT_SUPPORTED_ERROR:
      return blink::WebContentDecryptionModuleExceptionNotSupportedError;
    case MediaKeys::INVALID_STATE_ERROR:
      return blink::WebContentDecryptionModuleExceptionInvalidStateError;

    // TODO(jrummell): Since InvalidAccess is not returned, thus should be
    // renamed to TYPE_ERROR. http://crbug.com/570216#c11.
    case MediaKeys::INVALID_ACCESS_ERROR:
      return blink::WebContentDecryptionModuleExceptionTypeError;
    case MediaKeys::QUOTA_EXCEEDED_ERROR:
      return blink::WebContentDecryptionModuleExceptionQuotaExceededError;
    case MediaKeys::UNKNOWN_ERROR:
      return blink::WebContentDecryptionModuleExceptionUnknownError;

    // These are deprecated, and should be removed.
    // http://crbug.com/570216#c11.
    case MediaKeys::CLIENT_ERROR:
    case MediaKeys::OUTPUT_ERROR:
      break;
  }
  NOTREACHED();
  return blink::WebContentDecryptionModuleExceptionUnknownError;
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
