// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_CDM_INITIALIZED_PROMISE_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_CDM_INITIALIZED_PROMISE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_keys.h"

namespace content {

// Promise to be resolved when the CDM is initialized. It owns the MediaKeys
// object until the initialization completes, which it then passes to
// |cdm_created_cb|.
class CdmInitializedPromise : public media::SimpleCdmPromise {
 public:
  CdmInitializedPromise(const media::CdmCreatedCB& cdm_created_cb,
                        scoped_ptr<media::MediaKeys> cdm);
  ~CdmInitializedPromise() override;

  // SimpleCdmPromise implementation.
  void resolve() override;
  void reject(media::MediaKeys::Exception exception_code,
              uint32 system_code,
              const std::string& error_message) override;

 private:
  media::CdmCreatedCB cdm_created_cb_;
  scoped_ptr<media::MediaKeys> cdm_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_CDM_INITIALIZED_PROMISE_H_
