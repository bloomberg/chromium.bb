// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_PROMISE_H_
#define MEDIA_BASE_CDM_PROMISE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"

namespace media {

// Interface for promises being resolved/rejected in response to various
// session actions. These may be called synchronously or asynchronously.
// The promise must be resolved or rejected exactly once. It is expected that
// the caller free the promise once it is resolved/rejected.
//
// This is only the base class, as parameter to resolve() varies.
class MEDIA_EXPORT CdmPromise {
 public:
  // A superset of media::MediaKeys::Exception for UMA reporting.
  enum ResultCodeForUMA {
    SUCCESS,
    NOT_SUPPORTED_ERROR,
    INVALID_STATE_ERROR,
    INVALID_ACCESS_ERROR,
    QUOTA_EXCEEDED_ERROR,
    UNKNOWN_ERROR,
    CLIENT_ERROR,
    OUTPUT_ERROR,
    NUM_RESULT_CODES
  };

  enum ResolveParameterType {
    VOID_TYPE,
    STRING_TYPE,
    KEY_IDS_VECTOR_TYPE
  };

  typedef base::Callback<void(MediaKeys::Exception exception_code,
                              uint32 system_code,
                              const std::string& error_message)>
      PromiseRejectedCB;

  virtual ~CdmPromise();

  // Used to indicate that the operation failed. |exception_code| must be
  // specified. |system_code| is a Key System-specific value for the error
  // that occurred, or 0 if there is no associated status code or such status
  // codes are not supported by the Key System. |error_message| is optional.
  virtual void reject(MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message);

  virtual ResolveParameterType GetResolveParameterType() const = 0;

 protected:
  CdmPromise();
  CdmPromise(PromiseRejectedCB reject_cb);

  // If constructed with a |uma_name| (which must be the name of a
  // CdmPromiseResult UMA), CdmPromise will report the promise result (success
  // or rejection code).
  CdmPromise(PromiseRejectedCB reject_cb, const std::string& uma_name);

  PromiseRejectedCB reject_cb_;

  // Keep track of whether the promise hasn't been resolved or rejected yet.
  bool is_pending_;

  // UMA to report result to.
  std::string uma_name_;

  DISALLOW_COPY_AND_ASSIGN(CdmPromise);
};

// Specialization for no parameter to resolve().
template <>
class MEDIA_EXPORT CdmPromiseTemplate<void> : public CdmPromise {
 public:
  CdmPromiseTemplate(base::Callback<void(void)> resolve_cb,
                     PromiseRejectedCB rejected_cb);
  CdmPromiseTemplate(base::Callback<void(void)> resolve_cb,
                     PromiseRejectedCB rejected_cb,
                     const std::string& uma_name);
  virtual ~CdmPromiseTemplate();
  virtual void resolve();
  virtual ResolveParameterType GetResolveParameterType() const OVERRIDE;

 protected:
  // Allow subclasses to completely override the implementation.
  CdmPromiseTemplate();

 private:
  base::Callback<void(void)> resolve_cb_;

  DISALLOW_COPY_AND_ASSIGN(CdmPromiseTemplate);
};

template <>
class MEDIA_EXPORT CdmPromiseTemplate<std::string> : public CdmPromise {
 public:
  CdmPromiseTemplate(base::Callback<void(const std::string&)> resolve_cb,
                     PromiseRejectedCB rejected_cb);
  CdmPromiseTemplate(base::Callback<void(const std::string&)> resolve_cb,
                     PromiseRejectedCB rejected_cb,
                     const std::string& uma_name);
  virtual ~CdmPromiseTemplate();
  virtual void resolve(const std::string& result);
  virtual ResolveParameterType GetResolveParameterType() const OVERRIDE;

 protected:
  // Allow subclasses to completely override the implementation.
  // TODO(jrummell): Remove when derived class SessionLoadedPromise
  // (in ppapi_decryptor.cc) is no longer needed.
  CdmPromiseTemplate();

 private:
  base::Callback<void(const std::string&)> resolve_cb_;

  DISALLOW_COPY_AND_ASSIGN(CdmPromiseTemplate);
};

template <>
class MEDIA_EXPORT CdmPromiseTemplate<KeyIdsVector> : public CdmPromise {
 public:
  CdmPromiseTemplate(base::Callback<void(const KeyIdsVector&)> resolve_cb,
                     PromiseRejectedCB rejected_cb);
  CdmPromiseTemplate(base::Callback<void(const KeyIdsVector&)> resolve_cb,
                     PromiseRejectedCB rejected_cb,
                     const std::string& uma_name);
  virtual ~CdmPromiseTemplate();
  virtual void resolve(const KeyIdsVector& result);
  virtual ResolveParameterType GetResolveParameterType() const OVERRIDE;

 private:
  base::Callback<void(const KeyIdsVector&)> resolve_cb_;

  DISALLOW_COPY_AND_ASSIGN(CdmPromiseTemplate);
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_PROMISE_H_
