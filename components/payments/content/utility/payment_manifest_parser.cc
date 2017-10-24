// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/utility/payment_manifest_parser.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/payments/content/utility/fingerprint_parser.h"
#include "content/public/common/service_manager_connection.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "url/url_constants.h"

namespace payments {
namespace {

const size_t kMaximumNumberOfItems = 100U;

const char* const kDefaultApplications = "default_applications";
const char* const kSupportedOrigins = "supported_origins";
const char* const kHttpsPrefix = "https://";

void RunPaymentMethodCallbackForError(
    PaymentManifestParser::PaymentMethodCallback callback) {
  std::move(callback).Run(std::vector<GURL>(), std::vector<url::Origin>(),
                          /*all_origins_supported=*/false);
}

void RunWebAppCallbackForError(PaymentManifestParser::WebAppCallback callback) {
  std::move(callback).Run(std::vector<WebAppManifestSection>());
}

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
  if (apps_number > kMaximumNumberOfItems) {
    LOG(ERROR) << "\"" << kDefaultApplications << "\" must contain at most "
               << kMaximumNumberOfItems << " entries.";
    return false;
  }

  for (size_t i = 0; i < apps_number; ++i) {
    std::string item;
    if (!list->GetString(i, &item) || item.empty() ||
        !base::IsStringUTF8(item) ||
        !base::StartsWith(item, kHttpsPrefix, base::CompareCase::SENSITIVE)) {
      LOG(ERROR) << "Each entry in \"" << kDefaultApplications
                 << "\" must be UTF8 string that starts with \"" << kHttpsPrefix
                 << "\".";
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

  {
    std::string item;
    if (dict->GetString(kSupportedOrigins, &item)) {
      if (item != "*") {
        LOG(ERROR) << "\"" << item << "\" is not a valid value for \""
                   << kSupportedOrigins
                   << "\". Must be either \"*\" or a list of RFC6454 origins.";
        return false;
      }

      *all_origins_supported = true;
      return true;
    }
  }

  base::ListValue* list = nullptr;
  if (!dict->GetList(kSupportedOrigins, &list)) {
    LOG(ERROR) << "\"" << kSupportedOrigins
               << "\" must be either \"*\" or a list of origins.";
    return false;
  }

  size_t supported_origins_number = list->GetSize();
  const size_t kMaximumNumberOfSupportedOrigins = 100000;
  if (supported_origins_number > kMaximumNumberOfSupportedOrigins) {
    LOG(ERROR) << "\"" << kSupportedOrigins << "\" must contain at most "
               << kMaximumNumberOfSupportedOrigins << " entires.";
    return false;
  }

  for (size_t i = 0; i < supported_origins_number; ++i) {
    std::string item;
    if (!list->GetString(i, &item) || item.empty() ||
        !base::IsStringUTF8(item) ||
        !base::StartsWith(item, kHttpsPrefix, base::CompareCase::SENSITIVE)) {
      LOG(ERROR) << "Each entry in \"" << kSupportedOrigins
                 << "\" must be UTF8 string that starts with \"" << kHttpsPrefix
                 << "\".";
      supported_origins->clear();
      return false;
    }

    GURL url(item);
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) ||
        url.path() != "/" || url.has_query() || url.has_ref() ||
        url.has_username() || url.has_password()) {
      LOG(ERROR) << "\"" << item << "\" entry in \"" << kSupportedOrigins
                 << "\" is not a valid origin with HTTPS scheme.";
      supported_origins->clear();
      return false;
    }

    supported_origins->push_back(url::Origin::Create(url));
  }

  return true;
}

// An object that allows both SafeJsonParser's callbacks (error/success) to run
// the same callback provided to ParsePaymentMethodManifest/ParseWebAppManifest.
// (since Callbacks are movable type, that callback has to be shared)
template <typename Callback>
class JsonParserCallback
    : public base::RefCounted<JsonParserCallback<Callback>> {
 public:
  JsonParserCallback(
      base::Callback<void(Callback, std::unique_ptr<base::Value>)>
          parser_callback,
      Callback client_callback)
      : parser_callback_(std::move(parser_callback)),
        client_callback_(std::move(client_callback)) {}

  void OnSuccess(std::unique_ptr<base::Value> value) {
    std::move(parser_callback_)
        .Run(std::move(client_callback_), std::move(value));
  }

  void OnError(const std::string& error_message) {
    std::move(parser_callback_)
        .Run(std::move(client_callback_), /*value=*/nullptr);
  }

 private:
  friend class base::RefCounted<JsonParserCallback>;
  ~JsonParserCallback() = default;

  base::Callback<void(Callback, std::unique_ptr<base::Value>)> parser_callback_;
  Callback client_callback_;
};

}  // namespace

PaymentManifestParser::PaymentManifestParser() : weak_factory_(this) {}

PaymentManifestParser::~PaymentManifestParser() = default;

void PaymentManifestParser::ParsePaymentMethodManifest(
    const std::string& content,
    PaymentMethodCallback callback) {
  parse_payment_callback_counter_++;
  DCHECK_GE(10U, parse_payment_callback_counter_);

  scoped_refptr<JsonParserCallback<PaymentMethodCallback>> json_callback =
      new JsonParserCallback<PaymentMethodCallback>(
          base::Bind(&PaymentManifestParser::OnPaymentMethodParse,
                     weak_factory_.GetWeakPtr()),
          std::move(callback));

  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      content,
      base::Bind(&JsonParserCallback<PaymentMethodCallback>::OnSuccess,
                 json_callback),
      base::Bind(&JsonParserCallback<PaymentMethodCallback>::OnError,
                 json_callback));
}

void PaymentManifestParser::ParseWebAppManifest(const std::string& content,
                                                WebAppCallback callback) {
  parse_webapp_callback_counter_++;
  DCHECK_GE(10U, parse_webapp_callback_counter_);

  scoped_refptr<JsonParserCallback<WebAppCallback>> parser_callback =
      new JsonParserCallback<WebAppCallback>(
          base::Bind(&PaymentManifestParser::OnWebAppParse,
                     weak_factory_.GetWeakPtr()),
          std::move(callback));

  data_decoder::SafeJsonParser::Parse(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      content,
      base::Bind(&JsonParserCallback<WebAppCallback>::OnSuccess,
                 parser_callback),
      base::Bind(&JsonParserCallback<WebAppCallback>::OnError,
                 parser_callback));
}

void PaymentManifestParser::OnPaymentMethodParse(
    PaymentMethodCallback callback,
    std::unique_ptr<base::Value> value) {
  parse_payment_callback_counter_--;

  std::vector<GURL> web_app_manifest_urls;
  std::vector<url::Origin> supported_origins;
  bool all_origins_supported = false;
  ParsePaymentMethodManifestIntoVectors(
      std::move(value), &web_app_manifest_urls, &supported_origins,
      &all_origins_supported);

  const size_t kMaximumNumberOfSupportedOrigins = 100000;
  if (web_app_manifest_urls.size() > kMaximumNumberOfItems ||
      supported_origins.size() > kMaximumNumberOfSupportedOrigins) {
    // If more than 100 web app manifests URLs or more than 100,000 supported
    // origins, then something went wrong.
    RunPaymentMethodCallbackForError(std::move(callback));
    return;
  }

  for (const auto& url : web_app_manifest_urls) {
    if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
      // If not a valid URL with HTTPS scheme, then something went wrong.
      RunPaymentMethodCallbackForError(std::move(callback));
      return;
    }
  }

  if (all_origins_supported && !supported_origins.empty()) {
    // The format of the payment method manifest does not allow for both of
    // these conditions to be true.
    RunPaymentMethodCallbackForError(std::move(callback));
    return;
  }

  for (const auto& origin : supported_origins) {
    if (!origin.GetURL().is_valid() || origin.scheme() != url::kHttpsScheme) {
      // If not a valid origin with HTTPS scheme, then something went wrong.
      RunPaymentMethodCallbackForError(std::move(callback));
      return;
    }
  }

  // Can trigger synchronous deletion of this object, so can't access any of the
  // member variables after this block.
  std::move(callback).Run(web_app_manifest_urls, supported_origins,
                          all_origins_supported);
}

void PaymentManifestParser::OnWebAppParse(WebAppCallback callback,
                                          std::unique_ptr<base::Value> value) {
  parse_webapp_callback_counter_--;

  std::vector<WebAppManifestSection> manifest;
  ParseWebAppManifestIntoVector(std::move(value), &manifest);

  if (manifest.size() > kMaximumNumberOfItems) {
    // If more than 100 items, then something went wrong.
    RunWebAppCallbackForError(std::move(callback));
    return;
  }

  for (size_t i = 0; i < manifest.size(); ++i) {
    if (manifest[i].fingerprints.size() > kMaximumNumberOfItems) {
      // If more than 100 items, then something went wrong.
      RunWebAppCallbackForError(std::move(callback));
      return;
    }
  }

  // Can trigger synchronous deletion of this object, so can't access any of the
  // member variables after this block.
  std::move(callback).Run(manifest);
}

// static
void PaymentManifestParser::ParsePaymentMethodManifestIntoVectors(
    std::unique_ptr<base::Value> value,
    std::vector<GURL>* web_app_manifest_urls,
    std::vector<url::Origin>* supported_origins,
    bool* all_origins_supported) {
  DCHECK(web_app_manifest_urls);
  DCHECK(supported_origins);
  DCHECK(all_origins_supported);

  *all_origins_supported = false;

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
bool PaymentManifestParser::ParseWebAppManifestIntoVector(
    std::unique_ptr<base::Value> value,
    std::vector<WebAppManifestSection>* output) {
  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict) {
    LOG(ERROR) << "Web app manifest must be a JSON dictionary.";
    return false;
  }

  base::ListValue* list = nullptr;
  if (!dict->GetList("related_applications", &list)) {
    LOG(ERROR) << "\"related_applications\" must be a list.";
    return false;
  }

  size_t related_applications_size = list->GetSize();
  for (size_t i = 0; i < related_applications_size; ++i) {
    base::DictionaryValue* related_application = nullptr;
    if (!list->GetDictionary(i, &related_application) || !related_application) {
      LOG(ERROR) << "\"related_applications\" must be a list of dictionaries.";
      output->clear();
      return false;
    }

    std::string platform;
    if (!related_application->GetString("platform", &platform) ||
        platform != "play") {
      continue;
    }

    if (output->size() >= kMaximumNumberOfItems) {
      LOG(ERROR) << "\"related_applications\" must contain at most "
                 << kMaximumNumberOfItems
                 << " entries with \"platform\": \"play\".";
      output->clear();
      return false;
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
      return false;
    }

    WebAppManifestSection section;
    section.min_version = 0;

    if (!related_application->GetString(kId, &section.id) ||
        section.id.empty() || !base::IsStringASCII(section.id)) {
      LOG(ERROR) << "\"" << kId << "\" must be a non-empty ASCII string.";
      output->clear();
      return false;
    }

    std::string min_version;
    if (!related_application->GetString(kMinVersion, &min_version) ||
        min_version.empty() || !base::IsStringASCII(min_version) ||
        !base::StringToInt64(min_version, &section.min_version)) {
      LOG(ERROR) << "\"" << kMinVersion
                 << "\" must be a string convertible into a number.";
      output->clear();
      return false;
    }

    base::ListValue* fingerprints_list = nullptr;
    if (!related_application->GetList(kFingerprints, &fingerprints_list) ||
        fingerprints_list->empty() ||
        fingerprints_list->GetSize() > kMaximumNumberOfItems) {
      LOG(ERROR) << "\"" << kFingerprints
                 << "\" must be a non-empty list of at most "
                 << kMaximumNumberOfItems << " items.";
      output->clear();
      return false;
    }

    size_t fingerprints_size = fingerprints_list->GetSize();
    for (size_t j = 0; j < fingerprints_size; ++j) {
      base::DictionaryValue* fingerprint_dict = nullptr;
      std::string fingerprint_type;
      std::string fingerprint_value;
      if (!fingerprints_list->GetDictionary(j, &fingerprint_dict) ||
          !fingerprint_dict ||
          !fingerprint_dict->GetString("type", &fingerprint_type) ||
          fingerprint_type != "sha256_cert" ||
          !fingerprint_dict->GetString("value", &fingerprint_value) ||
          fingerprint_value.empty() ||
          !base::IsStringASCII(fingerprint_value)) {
        LOG(ERROR) << "Each entry in \"" << kFingerprints
                   << "\" must be a dictionary with \"type\": "
                      "\"sha256_cert\" and a non-empty ASCII string \"value\".";
        output->clear();
        return false;
      }

      std::vector<uint8_t> hash =
          FingerprintStringToByteArray(fingerprint_value);
      if (hash.empty()) {
        output->clear();
        return false;
      }

      section.fingerprints.push_back(hash);
    }

    output->push_back(section);
  }

  return true;
}

}  // namespace payments
