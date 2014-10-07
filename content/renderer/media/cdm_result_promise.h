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

// Used to convert a WebContentDecryptionModuleResult into a CdmPromiseTemplate
// so that it can be passed through Chromium. When resolve(T) is called, the
// appropriate complete...() method on WebContentDecryptionModuleResult will be
// invoked. If reject() is called instead,
// WebContentDecryptionModuleResult::completeWithError() is called.
// If constructed with a |uma_name|, CdmResultPromise will report the promise
// result (success or rejection code) to UMA.
template <typename... T>
class CdmResultPromise : public media::CdmPromiseTemplate<T...> {
 public:
  CdmResultPromise(const blink::WebContentDecryptionModuleResult& result,
                   const std::string& uma_name);
  virtual ~CdmResultPromise();

  // CdmPromiseTemplate<T> implementation.
  virtual void resolve(const T&... result) override;
  virtual void reject(media::MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message) override;

 private:
  using media::CdmPromiseTemplate<T...>::MarkPromiseSettled;

  blink::WebContentDecryptionModuleResult web_cdm_result_;

  // UMA name to report result to.
  std::string uma_name_;

  DISALLOW_COPY_AND_ASSIGN(CdmResultPromise);
};

typedef base::Callback<blink::WebContentDecryptionModuleResult::SessionStatus(
    const std::string& web_session_id)> SessionInitializedCB;

// Special class for resolving a new session promise. Resolving a new session
// promise returns the session ID (as a string), but the blink promise needs
// to get passed a SessionStatus. This class converts the session id to a
// SessionStatus by calling |new_session_created_cb|.
class NewSessionCdmResultPromise
    : public media::CdmPromiseTemplate<std::string> {
 public:
  NewSessionCdmResultPromise(
      const blink::WebContentDecryptionModuleResult& result,
      const std::string& uma_name,
      const SessionInitializedCB& new_session_created_cb);
  virtual ~NewSessionCdmResultPromise();

  // CdmPromiseTemplate<T> implementation.
  virtual void resolve(const std::string& web_session_id) override;
  virtual void reject(media::MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message) override;

 private:
  blink::WebContentDecryptionModuleResult web_cdm_result_;

  // UMA name to report result to.
  std::string uma_name_;

  // Called on resolve() to convert the session ID into a SessionStatus to
  // be reported to blink.
  SessionInitializedCB new_session_created_cb_;

  DISALLOW_COPY_AND_ASSIGN(NewSessionCdmResultPromise);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_RESULT_PROMISE_H_
