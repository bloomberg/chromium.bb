// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_
#define COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/payments/mojom/payment_manifest_parser.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace payments {

// Parser for payment method manifests and web app manifests. Should be used
// only in a sandboxed utility process.
//
// Example 1 of valid payment method manifest structure:
//
// {
//   "default_applications": ["https://bobpay.com/payment-app.json"],
//   "supported_origins": ["https://alicepay.com"]
// }
//
// Example 2 of valid payment method manifest structure:
//
// {
//   "default_applications": ["https://bobpay.com/payment-app.json"],
//   "supported_origins": "*"
// }
//
// Example valid web app manifest structure:
//
// {
//   "related_applications": [{
//     "platform": "play",
//     "id": "com.bobpay.app",
//     "min_version": "1",
//     "fingerprint": [{
//       "type": "sha256_cert",
//       "value": "91:5C:88:65:FF:C4:E8:20:CF:F7:3E:C8:64:D0:95:F0:06:19:2E:A6"
//     }]
//   }]
// }
//
// Spec:
// https://docs.google.com/document/d/1izV4uC-tiRJG3JLooqY3YRLU22tYOsLTNq0P_InPJeE
class PaymentManifestParser : public mojom::PaymentManifestParser {
 public:
  static void Create(const service_manager::BindSourceInfo& source_info,
                     mojom::PaymentManifestParserRequest request);

  static void ParsePaymentMethodManifestIntoVectors(
      const std::string& input,
      std::vector<GURL>* web_app_manifest_urls,
      std::vector<url::Origin>* supported_origins,
      bool* all_origins_supported);

  // The return value is move-only, so no copying occurs.
  static std::vector<mojom::WebAppManifestSectionPtr>
  ParseWebAppManifestIntoVector(const std::string& input);

  PaymentManifestParser();
  ~PaymentManifestParser() override;

  // mojom::PaymentManifestParser
  void ParsePaymentMethodManifest(
      const std::string& content,
      ParsePaymentMethodManifestCallback callback) override;
  void ParseWebAppManifest(const std::string& content,
                           ParseWebAppManifestCallback callack) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParser);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_UTILITY_PAYMENT_MANIFEST_PARSER_H_
