// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm_result_promise.h"

#include "base/bind.h"
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

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
    default:
      NOTREACHED();
      return blink::WebContentDecryptionModuleExceptionUnknownError;
  }
}

template <typename T>
CdmResultPromise<T>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result)
    : media::CdmPromiseTemplate<T>(
          base::Bind(&CdmResultPromise::OnResolve, base::Unretained(this)),
          base::Bind(&CdmResultPromise::OnReject, base::Unretained(this))),
      web_cdm_result_(result) {
}

template <typename T>
CdmResultPromise<T>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& uma_name)
    : media::CdmPromiseTemplate<T>(
          base::Bind(&CdmResultPromise::OnResolve, base::Unretained(this)),
          base::Bind(&CdmResultPromise::OnReject, base::Unretained(this)),
          uma_name),
      web_cdm_result_(result) {
}

template <typename T>
CdmResultPromise<T>::~CdmResultPromise() {
}

template <>
void CdmResultPromise<std::string>::OnResolve(const std::string& result) {
  // This must be overridden in a subclass.
  NOTREACHED();
}

template <>
void CdmResultPromise<media::KeyIdsVector>::OnResolve(
    const media::KeyIdsVector& result) {
  // TODO(jrummell): Update blink::WebContentDecryptionModuleResult to
  // handle the set of keys.
  OnReject(media::MediaKeys::NOT_SUPPORTED_ERROR, 0, "Not implemented.");
}

template <typename T>
void CdmResultPromise<T>::OnReject(media::MediaKeys::Exception exception_code,
                                   uint32 system_code,
                                   const std::string& error_message) {
  web_cdm_result_.completeWithError(ConvertException(exception_code),
                                    system_code,
                                    blink::WebString::fromUTF8(error_message));
}

CdmResultPromise<void>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result)
    : media::CdmPromiseTemplate<void>(
          base::Bind(&CdmResultPromise::OnResolve, base::Unretained(this)),
          base::Bind(&CdmResultPromise::OnReject, base::Unretained(this))),
      web_cdm_result_(result) {
}

CdmResultPromise<void>::CdmResultPromise(
    const blink::WebContentDecryptionModuleResult& result,
    const std::string& uma_name)
    : media::CdmPromiseTemplate<void>(
          base::Bind(&CdmResultPromise::OnResolve, base::Unretained(this)),
          base::Bind(&CdmResultPromise::OnReject, base::Unretained(this)),
          uma_name),
      web_cdm_result_(result) {
}

CdmResultPromise<void>::~CdmResultPromise() {
}

void CdmResultPromise<void>::OnResolve() {
  web_cdm_result_.complete();
}

void CdmResultPromise<void>::OnReject(
    media::MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  web_cdm_result_.completeWithError(ConvertException(exception_code),
                                    system_code,
                                    blink::WebString::fromUTF8(error_message));
}

// Explicit template instantiation for the templates needed.
template class CdmResultPromise<std::string>;
template class CdmResultPromise<media::KeyIdsVector>;

}  // namespace content
