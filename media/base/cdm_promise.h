// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_PROMISE_H_
#define MEDIA_BASE_CDM_PROMISE_H_

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/macros.h"
// TODO(xhwang): Remove this include after http://crbug.com/656706 is fixed.
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace media {

// Interface for promises being resolved/rejected in response to various
// session actions. These may be called synchronously or asynchronously.
// The promise must be resolved or rejected exactly once. It is expected that
// the caller free the promise once it is resolved/rejected.

// These classes are almost generic, except for the parameters to reject(). If
// a generic class for promises is available, this could be changed to use the
// generic class as long as the parameters to reject() can be set appropriately.

// The base class only has a reject() method and GetResolveParameterType() that
// indicates the type of CdmPromiseTemplate. CdmPromiseTemplate<T> adds the
// resolve(T) method that is dependent on the type of promise. This base class
// is specified so that the promises can be easily saved before passing across
// the pepper interface.
class MEDIA_EXPORT CdmPromise {
 public:
  // TODO(jrummell): Remove deprecated errors. See
  // http://crbug.com/570216
  enum Exception {
    NOT_SUPPORTED_ERROR,
    INVALID_STATE_ERROR,
    INVALID_ACCESS_ERROR,
    QUOTA_EXCEEDED_ERROR,
    UNKNOWN_ERROR,
    CLIENT_ERROR,
    OUTPUT_ERROR,
    EXCEPTION_MAX = OUTPUT_ERROR
  };

  enum ResolveParameterType {
    VOID_TYPE,
    INT_TYPE,
    STRING_TYPE,
    KEY_STATUS_TYPE
  };

  CdmPromise();
  virtual ~CdmPromise();

  // Used to indicate that the operation failed. |exception_code| must be
  // specified. |system_code| is a Key System-specific value for the error
  // that occurred, or 0 if there is no associated status code or such status
  // codes are not supported by the Key System. |error_message| is optional.
  virtual void reject(Exception exception_code,
                      uint32_t system_code,
                      const std::string& error_message) = 0;

  // Used to determine the template type of CdmPromiseTemplate<T> so that
  // saved CdmPromise objects can be cast to the correct templated version.
  virtual ResolveParameterType GetResolveParameterType() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmPromise);
};

// For some reason the Windows compiler is not happy with the implementation
// of CdmPromiseTemplate being in the .cc file, so moving it here.
template <typename... T>
struct CdmPromiseTraits {};

template <>
struct CdmPromiseTraits<> {
  static const CdmPromise::ResolveParameterType kType = CdmPromise::VOID_TYPE;
};

template <>
struct CdmPromiseTraits<int> {
  static const CdmPromise::ResolveParameterType kType = CdmPromise::INT_TYPE;
};

template <>
struct CdmPromiseTraits<std::string> {
  static const CdmPromise::ResolveParameterType kType = CdmPromise::STRING_TYPE;
};

template <>
struct CdmPromiseTraits<CdmKeyInformation::KeyStatus> {
  static const CdmPromise::ResolveParameterType kType =
      CdmPromise::KEY_STATUS_TYPE;
};

// This class adds the resolve(T) method. This class is still an interface, and
// is used as the type of promise that gets passed around.
template <typename... T>
class CdmPromiseTemplate : public CdmPromise {
 public:
  CdmPromiseTemplate() : is_settled_(false) {}

  virtual ~CdmPromiseTemplate() { DCHECK(is_settled_); }

  virtual void resolve(const T&... result) = 0;

  // CdmPromise implementation.
  virtual void reject(Exception exception_code,
                      uint32_t system_code,
                      const std::string& error_message) = 0;

  ResolveParameterType GetResolveParameterType() const override {
    return CdmPromiseTraits<T...>::kType;
  }

 protected:
  bool IsPromiseSettled() const { return is_settled_; }

  // All implementations must call this method in resolve() and reject() methods
  // to indicate that the promise has been settled.
  void MarkPromiseSettled() {
    // Promise can only be settled once.
    DCHECK(!is_settled_);
    is_settled_ = true;
  }

  // Must be called by the concrete destructor if !IsPromiseSettled().
  // Note: We can't call reject() in ~CdmPromise() because reject() is virtual.
  void RejectPromiseOnDestruction() {
    DCHECK(!is_settled_);
    std::string message =
        "Unfulfilled promise rejected automatically during destruction.";
    DVLOG(1) << message;
    reject(INVALID_STATE_ERROR, 0, message);
    DCHECK(is_settled_);
  }

 private:
  // Keep track of whether the promise has been resolved or rejected yet.
  bool is_settled_;

  DISALLOW_COPY_AND_ASSIGN(CdmPromiseTemplate);
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_PROMISE_H_
