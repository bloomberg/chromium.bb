// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_PARSER_HOST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_PARSER_HOST_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/payments/content/payment_manifest_parser.mojom.h"

namespace content {
template <class MojoInterface>
class UtilityProcessMojoClient;
}

namespace payments {

// Host of the utility process that parses manifest contents.
class PaymentManifestParserHost {
 public:
  // Called on successful parsing. The result is a move-only vector, which is
  // empty on parse failure.
  using Callback =
      base::OnceCallback<void(std::vector<mojom::PaymentManifestSectionPtr>)>;

  PaymentManifestParserHost();

  // Stops the utility process.
  ~PaymentManifestParserHost();

  // Starts the utility process. This can take up to 2 seconds and should be
  // done as soon as it is known that the parser will be needed.
  void StartUtilityProcess();

  void ParsePaymentManifest(const std::string& content, Callback callback);

 private:
  // The |callback_identifier| parameter is a pointer to one of the items in the
  // |pending_callbacks_| list.
  void OnParse(const Callback* callback_identifier,
               std::vector<mojom::PaymentManifestSectionPtr> manifest);

  void OnUtilityProcessStopped();

  std::unique_ptr<
      content::UtilityProcessMojoClient<mojom::PaymentManifestParser>>
      mojo_client_;

  std::vector<Callback> pending_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParserHost);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_MANIFEST_PARSER_HOST_H_
