// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_UTILITY_PAYMENT_MANIFEST_PARSER_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_UTILITY_PAYMENT_MANIFEST_PARSER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/payments/content/android/payment_manifest_parser.mojom.h"

namespace payments {

// Parser for payment manifests. Should be used only in a utility process.
//
// Example valid manifest structure:
//
// {
//   "android": [{
//     "package": "com.bobpay.app",
//     "version": 1,
//     "sha256_cert_fingerprints": ["12:34:56:78:90:AB:CD:EF"]
//   }]
// }
//
// Spec:
// https://docs.google.com/document/d/1izV4uC-tiRJG3JLooqY3YRLU22tYOsLTNq0P_InPJeE/edit#heading=h.cjp3jlnl47h5
class PaymentManifestParser : public mojom::PaymentManifestParser {
 public:
  static void Create(mojom::PaymentManifestParserRequest request);

  // The return value is move-only, so no copying occurs.
  static std::vector<mojom::PaymentManifestSectionPtr> ParseIntoVector(
      const std::string& input);

  PaymentManifestParser();
  ~PaymentManifestParser() override;

  // mojom::PaymentManifestParser
  void Parse(const std::string& content,
             const ParseCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParser);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_UTILITY_PAYMENT_MANIFEST_PARSER_H_
