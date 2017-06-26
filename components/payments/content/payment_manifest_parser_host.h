// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_PARSER_HOST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_PARSER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/payments/mojom/payment_manifest_parser.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
template <class MojoInterface>
class UtilityProcessMojoClient;
}

namespace payments {

class PaymentManifestParserHostTest;

// Host of the utility process that parses manifest contents.
class PaymentManifestParserHost {
 public:
  // Called on successful parsing of a payment method manifest. Parse failure
  // results in empty vectors and "false".
  using PaymentMethodCallback = base::OnceCallback<
      void(const std::vector<GURL>&, const std::vector<url::Origin>&, bool)>;
  // Called on successful parsing of a web app manifest. The result is a
  // move-only vector, which is empty on parse failure.
  using WebAppCallback =
      base::OnceCallback<void(std::vector<mojom::WebAppManifestSectionPtr>)>;

  PaymentManifestParserHost();

  // Stops the utility process.
  ~PaymentManifestParserHost();

  // Starts the utility process. This can take up to 2 seconds and should be
  // done as soon as it is known that the parser will be needed.
  void StartUtilityProcess();

  void ParsePaymentMethodManifest(const std::string& content,
                                  PaymentMethodCallback callback);
  void ParseWebAppManifest(const std::string& content, WebAppCallback callback);

 private:
  friend class PaymentManifestParserHostTest;

  void OnPaymentMethodParse(int64_t callback_identifier,
                            const std::vector<GURL>& web_app_manifest_urls,
                            const std::vector<url::Origin>& supported_origins,
                            bool all_origins_supported);
  void OnWebAppParse(int64_t callback_identifier,
                     std::vector<mojom::WebAppManifestSectionPtr> manifest);

  void OnUtilityProcessStopped();

  std::unique_ptr<
      content::UtilityProcessMojoClient<mojom::PaymentManifestParser>>
      mojo_client_;

  int64_t callback_counter_;
  std::map<int64_t, PaymentMethodCallback> pending_payment_method_callbacks_;
  std::map<int64_t, WebAppCallback> pending_web_app_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParserHost);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_PARSER_HOST_H_
