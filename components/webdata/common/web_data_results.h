// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "components/webdata/common/webdata_export.h"

class WDTypedResult;

//
// Result types for WebDataService.
//
typedef enum {
  BOOL_RESULT = 1,             // WDResult<bool>
  KEYWORDS_RESULT,             // WDResult<WDKeywordsResult>
  INT64_RESULT,                // WDResult<int64>
#if defined(OS_WIN)
  PASSWORD_IE7_RESULT,         // WDResult<IE7PasswordInfo>
#endif
  WEB_APP_IMAGES,              // WDResult<WDAppImagesResult>
  TOKEN_RESULT,                // WDResult<std::vector<std::string>>
  AUTOFILL_VALUE_RESULT,       // WDResult<std::vector<base::string16>>
  AUTOFILL_CHANGES,            // WDResult<std::vector<AutofillChange>>
  AUTOFILL_PROFILE_RESULT,     // WDResult<AutofillProfile>
  AUTOFILL_PROFILES_RESULT,    // WDResult<std::vector<AutofillProfile*>>
  AUTOFILL_CREDITCARD_RESULT,  // WDResult<CreditCard>
  AUTOFILL_CREDITCARDS_RESULT, // WDResult<std::vector<CreditCard*>>
  WEB_INTENTS_RESULT,          // WDResult<std::vector<WebIntentServiceData>>
  WEB_INTENTS_DEFAULTS_RESULT, // WDResult<std::vector<DefaultWebIntentService>>
} WDResultType;


typedef base::Callback<void(const WDTypedResult*)> DestroyCallback;

//
// The top level class for a result.
//
class WEBDATA_EXPORT WDTypedResult {
 public:
  virtual ~WDTypedResult() {
  }

  // Return the result type.
  WDResultType GetType() const {
    return type_;
  }

  virtual void Destroy() {
  }

 protected:
  explicit WDTypedResult(WDResultType type)
    : type_(type) {
  }

 private:
  WDResultType type_;
  DISALLOW_COPY_AND_ASSIGN(WDTypedResult);
};

// A result containing one specific pointer or literal value.
template <class T> class WDResult : public WDTypedResult {
 public:
  WDResult(WDResultType type, const T& v)
      : WDTypedResult(type), value_(v) {
  }

  virtual ~WDResult() {
  }

  // Return a single value result.
  T GetValue() const {
    return value_;
  }

 private:
  T value_;

  DISALLOW_COPY_AND_ASSIGN(WDResult);
};

template <class T> class WDDestroyableResult : public WDTypedResult {
 public:
  WDDestroyableResult(
      WDResultType type,
      const T& v,
      const DestroyCallback& callback)
      : WDTypedResult(type),
        value_(v),
        callback_(callback) {
  }

  virtual ~WDDestroyableResult() {
  }


  virtual void Destroy()  OVERRIDE {
    if (!callback_.is_null()) {
      callback_.Run(this);
    }
  }

  // Return a single value result.
  T GetValue() const {
    return value_;
  }

 private:
  T value_;
  DestroyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WDDestroyableResult);
};

template <class T> class WDObjectResult : public WDTypedResult {
 public:
  explicit WDObjectResult(WDResultType type)
    : WDTypedResult(type) {
  }

  T* GetValue() const {
    return &value_;
  }

 private:
  // mutable to keep GetValue() const.
  mutable T value_;
  DISALLOW_COPY_AND_ASSIGN(WDObjectResult);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_
