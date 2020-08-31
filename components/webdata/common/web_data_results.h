// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_

#include <stdint.h>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/webdata/common/webdata_export.h"

class WDTypedResult;

//
// Result types for WebDataService.
//
typedef enum {
  BOOL_RESULT = 1,               // WDResult<bool>
  KEYWORDS_RESULT,               // WDResult<WDKeywordsResult>
  INT64_RESULT,                  // WDResult<int64_t>
#if defined(OS_WIN)              //
  PASSWORD_IE7_RESULT,           // WDResult<IE7PasswordInfo>
#endif                           //
  WEB_APP_IMAGES,                // WDResult<WDAppImagesResult>
  TOKEN_RESULT,                  // WDResult<TokenResult>
  AUTOFILL_VALUE_RESULT,         // WDResult<std::vector<AutofillEntry>>
  AUTOFILL_CLEANUP_RESULT,       // WDResult<size_t>
  AUTOFILL_CHANGES,              // WDResult<std::vector<AutofillChange>>
  AUTOFILL_PROFILE_RESULT,       // WDResult<AutofillProfile>
  AUTOFILL_PROFILES_RESULT,      // WDResult<std::vector<
                                 //     std::unique_ptr<AutofillProfile>>>
  AUTOFILL_CLOUDTOKEN_RESULT,    // WDResult<std::vector<std::unique_ptr<
                                 //     CreditCardCloudTokenData>>>
  AUTOFILL_CREDITCARD_RESULT,    // WDResult<CreditCard>
  AUTOFILL_CREDITCARDS_RESULT,   // WDResult<std::vector<
                                 //     std::unique_ptr<CreditCard>>>
  AUTOFILL_CUSTOMERDATA_RESULT,  // WDResult<std::unique_ptr<
                                 //     PaymentsCustomerData>>
  AUTOFILL_UPI_RESULT,           // WDResult<std::string>
#if !defined(OS_IOS)             //
  PAYMENT_WEB_APP_MANIFEST,      // WDResult<std::vector<
                                 //     mojom::WebAppManifestSectionPtr>>
  PAYMENT_METHOD_MANIFEST,       // WDResult<std::vector<std::string>>
#endif
} WDResultType;

//
// The top level class for a result.
//
class WEBDATA_EXPORT WDTypedResult {
 public:
  virtual ~WDTypedResult() {}

  // Return the result type.
  WDResultType GetType() const { return type_; }

 protected:
  explicit WDTypedResult(WDResultType type) : type_(type) {}

 private:
  WDResultType type_;
  DISALLOW_COPY_AND_ASSIGN(WDTypedResult);
};

// A result containing one specific pointer or literal value.
template <class T>
class WDResult : public WDTypedResult {
 public:
  WDResult(WDResultType type, const T& v) : WDTypedResult(type), value_(v) {}
  WDResult(WDResultType type, T&& v)
      : WDTypedResult(type), value_(std::move(v)) {}

  ~WDResult() override {}

  // Return a single value result.
  const T& GetValue() const { return value_; }
  T GetValue() { return std::move(value_); }

 private:
  T value_;

  DISALLOW_COPY_AND_ASSIGN(WDResult);
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_RESULTS_H_
