// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/payments/content/utility/fingerprint_parser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/url_constants.h"

namespace payments {

// static
void PaymentManifestParser::Create(
    const service_manager::BindSourceInfo& source_info,
    mojom::PaymentManifestParserRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<PaymentManifestParser>(),
                          std::move(request));
}

// static
std::vector<GURL> PaymentManifestParser::ParsePaymentMethodManifestIntoVector(
    const std::string& input) {
  std::vector<GURL> output;
  std::unique_ptr<base::Value> value(base::JSONReader::Read(input));
  if (!value)
    return output;

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict)
    return output;

  base::ListValue* list = nullptr;
  if (!dict->GetList("default_applications", &list))
    return output;

  size_t apps_number = list->GetSize();
  const size_t kMaximumNumberOfApps = 100U;
  if (apps_number > kMaximumNumberOfApps)
    return output;

  std::string item;
  for (size_t i = 0; i < apps_number; ++i) {
    if (!list->GetString(i, &item) && item.empty()) {
      output.clear();
      return output;
    }

    GURL url(item);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      output.clear();
      return output;
    }

    output.push_back(url);
  }

  return output;
}

// static
std::vector<mojom::WebAppManifestSectionPtr>
PaymentManifestParser::ParseWebAppManifestIntoVector(const std::string& input) {
  std::vector<mojom::WebAppManifestSectionPtr> output;
  std::unique_ptr<base::Value> value(base::JSONReader::Read(input));
  if (!value)
    return output;

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict)
    return output;

  base::ListValue* list = nullptr;
  if (!dict->GetList("related_applications", &list))
    return output;

  size_t related_applications_size = list->GetSize();
  for (size_t i = 0; i < related_applications_size; ++i) {
    base::DictionaryValue* related_application = nullptr;
    if (!list->GetDictionary(i, &related_application) || !related_application) {
      output.clear();
      return output;
    }

    std::string platform;
    if (!related_application->GetString("platform", &platform) ||
        platform != "play") {
      continue;
    }

    const size_t kMaximumNumberOfRelatedApplications = 100U;
    if (output.size() >= kMaximumNumberOfRelatedApplications) {
      output.clear();
      return output;
    }

    const char* const kId = "id";
    const char* const kMinVersion = "min_version";
    const char* const kFingerprints = "fingerprints";
    if (!related_application->HasKey(kId) ||
        !related_application->HasKey(kMinVersion) ||
        !related_application->HasKey(kFingerprints)) {
      output.clear();
      return output;
    }

    mojom::WebAppManifestSectionPtr section =
        mojom::WebAppManifestSection::New();
    section->min_version = 0;

    if (!related_application->GetString(kId, &section->id) ||
        section->id.empty() || !base::IsStringASCII(section->id)) {
      output.clear();
      return output;
    }

    std::string min_version;
    if (!related_application->GetString(kMinVersion, &min_version) ||
        min_version.empty() || !base::IsStringASCII(min_version) ||
        !base::StringToInt64(min_version, &section->min_version)) {
      output.clear();
      return output;
    }

    const size_t kMaximumNumberOfFingerprints = 100U;
    base::ListValue* fingerprints_list = nullptr;
    if (!related_application->GetList(kFingerprints, &fingerprints_list) ||
        fingerprints_list->empty() ||
        fingerprints_list->GetSize() > kMaximumNumberOfFingerprints) {
      output.clear();
      return output;
    }

    size_t fingerprints_size = fingerprints_list->GetSize();
    for (size_t j = 0; j < fingerprints_size; ++j) {
      base::DictionaryValue* fingerprint_dict = nullptr;
      std::string fingerprint_type;
      std::string fingerprint_value;
      if (!fingerprints_list->GetDictionary(i, &fingerprint_dict) ||
          !fingerprint_dict ||
          !fingerprint_dict->GetString("type", &fingerprint_type) ||
          fingerprint_type != "sha256_cert" ||
          !fingerprint_dict->GetString("value", &fingerprint_value) ||
          fingerprint_value.empty()) {
        output.clear();
        return output;
      }

      std::vector<uint8_t> hash =
          FingerprintStringToByteArray(fingerprint_value);
      if (hash.empty()) {
        output.clear();
        return output;
      }

      section->fingerprints.push_back(hash);
    }

    output.push_back(std::move(section));
  }

  return output;
}

PaymentManifestParser::PaymentManifestParser() {}

PaymentManifestParser::~PaymentManifestParser() {}

void PaymentManifestParser::ParsePaymentMethodManifest(
    const std::string& content,
    ParsePaymentMethodManifestCallback callback) {
  std::move(callback).Run(ParsePaymentMethodManifestIntoVector(content));
}

void PaymentManifestParser::ParseWebAppManifest(
    const std::string& content,
    ParseWebAppManifestCallback callback) {
  std::move(callback).Run(ParseWebAppManifestIntoVector(content));
}

}  // namespace payments
