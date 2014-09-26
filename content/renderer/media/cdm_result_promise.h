// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_RESULT_PROMISE_H_
#define CONTENT_RENDERER_MEDIA_CDM_RESULT_PROMISE_H_

#include <map>

#include "base/basictypes.h"
#include "media/base/cdm_promise.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"

namespace content {

// Used to convert a WebContentDecryptionModuleResult into a CdmPromise so that
// it can be passed through Chromium. When CdmPromise::resolve(T) is called,
// OnResolve(T) will be called and will call the appropriate complete...()
// method on WebContentDecryptionModuleResult. If CdmPromise::reject() is called
// instead, WebContentDecryptionModuleResult::completeWithError() is called.
// If constructed with a |uma_name| (which must be the name of a
// CdmPromiseResult UMA), CdmResultPromise will report the promise result
// (success or rejection code).
template <typename T>
class CdmResultPromise : public media::CdmPromiseTemplate<T> {
 public:
  explicit CdmResultPromise(
      const blink::WebContentDecryptionModuleResult& result);
  CdmResultPromise(const blink::WebContentDecryptionModuleResult& result,
                   const std::string& uma_name);
  virtual ~CdmResultPromise();

 protected:
  // OnResolve() is virtual as it may need special handling in derived classes.
  virtual void OnResolve(const T& result);
  void OnReject(media::MediaKeys::Exception exception_code,
                uint32 system_code,
                const std::string& error_message);

  blink::WebContentDecryptionModuleResult web_cdm_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmResultPromise);
};

// Specialization for no parameter to resolve().
template <>
class CdmResultPromise<void> : public media::CdmPromiseTemplate<void> {
 public:
  explicit CdmResultPromise(
      const blink::WebContentDecryptionModuleResult& result);
  CdmResultPromise(const blink::WebContentDecryptionModuleResult& result,
                   const std::string& uma_name);
  virtual ~CdmResultPromise();

 protected:
  virtual void OnResolve();
  void OnReject(media::MediaKeys::Exception exception_code,
                uint32 system_code,
                const std::string& error_message);

  blink::WebContentDecryptionModuleResult web_cdm_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmResultPromise);
};

typedef CdmResultPromise<void> SimpleCdmResultPromise;

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_RESULT_PROMISE_H_
