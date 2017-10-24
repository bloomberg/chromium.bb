// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "url/gurl.h"
#include "url/origin.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

Environment* env = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::vector<GURL> web_app_manifest_urls;
  std::vector<url::Origin> supported_origins;
  bool all_origins_supported;

  std::string json_data(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<base::Value> value = base::JSONReader::Read(json_data);

  payments::PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
      std::move(value), &web_app_manifest_urls, &supported_origins,
      &all_origins_supported);
  return 0;
}
