// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_download_manager.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_protobufs.h"
#include "components/autofill/core/browser/proto/legacy_proto_bridge.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// The reserved identifier ranges for autofill server experiments.
constexpr std::pair<int, int> kAutofillExperimentRanges[] = {
    {3312923, 3312930}, {3314208, 3314209}, {3314711, 3314712},
    {3314445, 3314448}, {3314854, 3314883},
};

const size_t kMaxQueryGetSize = 1400;  // 1.25 KiB
const size_t kAutofillDownloadManagerMaxFormCacheSize = 16;
const size_t kMaxFieldsPerQueryRequest = 100;

const net::BackoffEntry::Policy kAutofillBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    1000,  // 1 second.

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.33,  // 33%.

    // Maximum amount of time we are willing to delay our request in ms.
    30 * 1000,  // 30 seconds.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

const char kDefaultAutofillServerURL[] =
    "https://content-autofill.googleapis.com/";

// The default number of days after which to reset the registry of autofill
// events for which an upload has been sent.
constexpr base::FeatureParam<int> kAutofillUploadThrottlingPeriodInDays(
    &features::kAutofillUploadThrottling,
    switches::kAutofillUploadThrottlingPeriodInDays,
    28);

// Header for API key.
constexpr char kGoogApiKey[] = "X-Goog-Api-Key";
// Header to get base64 encoded serialized proto from API for safety.
constexpr char kGoogEncodeResponseIfExecutable[] =
    "X-Goog-Encode-Response-If-Executable";

constexpr char kDefaultAPIKey[] = "";

// The maximum number of attempts for a given autofill request.
constexpr base::FeatureParam<int> kAutofillMaxServerAttempts(
    &features::kAutofillServerCommunication,
    "max-attempts",
    5);

// Returns the base URL for the autofill server.
GURL GetAutofillServerURL() {
  // If a valid autofill server URL is specified on the command line, then the
  // AutofillDownlaodManager will use it, and assume that server communication
  // is enabled.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAutofillServerURL)) {
    GURL url(command_line.GetSwitchValueASCII(switches::kAutofillServerURL));
    if (url.is_valid())
      return url;

    LOG(ERROR) << "Invalid URL value for --" << switches::kAutofillServerURL
               << ": "
               << command_line.GetSwitchValueASCII(
                      switches::kAutofillServerURL);
  }

  // If communication is disabled, leave the autofill server URL unset.
  if (!base::FeatureList::IsEnabled(features::kAutofillServerCommunication))
    return GURL();

  // Server communication is enabled. If there's an autofill server url param
  // use it, otherwise use the default.
  const std::string autofill_server_url_str =
      base::FeatureParam<std::string>(&features::kAutofillServerCommunication,
                                      switches::kAutofillServerURL,
                                      kDefaultAutofillServerURL)
          .Get();

  GURL autofill_server_url(autofill_server_url_str);

  if (!autofill_server_url.is_valid()) {
    LOG(ERROR) << "Invalid URL param for "
               << features::kAutofillServerCommunication.name << "/"
               << switches::kAutofillServerURL << ": "
               << autofill_server_url_str;
    return GURL();
  }

  return autofill_server_url;
}

base::TimeDelta GetThrottleResetPeriod() {
  return base::TimeDelta::FromDays(
      std::max(1, kAutofillUploadThrottlingPeriodInDays.Get()));
}

// Returns true if |id| is within |kAutofillExperimentRanges|.
bool IsAutofillExperimentId(int id) {
  for (const auto& range : kAutofillExperimentRanges) {
    if (range.first <= id && id <= range.second)
      return true;
  }
  return false;
}

// Helper to log the HTTP |response_code| and other data received for
// |request_type| to UMA.
void LogHttpResponseData(AutofillDownloadManager::RequestType request_type,
                         int response_code,
                         int net_error,
                         base::TimeDelta request_duration) {
  int response_or_error_code = net_error;
  if (net_error == net::OK || net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE)
    response_or_error_code = response_code;

  switch (request_type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      base::UmaHistogramSparse("Autofill.Query.HttpResponseOrErrorCode",
                               response_or_error_code);
      UMA_HISTOGRAM_TIMES("Autofill.Query.RequestDuration", request_duration);
      break;
    case AutofillDownloadManager::REQUEST_UPLOAD:
      base::UmaHistogramSparse("Autofill.Upload.HttpResponseOrErrorCode",
                               response_or_error_code);
      UMA_HISTOGRAM_TIMES("Autofill.Upload.RequestDuration", request_duration);
      break;
    default:
      NOTREACHED();
  }
}

// Helper to log, to UMA, the |num_bytes| sent for a failing instance of
// |request_type|.
void LogFailingPayloadSize(AutofillDownloadManager::RequestType request_type,
                           size_t num_bytes) {
  switch (request_type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      UMA_HISTOGRAM_COUNTS_100000("Autofill.Query.FailingPayloadSize",
                                  num_bytes);
      break;
    case AutofillDownloadManager::REQUEST_UPLOAD:
      UMA_HISTOGRAM_COUNTS_100000("Autofill.Upload.FailingPayloadSize",
                                  num_bytes);
      break;
    default:
      NOTREACHED();
  }
}

// Helper to log, to UMA, the |delay| caused by exponential backoff.
void LogExponentialBackoffDelay(
    AutofillDownloadManager::RequestType request_type,
    base::TimeDelta delay) {
  switch (request_type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      UMA_HISTOGRAM_MEDIUM_TIMES("Autofill.Query.BackoffDelay", delay);
      break;
    case AutofillDownloadManager::REQUEST_UPLOAD:
      UMA_HISTOGRAM_MEDIUM_TIMES("Autofill.Upload.BackoffDelay", delay);
      break;
    default:
      NOTREACHED();
  }
}

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    const autofill::AutofillDownloadManager::RequestType& request_type) {
  if (request_type == autofill::AutofillDownloadManager::REQUEST_QUERY) {
    return net::DefineNetworkTrafficAnnotation("autofill_query", R"(
        semantics {
          sender: "Autofill"
          description:
            "Chromium can automatically fill in web forms. If the feature is "
            "enabled, Chromium will send a non-identifying description of the "
            "form to Google's servers, which will respond with the type of "
            "data required by each of the form's fields, if known. I.e., if a "
            "field expects to receive a name, phone number, street address, "
            "etc."
          trigger: "User encounters a web form."
          data:
            "Hashed descriptions of the form and its fields. User data is not "
            "sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via 'Enable autofill to "
            "fill out web forms in a single click.' in Chromium's settings "
            "under 'Passwords and forms'. The feature is enabled by default."
          chrome_policy {
            AutoFillEnabled {
                policy_options {mode: MANDATORY}
                AutoFillEnabled: false
            }
          }
        })");
  }

  DCHECK_EQ(request_type, autofill::AutofillDownloadManager::REQUEST_UPLOAD);
  return net::DefineNetworkTrafficAnnotation("autofill_upload", R"(
      semantics {
        sender: "Autofill"
        description:
          "Chromium relies on crowd-sourced field type classifications to "
          "help it automatically fill in web forms. If the feature is "
          "enabled, Chromium will send a non-identifying description of the "
          "form to Google's servers along with the type of data Chromium "
          "observed being given to the form. I.e., if you entered your first "
          "name into a form field, Chromium will 'vote' for that form field "
          "being a first name field."
        trigger: "User submits a web form."
        data:
          "Hashed descriptions of the form and its fields along with type of "
          "data given to each field, if recognized from the user's "
          "profile(s). User data is not sent."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can enable or disable this feature via 'Enable autofill to "
          "fill out web forms in a single click.' in Chromium's settings "
          "under 'Passwords and forms'. The feature is enabled by default."
        chrome_policy {
          AutoFillEnabled {
              policy_options {mode: MANDATORY}
              AutoFillEnabled: false
          }
        }
      })");
}

size_t CountActiveFieldsInForms(const std::vector<FormStructure*>& forms) {
  size_t active_field_count = 0;
  for (const auto* form : forms)
    active_field_count += form->active_field_count();
  return active_field_count;
}

const char* RequestTypeToString(AutofillDownloadManager::RequestType type) {
  switch (type) {
    case AutofillDownloadManager::REQUEST_QUERY:
      return "query";
    case AutofillDownloadManager::REQUEST_UPLOAD:
      return "upload";
  }
  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryContents& query) {
  out << "client_version: " << query.client_version();
  for (const auto& form : query.form()) {
    out << "\nForm\n signature: " << form.signature();
    for (const auto& field : form.field()) {
      out << "\n Field\n  signature: " << field.signature();
      if (!field.name().empty())
        out << "\n  name: " << field.name();
      if (!field.type().empty())
        out << "\n  type: " << field.type();
    }
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillUploadContents& upload) {
  out << "client_version: " << upload.client_version() << "\n";
  out << "form_signature: " << upload.form_signature() << "\n";
  out << "data_present: " << upload.data_present() << "\n";
  out << "submission: " << upload.submission() << "\n";
  if (upload.action_signature())
    out << "action_signature: " << upload.action_signature() << "\n";
  if (upload.login_form_signature())
    out << "login_form_signature: " << upload.login_form_signature() << "\n";
  if (!upload.form_name().empty())
    out << "form_name: " << upload.form_name() << "\n";

  for (const auto& field : upload.field()) {
    out << "\n Field"
        << "\n signature: " << field.signature() << "\n autofill_type: [";
    for (int i = 0; i < field.autofill_type_size(); ++i) {
      if (i)
        out << ", ";
      out << field.autofill_type(i);
    }
    out << "]";

    out << "\n (autofill_type, validity_states): [";
    for (const auto& type_validities : field.autofill_type_validities()) {
      out << "(type: " << type_validities.type() << ", validities: {";
      for (int i = 0; i < type_validities.validity_size(); ++i) {
        if (i)
          out << ", ";
        out << type_validities.validity(i);
      }
      out << "})";
    }
    out << "]\n";
    if (!field.name().empty())
      out << "\n name: " << field.name();
    if (!field.autocomplete().empty())
      out << "\n autocomplete: " << field.autocomplete();
    if (!field.type().empty())
      out << "\n type: " << field.type();
    if (field.generation_type())
      out << "\n generation_type: " << field.generation_type();
  }
  return out;
}

std::string FieldTypeToString(int type) {
  return base::StrCat(
      {base::NumberToString(type), std::string("/"),
       AutofillType(static_cast<ServerFieldType>(type)).ToString()});
}

LogBuffer& operator<<(LogBuffer& out,
                      const autofill::AutofillUploadContents& upload) {
  if (!out.active())
    return out;
  out << Tag{"div"} << Attrib{"class", "form"};
  out << Tag{"table"};
  out << Tr{} << "client_version:" << upload.client_version();
  out << Tr{} << "data_present:" << upload.data_present();
  out << Tr{} << "autofill_used:" << upload.autofill_used();
  out << Tr{} << "submission:" << upload.submission();
  if (upload.has_submission_event()) {
    out << Tr{}
        << "submission_event:" << static_cast<int>(upload.submission_event());
  }
  if (upload.action_signature())
    out << Tr{} << "action_signature:" << upload.action_signature();
  if (upload.login_form_signature())
    out << Tr{} << "login_form_signature:" << upload.login_form_signature();
  if (!upload.form_name().empty())
    out << Tr{} << "form_name:" << upload.form_name();
  if (upload.has_passwords_revealed())
    out << Tr{} << "passwords_revealed:" << upload.passwords_revealed();
  if (upload.has_has_form_tag())
    out << Tr{} << "has_form_tag:" << upload.has_form_tag();

  out << Tr{} << "form_signature:" << upload.form_signature();
  for (const auto& field : upload.field()) {
    out << Tr{} << Attrib{"style", "font-weight: bold"}
        << "field_signature:" << field.signature();

    std::vector<std::string> types_as_strings;
    types_as_strings.reserve(field.autofill_type_size());
    for (int type : field.autofill_type())
      types_as_strings.emplace_back(FieldTypeToString(type));
    out << Tr{} << "autofill_type:" << types_as_strings;

    LogBuffer validities;
    validities << Tag{"span"} << "[";
    for (const auto& type_validities : field.autofill_type_validities()) {
      validities << "(type: " << type_validities.type()
                 << ", validities: " << type_validities.validity() << ")";
    }
    validities << "]";
    out << Tr{} << "validity_states" << std::move(validities);

    if (!field.name().empty())
      out << Tr{} << "name:" << field.name();
    if (!field.autocomplete().empty())
      out << Tr{} << "autocomplete:" << field.autocomplete();
    if (!field.type().empty())
      out << Tr{} << "type:" << field.type();
    if (field.generation_type()) {
      out << Tr{}
          << "generation_type:" << static_cast<int>(field.generation_type());
    }
  }
  out << CTag{"table"};
  out << CTag{"div"};
  return out;
}

// Returns true if an upload of |form| triggered by |form.submission_source()|
// can be throttled/suppressed. This is true if |prefs| indicates that this
// upload has already happened within the last update window. Updates |prefs|
// account for the upload for |form|.
bool CanThrottleUpload(const FormStructure& form,
                       base::TimeDelta throttle_reset_period,
                       PrefService* pref_service) {
  // PasswordManager uploads are triggered via specific first occurrences and
  // do not participate in the pref-service tracked throttling mechanism. Return
  // false for these uploads.
  if (!pref_service)
    return false;

  // If the upload event pref needs to be reset, clear it now.
  base::Time now = AutofillClock::Now();
  base::Time last_reset =
      pref_service->GetTime(prefs::kAutofillUploadEventsLastResetTimestamp);
  if ((now - last_reset) > throttle_reset_period) {
    AutofillDownloadManager::ClearUploadHistory(pref_service);
  }

  // Get the key for the upload bucket and extract the current bitfield value.
  static constexpr size_t kNumUploadBuckets = 1021;
  std::string key = base::StringPrintf(
      "%03X",
      static_cast<int>(form.form_signature().value() % kNumUploadBuckets));
  auto* upload_events =
      pref_service->GetDictionary(prefs::kAutofillUploadEvents);
  auto* found = upload_events->FindKeyOfType(key, base::Value::Type::INTEGER);
  int value = found ? found->GetInt() : 0;

  // Calculate the mask we expect to be set for the form's upload bucket.
  const int bit = static_cast<int>(form.submission_source());
  DCHECK_LE(0, bit);
  DCHECK_LT(bit, 32);
  const int mask = (1 << bit);

  // Check if this is the first upload for this event. If so, update the upload
  // event pref to set the appropriate bit.
  bool is_first_upload_for_event = ((value & mask) == 0);
  if (is_first_upload_for_event) {
    DictionaryPrefUpdate update(pref_service, prefs::kAutofillUploadEvents);
    update->SetKey(std::move(key), base::Value(value | mask));
  }

  return !is_first_upload_for_event;
}

// Determines whether to use the API instead of the legacy server.
inline bool UseApi() {
  return base::FeatureList::IsEnabled(features::kAutofillUseApi);
}

// Determines whether a HTTP request was successful based on its response code.
bool IsHttpSuccess(int response_code) {
  return (response_code >= 200 && response_code < 300);
}

// Gets an upload payload for requests to the legacy server.
inline bool GetUploadPayloadForLegacy(const AutofillUploadContents& upload,
                                      std::string* payload) {
  return upload.SerializeToString(payload);
}

bool GetUploadPayloadForApi(const AutofillUploadContents& upload,
                            std::string* payload) {
  AutofillUploadRequest upload_request;
  *upload_request.mutable_upload() = upload;
  return upload_request.SerializeToString(payload);
}

// Gets an API method URL given its type (query or upload), an optional
// resource ID, and the HTTP method to be used.
// Example usage:
// * GetAPIMethodUrl(REQUEST_QUERY, "1234", "GET") will return "/v1/pages/1234".
// * GetAPIMethodUrl(REQUEST_QUERY, "1234", "POST") will return "/v1/pages:get".
// * GetAPIMethodUrl(REQUEST_UPLOAD, "", "POST") will return "/v1/forms:vote".
std::string GetAPIMethodUrl(AutofillDownloadManager::RequestType type,
                            base::StringPiece resource_id,
                            base::StringPiece method) {
  const char* api_method_url;
  if (type == AutofillDownloadManager::REQUEST_QUERY) {
    if (method == "POST") {
      api_method_url = "/v1/pages:get";
    } else {
      api_method_url = "/v1/pages";
    }
  } else if (type == AutofillDownloadManager::REQUEST_UPLOAD) {
    api_method_url = "/v1/forms:vote";
  } else {
    // This should not be reached, but we never know.
    NOTREACHED() << "Request of type " << type << " is invalid";
    return "";
  }
  if (resource_id.empty()) {
    return std::string(api_method_url);
  }
  return base::StrCat({api_method_url, "/", resource_id});
}

// Gets HTTP body payload for API POST request.
bool GetAPIBodyPayload(const std::string& payload,
                       AutofillDownloadManager::RequestType type,
                       std::string* output_payload) {
  // Don't do anything for payloads not related to Query.
  if (type != AutofillDownloadManager::REQUEST_QUERY) {
    *output_payload = payload;
    return true;
  }
  // Wrap query payload in a request proto to interface with API Query method.
  AutofillPageResourceQueryRequest request;
  request.set_serialized_request(payload);
  if (!request.SerializeToString(output_payload)) {
    return false;
  }
  return true;
}

// Gets the data payload for API Query (POST and GET).
bool GetAPIQueryPayload(const AutofillQueryContents& query,
                        std::string* payload) {
  std::string serialized_query;
  if (!CreateApiRequestFromLegacyRequest(query).SerializeToString(
          &serialized_query)) {
    return false;
  }
  base::Base64UrlEncode(serialized_query,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING, payload);
  return true;
}

}  // namespace

struct AutofillDownloadManager::FormRequestData {
  std::vector<std::string> form_signatures;
  RequestType request_type;
  std::string payload;
  int num_attempts = 0;
};

ScopedActiveAutofillExperiments::ScopedActiveAutofillExperiments() {
  AutofillDownloadManager::ResetActiveExperiments();
}

ScopedActiveAutofillExperiments::~ScopedActiveAutofillExperiments() {
  AutofillDownloadManager::ResetActiveExperiments();
}

std::vector<variations::VariationID>*
    AutofillDownloadManager::active_experiments_ = nullptr;

AutofillDownloadManager::AutofillDownloadManager(AutofillDriver* driver,
                                                 Observer* observer,
                                                 const std::string& api_key,
                                                 LogManager* log_manager)
    : driver_(driver),
      observer_(observer),
      api_key_(api_key),
      log_manager_(log_manager),
      autofill_server_url_(GetAutofillServerURL()),
      throttle_reset_period_(GetThrottleResetPeriod()),
      max_form_cache_size_(kAutofillDownloadManagerMaxFormCacheSize),
      loader_backoff_(&kAutofillBackoffPolicy) {
  DCHECK(observer_);
}

AutofillDownloadManager::AutofillDownloadManager(AutofillDriver* driver,
                                                 Observer* observer)
    : AutofillDownloadManager(driver,
                              observer,
                              kDefaultAPIKey,
                              /*log_manager=*/nullptr) {}

AutofillDownloadManager::~AutofillDownloadManager() = default;

bool AutofillDownloadManager::StartQueryRequest(
    const std::vector<FormStructure*>& forms) {
  if (!IsEnabled())
    return false;

  // Do not send the request if it contains more fields than the server can
  // accept.
  if (CountActiveFieldsInForms(forms) > kMaxFieldsPerQueryRequest)
    return false;

  // Encode the query for the requested forms.
  AutofillQueryContents query;
  FormRequestData request_data;
  if (!FormStructure::EncodeQueryRequest(forms, &request_data.form_signatures,
                                         &query)) {
    return false;
  }

  // The set of active autofill experiments is constant for the life of the
  // process. We initialize and statically cache it on first use. Leaked on
  // process termination.
  if (active_experiments_ == nullptr)
    InitActiveExperiments();

  // Attach any active autofill experiments.
  query.mutable_experiments()->Reserve(active_experiments_->size());
  for (int id : *active_experiments_)
    query.mutable_experiments()->Add(id);

  // Get the query request payload.
  std::string payload;
  bool is_payload_serialized = UseApi() ? GetAPIQueryPayload(query, &payload)
                                        : query.SerializeToString(&payload);
  if (!is_payload_serialized) {
    return false;
  }

  request_data.request_type = AutofillDownloadManager::REQUEST_QUERY;
  request_data.payload = std::move(payload);
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_SENT);

  std::string query_data;
  if (CheckCacheForQueryRequest(request_data.form_signatures, &query_data)) {
    DVLOG(1) << "AutofillDownloadManager: query request has been retrieved "
             << "from the cache, form signatures: "
             << GetCombinedSignature(request_data.form_signatures);
    observer_->OnLoadedServerPredictions(std::move(query_data),
                                         request_data.form_signatures);
    return true;
  }

  DVLOG(1) << "Sending Autofill Query Request:\n" << query;

  return StartRequest(std::move(request_data));
}

bool AutofillDownloadManager::StartUploadRequest(
    const FormStructure& form,
    bool form_was_autofilled,
    const ServerFieldTypeSet& available_field_types,
    const std::string& login_form_signature,
    bool observed_submission,
    PrefService* prefs) {
  if (!IsEnabled())
    return false;

  bool can_throttle_upload =
      CanThrottleUpload(form, throttle_reset_period_, prefs);
  bool throttling_is_enabled =
      base::FeatureList::IsEnabled(features::kAutofillUploadThrottling);
  bool is_small_form = form.active_field_count() < 3;
  bool allow_upload =
      !(can_throttle_upload && (throttling_is_enabled || is_small_form));
  AutofillMetrics::LogUploadEvent(form.submission_source(), allow_upload);

  // For debugging purposes, even throttled uploads are logged. If no log
  // manager is active, the function can exit early for throttled uploads.
  bool needs_logging = log_manager_ && log_manager_->IsLoggingActive();
  if (!needs_logging && !allow_upload)
    return false;

  AutofillUploadContents upload;
  if (!form.EncodeUploadRequest(available_field_types, form_was_autofilled,
                                login_form_signature, observed_submission,
                                &upload)) {
    return false;
  }

  // If this upload was a candidate for throttling, tag it and make sure that
  // any throttling sensitive features are enforced.
  if (can_throttle_upload) {
    upload.set_was_throttleable(true);

    // Don't send randomized metadata.
    upload.clear_randomized_form_metadata();
    for (auto& f : *upload.mutable_field()) {
      f.clear_randomized_field_metadata();
    }
  }

  // Get the POST payload that contains upload data.
  std::string payload;
  bool is_payload = UseApi() ? GetUploadPayloadForApi(upload, &payload)
                             : GetUploadPayloadForLegacy(upload, &payload);
  // Indicate that we could not serialize upload in the payload.
  if (!is_payload) {
    return false;
  }

  if (form.upload_required() == UPLOAD_NOT_REQUIRED) {
    // If we ever need notification that upload was skipped, add it here.
    return false;
  }

  FormRequestData request_data;
  request_data.form_signatures.push_back(form.FormSignatureAsStr());
  request_data.request_type = AutofillDownloadManager::REQUEST_UPLOAD;
  request_data.payload = std::move(payload);

  DVLOG(1) << "Sending Autofill Upload Request:\n" << upload;
  if (log_manager_) {
    log_manager_->Log() << LoggingScope::kAutofillServer
                        << LogMessage::kSendAutofillUpload << Br{}
                        << "Allow upload?: " << allow_upload << Br{}
                        << "Data: " << Br{} << upload;
  }

  if (!allow_upload)
    return false;

  return StartRequest(std::move(request_data));
}

void AutofillDownloadManager::ClearUploadHistory(PrefService* pref_service) {
  if (pref_service) {
    pref_service->ClearPref(prefs::kAutofillUploadEvents);
    pref_service->SetTime(prefs::kAutofillUploadEventsLastResetTimestamp,
                          AutofillClock::Now());
  }
}

size_t AutofillDownloadManager::GetPayloadLength(
    base::StringPiece payload) const {
  return payload.length();
}

std::tuple<GURL, std::string> AutofillDownloadManager::GetRequestURLAndMethod(
    const FormRequestData& request_data) const {
  std::string method("POST");
  std::string query_str;

  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY) {
    if (request_data.payload.length() <= kMaxQueryGetSize &&
        base::FeatureList::IsEnabled(features::kAutofillCacheQueryResponses)) {
      method = "GET";
      std::string base64_payload;
      base::Base64UrlEncode(request_data.payload,
                            base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                            &base64_payload);
      base::StrAppend(&query_str, {"q=", base64_payload});
    }
    UMA_HISTOGRAM_BOOLEAN("Autofill.Query.Method", (method == "GET") ? 0 : 1);
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query_str);

  GURL url = autofill_server_url_
                 .Resolve(RequestTypeToString(request_data.request_type))
                 .ReplaceComponents(replacements);
  return std::make_tuple(std::move(url), std::move(method));
}

std::tuple<GURL, std::string>
AutofillDownloadManager::GetRequestURLAndMethodForApi(
    const FormRequestData& request_data) const {
  // ID of the resource to add to the API request URL. Nothing will be added if
  // |resource_id| is empty.
  std::string resource_id;
  std::string method = "POST";

  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY) {
    if (GetPayloadLength(request_data.payload) <= kMaxAPIQueryGetSize &&
        base::FeatureList::IsEnabled(features::kAutofillCacheQueryResponses)) {
      resource_id = request_data.payload;
      method = "GET";
      UMA_HISTOGRAM_BOOLEAN("Autofill.Query.ApiUrlIsTooLong", false);
    } else {
      UMA_HISTOGRAM_BOOLEAN("Autofill.Query.ApiUrlIsTooLong", true);
    }
    UMA_HISTOGRAM_BOOLEAN("Autofill.Query.Method", (method == "GET") ? 0 : 1);
  }

  // Make the canonical URL to query the API, e.g.,
  // https://autofill.googleapis.com/v1/forms/1234?alt=proto.
  GURL url = autofill_server_url_.Resolve(
      GetAPIMethodUrl(request_data.request_type, resource_id, method));

  // Add the query parameter to set the response format to a serialized proto.
  url = net::AppendQueryParameter(url, "alt", "proto");

  return std::make_tuple(std::move(url), std::move(method));
}

bool AutofillDownloadManager::StartRequest(FormRequestData request_data) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      driver_->GetURLLoaderFactory();
  DCHECK(url_loader_factory);

  // Get the URL and method to use for this request.
  std::string method;
  GURL request_url;
  std::tie(request_url, method) =
      UseApi() ? GetRequestURLAndMethodForApi(request_data)
               : GetRequestURLAndMethod(request_data);

  // Track the URL length for GET queries because the URL length can be in the
  // thousands when rich metadata is enabled.
  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY &&
      method == "GET") {
    UMA_HISTOGRAM_COUNTS_100000("Autofill.Query.GetUrlLength",
                                request_url.spec().length());
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = method;

  // On iOS we have a single, shared URLLoaderFactory provided by BrowserState.
  // As it is shared, it is not trusted and we cannot assign trusted_params
  // to the network request.
#if !defined(OS_IOS)
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = driver_->IsolationInfo();
#endif

  // Add Chrome experiment state to the request headers.
  variations::AppendVariationsHeaderUnknownSignedIn(
      request_url,
      driver_->IsIncognito() ? variations::InIncognito::kYes
                             : variations::InIncognito::kNo,
      resource_request.get());

  // Set headers specific to the API if using it.
  if (UseApi())
    // Encode response serialized proto in base64 for safety.
    resource_request->headers.SetHeader(kGoogEncodeResponseIfExecutable,
                                        "base64");

  // Put API key in request's header if a key exists, and the endpoint is
  // trusted by Google.
  if (!api_key_.empty() && request_url.SchemeIs(url::kHttpsScheme) &&
      google_util::IsGoogleAssociatedDomainUrl(request_url)) {
    resource_request->headers.SetHeader(kGoogApiKey, api_key_);
  }

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      GetNetworkTrafficAnnotation(request_data.request_type));

  // This allows reading the error message within the API response when status
  // is not 200 (e.g., 400). Otherwise, URL loader will not give any content in
  // the response when there is a failure, which makes debugging hard.
  simple_loader->SetAllowHttpErrorResults(true);

  if (method == "POST") {
    const std::string content_type =
        UseApi() ? "application/x-protobuf" : "text/proto";
    std::string payload;
    if (UseApi()) {
      if (!GetAPIBodyPayload(request_data.payload, request_data.request_type,
                             &payload)) {
        return false;
      }
    } else {
      payload = request_data.payload;
    }
    // Attach payload data and add data format header.
    simple_loader->AttachStringForUpload(payload, content_type);
  }

  // Transfer ownership of the loader into url_loaders_. Temporarily hang
  // onto the raw pointer to use it as a key and to kick off the request;
  // transferring ownership (std::move) invalidates the |simple_loader|
  // variable.
  auto* raw_simple_loader = simple_loader.get();
  url_loaders_.push_back(std::move(simple_loader));
  raw_simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&AutofillDownloadManager::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(--url_loaders_.end()),
                     std::move(request_data), AutofillTickClock::NowTicks()));
  return true;
}

void AutofillDownloadManager::CacheQueryRequest(
    const std::vector<std::string>& forms_in_query,
    const std::string& query_data) {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (auto it = cached_forms_.begin(); it != cached_forms_.end(); ++it) {
    if (it->first == signature) {
      // We hit the cache, move to the first position and return.
      std::pair<std::string, std::string> data = *it;
      cached_forms_.erase(it);
      cached_forms_.push_front(data);
      return;
    }
  }
  std::pair<std::string, std::string> data;
  data.first = signature;
  data.second = query_data;
  cached_forms_.push_front(data);
  while (cached_forms_.size() > max_form_cache_size_)
    cached_forms_.pop_back();
}

bool AutofillDownloadManager::CheckCacheForQueryRequest(
    const std::vector<std::string>& forms_in_query,
    std::string* query_data) const {
  std::string signature = GetCombinedSignature(forms_in_query);
  for (const auto& it : cached_forms_) {
    if (it.first == signature) {
      // We hit the cache, fill the data and return.
      *query_data = it.second;
      return true;
    }
  }
  return false;
}

std::string AutofillDownloadManager::GetCombinedSignature(
    const std::vector<std::string>& forms_in_query) const {
  size_t total_size = forms_in_query.size();
  for (size_t i = 0; i < forms_in_query.size(); ++i)
    total_size += forms_in_query[i].length();
  std::string signature;

  signature.reserve(total_size);

  for (size_t i = 0; i < forms_in_query.size(); ++i) {
    if (i)
      signature.append(",");
    signature.append(forms_in_query[i]);
  }
  return signature;
}

// static
int AutofillDownloadManager::GetMaxServerAttempts() {
  // This value is constant for the life of the browser, so we cache it
  // statically on first use to avoid re-parsing the param on each retry
  // opportunity.
  static const int max_attempts =
      base::ClampToRange(kAutofillMaxServerAttempts.Get(), 1, 20);
  return max_attempts;
}

void AutofillDownloadManager::OnSimpleLoaderComplete(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    FormRequestData request_data,
    base::TimeTicks request_start,
    std::unique_ptr<std::string> response_body) {
  // Move the loader out of the active loaders list.
  std::unique_ptr<network::SimpleURLLoader> simple_loader = std::move(*it);
  url_loaders_.erase(it);

  CHECK(request_data.form_signatures.size());
  int response_code = -1;  // Invalid response code.
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers) {
    response_code = simple_loader->ResponseInfo()->headers->response_code();
  }

  // We define success as getting 2XX response code and having a response body.
  // Even if the server does not fill the response body when responding, the
  // corresponding response string will be at least instantiated and empty.
  // Having the response body a nullptr probably reflects a problem.
  const bool success =
      IsHttpSuccess(response_code) && (response_body != nullptr);
  loader_backoff_.InformOfRequest(success);

  LogHttpResponseData(request_data.request_type, response_code,
                      simple_loader->NetError(),
                      AutofillTickClock::NowTicks() - request_start);

  // Handle error if there is and return.
  if (!success) {
    std::string error_message =
        (response_body != nullptr) ? *response_body : "";
    DVLOG(1) << "AutofillDownloadManager: "
             << RequestTypeToString(request_data.request_type)
             << " request has failed with net error "
             << simple_loader->NetError() << " and HTTP response code "
             << response_code << " and error message from the server "
             << error_message;

    observer_->OnServerRequestError(request_data.form_signatures[0],
                                    request_data.request_type, response_code);

    LogFailingPayloadSize(request_data.request_type,
                          request_data.payload.length());

    // If the failure was a client error don't retry.
    if (response_code >= 400 && response_code <= 499)
      return;

    // If we've exhausted the maximum number of attempts, don't retry.
    if (++request_data.num_attempts >= GetMaxServerAttempts())
      return;

    base::TimeDelta backoff = loader_backoff_.GetTimeUntilRelease();
    LogExponentialBackoffDelay(request_data.request_type, backoff);

    // Reschedule with the appropriate delay, ignoring return value because
    // payload is already well formed.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&AutofillDownloadManager::StartRequest),
            weak_factory_.GetWeakPtr(), std::move(request_data)),
        backoff);
    return;
  }

  if (request_data.request_type == AutofillDownloadManager::REQUEST_QUERY) {
    CacheQueryRequest(request_data.form_signatures, *response_body);
    UMA_HISTOGRAM_BOOLEAN("Autofill.Query.WasInCache",
                          simple_loader->LoadedFromCache());
    observer_->OnLoadedServerPredictions(std::move(*response_body),
                                         request_data.form_signatures);
    return;
  }

  DCHECK_EQ(request_data.request_type, AutofillDownloadManager::REQUEST_UPLOAD);
  DVLOG(1) << "AutofillDownloadManager: upload request has succeeded.";

  observer_->OnUploadedPossibleFieldTypes();
}

void AutofillDownloadManager::InitActiveExperiments() {
  auto* variations_http_header_provider =
      variations::VariationsHttpHeaderProvider::GetInstance();
  DCHECK(variations_http_header_provider != nullptr);

  delete active_experiments_;
  active_experiments_ = new std::vector<variations::VariationID>(
      variations_http_header_provider->GetVariationsVector(
          variations::GOOGLE_WEB_PROPERTIES_TRIGGER));
  base::EraseIf(*active_experiments_, [](variations::VariationID id) {
    return !IsAutofillExperimentId(id);
  });
  std::sort(active_experiments_->begin(), active_experiments_->end());
}

// static
void AutofillDownloadManager::ResetActiveExperiments() {
  delete active_experiments_;
  active_experiments_ = nullptr;
}

}  // namespace autofill
