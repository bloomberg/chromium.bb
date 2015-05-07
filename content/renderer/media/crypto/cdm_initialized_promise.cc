// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/cdm_initialized_promise.h"

namespace content {

CdmInitializedPromise::CdmInitializedPromise(
    const media::CdmCreatedCB& cdm_created_cb,
    scoped_ptr<media::MediaKeys> cdm)
    : cdm_created_cb_(cdm_created_cb), cdm_(cdm.Pass()) {
}

CdmInitializedPromise::~CdmInitializedPromise() {
}

void CdmInitializedPromise::resolve() {
  MarkPromiseSettled();
  cdm_created_cb_.Run(cdm_.Pass(), "");
}

void CdmInitializedPromise::reject(media::MediaKeys::Exception exception_code,
                                   uint32 system_code,
                                   const std::string& error_message) {
  MarkPromiseSettled();
  cdm_created_cb_.Run(nullptr, error_message);
}

}  // namespace content
