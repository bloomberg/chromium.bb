// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_promise.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"

namespace media {

CdmPromise::CdmPromise() : is_pending_(true) {
}

CdmPromise::CdmPromise(PromiseRejectedCB reject_cb)
    : reject_cb_(reject_cb), is_pending_(true) {
  DCHECK(!reject_cb_.is_null());
}

CdmPromise::CdmPromise(PromiseRejectedCB reject_cb, const std::string& uma_name)
    : reject_cb_(reject_cb), is_pending_(true), uma_name_(uma_name) {
  DCHECK(!reject_cb_.is_null());
}

CdmPromise::~CdmPromise() {
  DCHECK(!is_pending_);
}

static CdmPromise::ResultCodeForUMA ConvertExceptionToUMAResult(
    MediaKeys::Exception exception_code) {
  switch (exception_code) {
    case MediaKeys::NOT_SUPPORTED_ERROR:
      return CdmPromise::NOT_SUPPORTED_ERROR;
    case MediaKeys::INVALID_STATE_ERROR:
      return CdmPromise::INVALID_STATE_ERROR;
    case MediaKeys::INVALID_ACCESS_ERROR:
      return CdmPromise::INVALID_ACCESS_ERROR;
    case MediaKeys::QUOTA_EXCEEDED_ERROR:
      return CdmPromise::QUOTA_EXCEEDED_ERROR;
    case MediaKeys::UNKNOWN_ERROR:
      return CdmPromise::UNKNOWN_ERROR;
    case MediaKeys::CLIENT_ERROR:
      return CdmPromise::CLIENT_ERROR;
    case MediaKeys::OUTPUT_ERROR:
      return CdmPromise::OUTPUT_ERROR;
  }
  NOTREACHED();
  return CdmPromise::UNKNOWN_ERROR;
}

void CdmPromise::reject(MediaKeys::Exception exception_code,
                        uint32 system_code,
                        const std::string& error_message) {
  DCHECK(is_pending_);
  is_pending_ = false;
  if (!uma_name_.empty()) {
    ResultCodeForUMA result_code = ConvertExceptionToUMAResult(exception_code);
    base::LinearHistogram::FactoryGet(
        uma_name_, 1, NUM_RESULT_CODES, NUM_RESULT_CODES + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)->Add(result_code);
  }
  reject_cb_.Run(exception_code, system_code, error_message);
}

template <typename T>
CdmPromiseTemplate<T>::CdmPromiseTemplate(
    base::Callback<void(const T&)> resolve_cb,
    PromiseRejectedCB reject_cb)
    : CdmPromise(reject_cb), resolve_cb_(resolve_cb) {
  DCHECK(!resolve_cb_.is_null());
}

template <typename T>
CdmPromiseTemplate<T>::CdmPromiseTemplate(
    base::Callback<void(const T&)> resolve_cb,
    PromiseRejectedCB reject_cb,
    const std::string& uma_name)
    : CdmPromise(reject_cb, uma_name), resolve_cb_(resolve_cb) {
  DCHECK(!resolve_cb_.is_null());
}

template <typename T>
CdmPromiseTemplate<T>::~CdmPromiseTemplate() {
  DCHECK(!is_pending_);
}

template <typename T>
void CdmPromiseTemplate<T>::resolve(const T& result) {
  DCHECK(is_pending_);
  is_pending_ = false;
  if (!uma_name_.empty()) {
    base::LinearHistogram::FactoryGet(
        uma_name_, 1, NUM_RESULT_CODES, NUM_RESULT_CODES + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)->Add(SUCCESS);
  }
  resolve_cb_.Run(result);
}

CdmPromiseTemplate<void>::CdmPromiseTemplate(base::Callback<void()> resolve_cb,
                                             PromiseRejectedCB reject_cb)
    : CdmPromise(reject_cb), resolve_cb_(resolve_cb) {
  DCHECK(!resolve_cb_.is_null());
}

CdmPromiseTemplate<void>::CdmPromiseTemplate(base::Callback<void()> resolve_cb,
                                             PromiseRejectedCB reject_cb,
                                             const std::string& uma_name)
    : CdmPromise(reject_cb, uma_name), resolve_cb_(resolve_cb) {
  DCHECK(!resolve_cb_.is_null());
  DCHECK(!uma_name_.empty());
}

CdmPromiseTemplate<void>::CdmPromiseTemplate() {
}

CdmPromiseTemplate<void>::~CdmPromiseTemplate() {
  DCHECK(!is_pending_);
}

void CdmPromiseTemplate<void>::resolve() {
  DCHECK(is_pending_);
  is_pending_ = false;
  if (!uma_name_.empty()) {
    base::LinearHistogram::FactoryGet(
        uma_name_, 1, NUM_RESULT_CODES, NUM_RESULT_CODES + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag)->Add(SUCCESS);
  }
  resolve_cb_.Run();
}

// Explicit template instantiation for the Promises needed.
template class MEDIA_EXPORT CdmPromiseTemplate<std::string>;

}  // namespace media
