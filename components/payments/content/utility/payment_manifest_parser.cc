// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/payments/content/utility/fingerprint_parser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/url_constants.h"

namespace payments {
namespace {

const size_t kMaximumNumberOfEntries = 100U;
const char* const kDefaultApplications = "default_applications";
const char* const kSupportedOrigins = "supported_origins";

// Parses the "default_applications": ["https://some/url"] from |dict| into
// |web_app_manifest_urls|. Returns 'false' for invalid data.
bool ParseDefaultApplications(base::DictionaryValue* dict,
                              std::vector<GURL>* web_app_manifest_urls) {
  DCHECK(dict);
  DCHECK(web_app_manifest_urls);

  base::ListValue* list = nullptr;
  if (!dict->GetList(kDefaultApplications, &list)) {
    LOG(ERROR) << "\"" << kDefaultApplications << "\" must be a list.";
    return false;
  }

  size_t apps_number = list->GetSize();
  if (apps_number > kMaximumNumberOfEntries) {
    LOG(ERROR) << "\"" << kDefaultApplications << "\" must contain at most "
               << kMaximumNumberOfEntries << " entries.";
    return false;
  }

  std::string item;
  for (size_t i = 0; i < apps_number; ++i) {
    if (!list->GetString(i, &item) && item.empty()) {
      LOG(ERROR) << "Each entry in \"" << kDefaultApplications
                 << "\" must be a string.";
      web_app_manifest_urls->clear();
      return false;
    }

    GURL url(item);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      LOG(ERROR) << "\"" << item << "\" entry in \"" << kDefaultApplications
                 << "\" is not a valid URL with HTTPS scheme.";
      web_app_manifest_urls->clear();
      return false;
    }

    web_app_manifest_urls->push_back(url);
  }

  return true;
}

// Parses the "supported_origins": "*" (or ["https://some.origin"]) from |dict|
// into |supported_origins| and |all_origins_supported|. Returns 'false' for
// invalid data.
bool ParseSupportedOrigins(base::DictionaryValue* dict,
                           std::vector<url::Origin>* supported_origins,
                           bool* all_origins_supported) {
  DCHECK(dict);
  DCHECK(supported_origins);
  DCHECK(all_origins_supported);

  *all_origins_supported = false;

  std::string item;
  if (dict->GetString(kSupportedOrigins, &item)) {
    if (item != "*") {
      LOG(ERROR) << "\"" << item << "\" is not a valid value for \""
                 << kSupportedOrigins
                 << "\". Must be either \"*\" or a list of origins.";
      return false;
    }

    *all_origins_supported = true;
    return true;
  }

  base::ListValue* list = nullptr;
  if (!dict->GetList(kSupportedOrigins, &list)) {
    LOG(ERROR) << "\"" << kSupportedOrigins
               << "\" must be either \"*\" or a list of origins.";
    return false;
  }

  size_t supported_origins_number = list->GetSize();
  if (supported_origins_number > kMaximumNumberOfEntries) {
    LOG(ERROR) << "\"" << kSupportedOrigins << "\" must contain at most "
               << kMaximumNumberOfEntries << " entires.";
    return false;
  }

  for (size_t i = 0; i < supported_origins_number; ++i) {
    if (!list->GetString(i, &item) && item.empty()) {
      LOG(ERROR) << "Each entry in \"" << kSupportedOrigins
                 << "\" must be a string.";
      supported_origins->clear();
      return false;
    }

    GURL url(item);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) ||
        url.path() != "/" || url.has_query() || url.has_ref()) {
      LOG(ERROR) << "\"" << item << "\" entry in \"" << kSupportedOrigins
                 << "\" is not a valid origin with HTTPS scheme.";
      supported_origins->clear();
      return false;
    }

    supported_origins->push_back(url::Origin(url));
  }

  return true;
}

}  // namespace

// static
void PaymentManifestParser::Create(
    const service_manager::BindSourceInfo& source_info,
    mojom::PaymentManifestParserRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<PaymentManifestParser>(),
                          std::move(request));
}

// static
void PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
    const std::string& input,
    std::vector<GURL>* web_app_manifest_urls,
    std::vector<url::Origin>* supported_origins,
    bool* all_origins_supported) {
  DCHECK(web_app_manifest_urls);
  DCHECK(supported_origins);
  DCHECK(all_origins_supported);

  *all_origins_supported = false;

  std::unique_ptr<base::Value> value(base::JSONReader::Read(input));
  if (!value) {
    LOG(ERROR) << "Payment method manifest must be in JSON format.";
    return;
  }

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict) {
    LOG(ERROR) << "Payment method manifest must be a JSON dictionary.";
    return;
  }

  if (dict->HasKey(kDefaultApplications) &&
      !ParseDefaultApplications(dict.get(), web_app_manifest_urls)) {
    return;
  }

  if (dict->HasKey(kSupportedOrigins) &&
      !ParseSupportedOrigins(dict.get(), supported_origins,
                             all_origins_supported)) {
    web_app_manifest_urls->clear();
  }
}

// static
std::vector<mojom::WebAppManifestSectionPtr>
PaymentManifestParser::ParseWebAppManifestIntoVector(const std::string& input) {
  std::vector<mojom::WebAppManifestSectionPtr> output;
  std::unique_ptr<base::Value> value(base::JSONReader::Read(input));
  if (!value) {
    LOG(ERROR) << "Web app manifest must be in JSON format.";
    return output;
  }

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict) {
    LOG(ERROR) << "Web app manifest must be a JSON dictionary.";
    return output;
  }

  base::ListValue* list = nullptr;
  if (!dict->GetList("related_applications", &list)) {
    LOG(ERROR) << "\"related_applications\" must be a list.";
    return output;
  }

  size_t related_applications_size = list->GetSize();
  for (size_t i = 0; i < related_applications_size; ++i) {
    base::DictionaryValue* related_application = nullptr;
    if (!list->GetDictionary(i, &related_application) || !related_application) {
      LOG(ERROR) << "\"related_applications\" must be a list of dictionaries.";
      output.clear();
      return output;
    }

    std::string platform;
    if (!related_application->GetString("platform", &platform) ||
        platform != "play") {
      continue;
    }

    if (output.size() >= kMaximumNumberOfEntries) {
      LOG(ERROR) << "\"related_applications\" must contain at most "
                 << kMaximumNumberOfEntries
                 << " entries with \"platform\": \"play\".";
      output.clear();
      return output;
    }

    const char* const kId = "id";
    const char* const kMinVersion = "min_version";
    const char* const kFingerprints = "fingerprints";
    if (!related_application->HasKey(kId) ||
        !related_application->HasKey(kMinVersion) ||
        !related_application->HasKey(kFingerprints)) {
      LOG(ERROR) << "Each \"platform\": \"play\" entry in "
                    "\"related_applications\" must contain \""
                 << kId << "\", \"" << kMinVersion << "\", and \""
                 << kFingerprints << "\".";
      return output;
    }

    mojom::WebAppManifestSectionPtr section =
        mojom::WebAppManifestSection::New();
    section->min_version = 0;

    if (!related_application->GetString(kId, &section->id) ||
        section->id.empty() || !base::IsStringASCII(section->id)) {
      LOG(ERROR) << "\"" << kId << "\" must be a non-empty ASCII string.";
      output.clear();
      return output;
    }

    std::string min_version;
    if (!related_application->GetString(kMinVersion, &min_version) ||
        min_version.empty() || !base::IsStringASCII(min_version) ||
        !base::StringToInt64(min_version, &section->min_version)) {
      LOG(ERROR) << "\"" << kMinVersion
                 << "\" must be a string convertible into a number.";
      output.clear();
      return output;
    }

    base::ListValue* fingerprints_list = nullptr;
    if (!related_application->GetList(kFingerprints, &fingerprints_list) ||
        fingerprints_list->empty() ||
        fingerprints_list->GetSize() > kMaximumNumberOfEntries) {
      LOG(ERROR) << "\"" << kFingerprints
                 << "\" must be a non-empty list of at most "
                 << kMaximumNumberOfEntries << " items.";
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
          fingerprint_value.empty() ||
          !base::IsStringASCII(fingerprint_value)) {
        LOG(ERROR) << "Each entry in \"" << kFingerprints
                   << "\" must be a dictionary with \"type\": "
                      "\"sha256_cert\" and a non-empty ASCII string \"value\".";
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
  std::vector<GURL> web_app_manifest_urls;
  std::vector<url::Origin> supported_origins;
  bool all_origins_supported = false;

  ParsePaymentMethodManifestIntoVectors(content, &web_app_manifest_urls,
                                        &supported_origins,
                                        &all_origins_supported);

  std::move(callback).Run(web_app_manifest_urls, supported_origins,
                          all_origins_supported);
}

void PaymentManifestParser::ParseWebAppManifest(
    const std::string& content,
    ParseWebAppManifestCallback callback) {
  std::move(callback).Run(ParseWebAppManifestIntoVector(content));
}

}  // namespace payments
