// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/utility/payment_manifest_parser.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/payments/content/android/utility/fingerprint_parser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace payments {

// static
void PaymentManifestParser::Create(
    mojom::PaymentManifestParserRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<PaymentManifestParser>(),
                          std::move(request));
}

// static
std::vector<mojom::PaymentManifestSectionPtr>
PaymentManifestParser::ParseIntoVector(const std::string& input) {
  std::vector<mojom::PaymentManifestSectionPtr> output;
  std::unique_ptr<base::Value> value(base::JSONReader::Read(input));
  if (!value)
    return output;

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict)
    return output;

  base::ListValue* list = nullptr;
  if (!dict->GetList("android", &list) || !list)
    return output;

  size_t sections_size = list->GetSize();
  const size_t kMaximumNumberOfSections = 100U;
  if (sections_size > kMaximumNumberOfSections)
    return output;

  const char* const kVersion = "version";
  const char* const kFingerprints = "sha256_cert_fingerprints";
  for (size_t i = 0; i < sections_size; ++i) {
    base::DictionaryValue* item = nullptr;
    if (!list->GetDictionary(i, &item) || !item) {
      output.clear();
      return output;
    }

    mojom::PaymentManifestSectionPtr section =
        mojom::PaymentManifestSection::New();
    section->version = 0;

    if (!item->GetString("package", &section->package_name) ||
        section->package_name.empty() ||
        !base::IsStringASCII(section->package_name)) {
      output.clear();
      return output;
    }

    if (section->package_name == "*") {
      output.clear();
      // If there's a section with "package": "*", then it must be the only
      // section and it should not have "version" or "sha256_cert_fingerprints".
      // (Any deviations from a correct format cause the full file to be
      // rejected.)
      if (!item->HasKey(kVersion) && !item->HasKey(kFingerprints) &&
          sections_size == 1U) {
        output.push_back(std::move(section));
      }
      return output;
    }

    if (!item->HasKey(kVersion) || !item->HasKey(kFingerprints)) {
      output.clear();
      return output;
    }

    int version = 0;
    if (!item->GetInteger(kVersion, &version)) {
      output.clear();
      return output;
    }

    section->version = static_cast<int64_t>(version);

    base::ListValue* fingerprints = nullptr;
    if (!item->GetList(kFingerprints, &fingerprints) || !fingerprints ||
        fingerprints->empty()) {
      output.clear();
      return output;
    }

    size_t fingerprints_size = fingerprints->GetSize();
    const size_t kMaximumNumberOfFingerprints = 100U;
    if (fingerprints_size > kMaximumNumberOfFingerprints) {
      output.clear();
      return output;
    }

    for (size_t j = 0; j < fingerprints_size; ++j) {
      std::string fingerprint;
      if (!fingerprints->GetString(j, &fingerprint) || fingerprint.empty()) {
        output.clear();
        return output;
      }

      std::vector<uint8_t> fingerprint_bytes =
          FingerprintStringToByteArray(fingerprint);
      if (32U != fingerprint_bytes.size()) {
        output.clear();
        return output;
      }

      section->sha256_cert_fingerprints.push_back(fingerprint_bytes);
    }

    output.push_back(std::move(section));
  }

  return output;
}

PaymentManifestParser::PaymentManifestParser() {}

PaymentManifestParser::~PaymentManifestParser() {}

void PaymentManifestParser::Parse(const std::string& content,
                                  const ParseCallback& callback) {
  callback.Run(ParseIntoVector(content));
}

}  // namespace payments
