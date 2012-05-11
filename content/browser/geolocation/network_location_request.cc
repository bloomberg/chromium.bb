// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/network_location_request.h"

#include <set>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/common/net/url_fetcher_impl.h"
#include "content/public/common/geoposition.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace {

const size_t kMaxRequestLength = 2048;

const char kAccessTokenString[] = "access_token";
const char kLocationString[] = "location";
const char kLatitudeString[] = "lat";
const char kLongitudeString[] = "lng";
const char kAccuracyString[] = "accuracy";
const char kStatusString[] = "status";
const char kStatusOKString[] = "OK";

// Local functions
// Creates the request url to send to the server.
GURL FormRequestURL(const std::string& url,
                    const string16& access_token,
                    const WifiData& wifi_data,
                    const base::Time& timestamp);

// Parsers the server response.
void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             const base::Time& timestamp,
                             const GURL& server_url,
                             content::Geoposition* position,
                             string16* access_token);

// Parses the server response body. Returns true if parsing was successful.
// Sets |*position| to the parsed location if a valid fix was received,
// otherwise leaves it unchanged.
bool ParseServerResponse(const std::string& response_body,
                         const base::Time& timestamp,
                         content::Geoposition* position,
                         string16* access_token);
void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 std::vector<std::string>* params);
}  // namespace

int NetworkLocationRequest::url_fetcher_id_for_tests = 0;

NetworkLocationRequest::NetworkLocationRequest(
    net::URLRequestContextGetter* context,
    const GURL& url,
    ListenerInterface* listener)
        : url_context_(context), listener_(listener),
          url_(url) {
  DCHECK(listener);
}

NetworkLocationRequest::~NetworkLocationRequest() {
}

bool NetworkLocationRequest::MakeRequest(const string16& access_token,
                                         const RadioData& radio_data,
                                         const WifiData& wifi_data,
                                         const base::Time& timestamp) {
  if (url_fetcher_ != NULL) {
    DVLOG(1) << "NetworkLocationRequest : Cancelling pending request";
    url_fetcher_.reset();
  }
  radio_data_ = radio_data;
  wifi_data_ = wifi_data;
  timestamp_ = timestamp;

  GURL request_url = FormRequestURL(url_.spec(), access_token,
                                    wifi_data, timestamp_);
  url_fetcher_.reset(URLFetcherImpl::Create(
      url_fetcher_id_for_tests, request_url, URLFetcherImpl::GET, this));
  url_fetcher_->SetRequestContext(url_context_);
  url_fetcher_->SetLoadFlags(
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);

  url_fetcher_->Start();
  return true;
}

void NetworkLocationRequest::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);

  net::URLRequestStatus status = source->GetStatus();
  int response_code = source->GetResponseCode();

  content::Geoposition position;
  string16 access_token;
  std::string data;
  source->GetResponseAsString(&data);
  GetLocationFromResponse(status.is_success(),
                          response_code,
                          data,
                          timestamp_,
                          source->GetURL(),
                          &position,
                          &access_token);
  const bool server_error =
      !status.is_success() || (response_code >= 500 && response_code < 600);
  url_fetcher_.reset();

  DCHECK(listener_);
  DVLOG(1) << "NetworkLocationRequest::Run() : Calling listener with position.";
  listener_->LocationResponseAvailable(position, server_error, access_token,
                                       radio_data_, wifi_data_);
}

// Local functions.
namespace {

struct AccessPointLess {
  bool operator()(const AccessPointData* ap1,
                  const AccessPointData* ap2) const {
    return ap2->radio_signal_strength < ap1->radio_signal_strength;
  }
};

GURL FormRequestURL(const std::string& url,
                    const string16& access_token,
                    const WifiData& wifi_data,
                    const base::Time& timestamp) {
  int age = kint32min;  // Invalid so AddInteger() will ignore.
  if (!timestamp.is_null()) {
    // Convert absolute timestamps into a relative age.
    int64 delta_ms = (base::Time::Now() - timestamp).InMilliseconds();
    if (delta_ms >= 0 && delta_ms < kint32max)
      age = static_cast<int>(delta_ms);
  }

  std::vector<std::string> params;
#if defined(GOOGLE_CHROME_BUILD)
  params.push_back("browser=googlechrome");
#else
  params.push_back("browser=chromium");
#endif

  params.push_back("sensor=true");
  if (!access_token.empty())
    params.push_back("token=" + UTF16ToUTF8(access_token));
  AddWifiData(wifi_data, age, &params);

  std::string request_string = url + '?' + JoinString(params, '&');
  if (request_string.length() > kMaxRequestLength) {
    size_t last_param_pos =
        request_string.find_last_of('&', kMaxRequestLength);
    CHECK_NE(std::string::npos, last_param_pos);
    request_string.erase(last_param_pos);
  }

  return GURL(request_string);
}

void AddString(const std::string& property_name, const std::string& value,
               std::string* wifi_params) {
  DCHECK(wifi_params);
  if (!value.empty()) {
    if (!wifi_params->empty())
      *wifi_params += '|';
    *wifi_params += property_name;
    *wifi_params += value;
  }
}

void AddInteger(const std::string& property_name, int value,
                std::string* wifi_params) {
  DCHECK(wifi_params);
  if (value != kint32min) {
    if (!wifi_params->empty())
        *wifi_params += '|';
      *wifi_params += property_name;
      *wifi_params += base::IntToString(value);
  }
}

void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 std::vector<std::string>* params) {
  DCHECK(params);

  if (wifi_data.access_point_data.empty())
    return;

  typedef std::multiset<const AccessPointData*, AccessPointLess> AccessPointSet;
  AccessPointSet access_points_by_signal_strength;

  for (WifiData::AccessPointDataSet::const_iterator iter =
       wifi_data.access_point_data.begin();
       iter != wifi_data.access_point_data.end();
       ++iter) {
    access_points_by_signal_strength.insert(&(*iter));
  }

  for (AccessPointSet::iterator iter =
      access_points_by_signal_strength.begin();
      iter != access_points_by_signal_strength.end();
      ++iter) {
    std::string wifi_params;
    AddString("mac:", UTF16ToUTF8((*iter)->mac_address), &wifi_params);
    AddInteger("ss:", (*iter)->radio_signal_strength, &wifi_params);
    AddInteger("age:", age_milliseconds, &wifi_params);
    AddInteger("chan:", (*iter)->channel, &wifi_params);
    AddInteger("snr:", (*iter)->signal_to_noise, &wifi_params);
    std::string ssid = UTF16ToUTF8((*iter)->ssid);
    // Backslash characters in the ssid need backslash-escaping to avoid
    // escaping a following wifi parameter separator.
    ReplaceSubstringsAfterOffset(&ssid, 0, "\\", "\\\\");
    // Pipe characters in the ssid need backslash-escaping to avoid being
    // interpreted as the wifi parameter separator.
    ReplaceSubstringsAfterOffset(&ssid, 0, "|", "\\|");
    AddString("ssid:", ssid, &wifi_params);
    params->push_back(
        "wifi=" + net::EscapeQueryParamValue(wifi_params, false));
  }
}

void FormatPositionError(const GURL& server_url,
                         const std::string& message,
                         content::Geoposition* position) {
    position->error_code =
        content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = "Network location provider at '";
    position->error_message += server_url.possibly_invalid_spec();
    position->error_message += "' : ";
    position->error_message += message;
    position->error_message += ".";
    VLOG(1) << "NetworkLocationRequest::GetLocationFromResponse() : "
            << position->error_message;
}

void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             const base::Time& timestamp,
                             const GURL& server_url,
                             content::Geoposition* position,
                             string16* access_token) {
  DCHECK(position);
  DCHECK(access_token);

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (!http_post_result) {
    FormatPositionError(server_url, "No response received", position);
    return;
  }
  if (status_code != 200) {  // HTTP OK.
    std::string message = "Returned error code ";
    message += base::IntToString(status_code);
    FormatPositionError(server_url, message, position);
    return;
  }
  // We use the timestamp from the device data that was used to generate
  // this position fix.
  if (!ParseServerResponse(response_body, timestamp, position, access_token)) {
    // We failed to parse the repsonse.
    FormatPositionError(server_url, "Response was malformed", position);
    return;
  }
  // The response was successfully parsed, but it may not be a valid
  // position fix.
  if (!position->Validate()) {
    FormatPositionError(server_url,
                        "Did not provide a good position fix", position);
    return;
  }
}

// Numeric values without a decimal point have type integer and IsDouble() will
// return false. This is convenience function for detecting integer or floating
// point numeric values. Note that isIntegral() includes boolean values, which
// is not what we want.
bool GetAsDouble(const DictionaryValue& object,
                 const std::string& property_name,
                 double* out) {
  DCHECK(out);
  Value* value = NULL;
  if (!object.Get(property_name, &value))
    return false;
  int value_as_int;
  DCHECK(value);
  if (value->GetAsInteger(&value_as_int)) {
    *out = value_as_int;
    return true;
  }
  return value->GetAsDouble(out);
}

bool ParseServerResponse(const std::string& response_body,
                         const base::Time& timestamp,
                         content::Geoposition* position,
                         string16* access_token) {
  DCHECK(position);
  DCHECK(!position->Validate());
  DCHECK(position->error_code == content::Geoposition::ERROR_CODE_NONE);
  DCHECK(access_token);
  DCHECK(!timestamp.is_null());

  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse() : Response was empty.";
    return false;
  }
  DVLOG(1) << "ParseServerResponse() : Parsing response " << response_body;

  // Parse the response, ignoring comments.
  std::string error_msg;
  scoped_ptr<Value> response_value(base::JSONReader::ReadAndReturnError(
      response_body, base::JSON_PARSE_RFC, NULL, &error_msg));
  if (response_value == NULL) {
    LOG(WARNING) << "ParseServerResponse() : JSONReader failed : "
                 << error_msg;
    return false;
  }

  if (!response_value->IsType(Value::TYPE_DICTIONARY)) {
    VLOG(1) << "ParseServerResponse() : Unexpected response type "
            << response_value->GetType();
    return false;
  }
  const DictionaryValue* response_object =
      static_cast<DictionaryValue*>(response_value.get());

  // Check the status code.
  Value* status_value = NULL;
  if (!response_object->Get(kStatusString, &status_value)) {
    VLOG(1) << "ParseServerResponse() : Missing status attribute.";
    // The status attribute is required.
    return false;
  }
  DCHECK(status_value);

  if (!status_value->IsType(Value::TYPE_STRING)) {
    VLOG(1) << "ParseServerResponse() : Unexpected status type "
            << status_value->GetType();
    // The status attribute is required to be a string.
    return false;
  }
  StringValue* status_object = static_cast<StringValue*>(status_value);

  std::string status;
  if (!status_object->GetAsString(&status)) {
    VLOG(1) << "ParseServerResponse() : Error parsing the status value.";
    return false;
  }

  if (status != kStatusOKString) {
    VLOG(1) << "ParseServerResponse() : Request failed with status "
            << status;
    return false;
  }

  // Get the access token, if any.
  response_object->GetString(kAccessTokenString, access_token);

  // Get the location
  Value* location_value = NULL;
  if (!response_object->Get(kLocationString, &location_value)) {
    VLOG(1) << "ParseServerResponse() : Missing location attribute.";
    // GLS returns a response with no location property to represent
    // no fix available; return true to indicate successful parse.
    return true;
  }
  DCHECK(location_value);

  if (!location_value->IsType(Value::TYPE_DICTIONARY)) {
    if (!location_value->IsType(Value::TYPE_NULL)) {
      VLOG(1) << "ParseServerResponse() : Unexpected location type "
              << location_value->GetType();
      // If the network provider was unable to provide a position fix, it should
      // return a HTTP 200, with "location" : null. Otherwise it's an error.
      return false;
    }
    return true;  // Successfully parsed response containing no fix.
  }
  DictionaryValue* location_object =
      static_cast<DictionaryValue*>(location_value);

  // latitude and longitude fields are always required.
  double latitude, longitude;
  if (!GetAsDouble(*location_object, kLatitudeString, &latitude) ||
      !GetAsDouble(*location_object, kLongitudeString, &longitude)) {
    VLOG(1) << "ParseServerResponse() : location lacks lat and/or long.";
    return false;
  }
  // All error paths covered: now start actually modifying postion.
  position->latitude = latitude;
  position->longitude = longitude;
  position->timestamp = timestamp;

  // Other fields are optional.
  GetAsDouble(*response_object, kAccuracyString, &position->accuracy);

  return true;
}

}  // namespace
