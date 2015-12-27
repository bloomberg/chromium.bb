// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_promise.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/decryptor.h"
#include "media/base/media_keys.h"

namespace media {

static interfaces::CdmPromiseResultPtr GetRejectResult(
    MediaKeys::Exception exception,
    uint32_t system_code,
    const std::string& error_message) {
  interfaces::CdmPromiseResultPtr cdm_promise_result(
      interfaces::CdmPromiseResult::New());
  cdm_promise_result->success = false;
  cdm_promise_result->exception =
      static_cast<interfaces::CdmException>(exception);
  cdm_promise_result->system_code = system_code;
  cdm_promise_result->error_message = error_message;
  return cdm_promise_result;
}

template <typename... T>
MojoCdmPromise<T...>::MojoCdmPromise(const CallbackType& callback)
    : callback_(callback) {
  DCHECK(!callback_.is_null());
}

template <typename... T>
MojoCdmPromise<T...>::~MojoCdmPromise() {
  if (!callback_.is_null())
    DVLOG(1) << "Promise not resolved before destruction.";
}

template <typename... T>
void MojoCdmPromise<T...>::resolve(const T&... result) {
  MarkPromiseSettled();
  interfaces::CdmPromiseResultPtr cdm_promise_result(
      interfaces::CdmPromiseResult::New());
  cdm_promise_result->success = true;
  callback_.Run(
      std::move(cdm_promise_result),
      mojo::TypeConverter<typename MojoTypeTrait<T>::MojoType, T>::Convert(
          result)...);
  callback_.reset();
}

template <typename... T>
void MojoCdmPromise<T...>::reject(MediaKeys::Exception exception,
                                  uint32_t system_code,
                                  const std::string& error_message) {
  MarkPromiseSettled();
  callback_.Run(GetRejectResult(exception, system_code, error_message),
                MojoTypeTrait<T>::DefaultValue()...);
  callback_.reset();
}

template class MojoCdmPromise<>;
template class MojoCdmPromise<std::string>;

}  // namespace media
