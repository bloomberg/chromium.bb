// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/network_location_request.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/geoposition.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

namespace {
const char kMimeApplicationJson[] = "application/json";

// See http://code.google.com/apis/gears/geolocation_network_protocol.html
const char kGeoLocationNetworkProtocolVersion[] = "1.1.0";

const char kAccessTokenString[] = "access_token";
const char kLocationString[] = "location";
const char kLatitudeString[] = "latitude";
const char kLongitudeString[] = "longitude";
const char kAltitudeString[] = "altitude";
const char kAccuracyString[] = "accuracy";
const char kAltitudeAccuracyString[] = "altitude_accuracy";

// Local functions
// Creates the request payload to send to the server.
void FormRequestBody(const std::string& host_name,
                     const string16& access_token,
                     const GatewayData& gateway_data,
                     const RadioData& radio_data,
                     const WifiData& wifi_data,
                     const base::Time& timestamp,
                     std::string* data);
// Parsers the server response.
void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             const base::Time& timestamp,
                             const GURL& server_url,
                             Geoposition* position,
                             string16* access_token);

const char* RadioTypeToString(RadioType type);
// Adds a string if it's valid to the JSON object.
void AddString(const std::string& property_name,
               const string16& value,
               DictionaryValue* object);
// Adds an integer if it's valid to the JSON object.
void AddInteger(const std::string& property_name,
                int value,
                DictionaryValue* object);
// Parses the server response body. Returns true if parsing was successful.
// Sets |*position| to the parsed location if a valid fix was received,
// otherwise leaves it unchanged (i.e. IsInitialized() == false).
bool ParseServerResponse(const std::string& response_body,
                         const base::Time& timestamp,
                         Geoposition* position,
                         string16* access_token);
void AddGatewayData(const GatewayData& gateway_data,
                  int age_milliseconds,
                  DictionaryValue* body_object);
void AddRadioData(const RadioData& radio_data,
                  int age_milliseconds,
                  DictionaryValue* body_object);
void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 DictionaryValue* body_object);
}  // namespace

int NetworkLocationRequest::url_fetcher_id_for_tests = 0;

NetworkLocationRequest::NetworkLocationRequest(URLRequestContextGetter* context,
                                               const GURL& url,
                                               ListenerInterface* listener)
    : url_context_(context), listener_(listener),
      url_(url) {
  DCHECK(listener);
}

NetworkLocationRequest::~NetworkLocationRequest() {
}

bool NetworkLocationRequest::MakeRequest(const std::string& host_name,
                                         const string16& access_token,
                                         const GatewayData& gateway_data,
                                         const RadioData& radio_data,
                                         const WifiData& wifi_data,
                                         const base::Time& timestamp) {
  if (url_fetcher_ != NULL) {
    DVLOG(1) << "NetworkLocationRequest : Cancelling pending request";
    url_fetcher_.reset();
  }
  gateway_data_ = gateway_data;
  radio_data_ = radio_data;
  wifi_data_ = wifi_data;
  timestamp_ = timestamp;
  std::string post_body;
  FormRequestBody(host_name, access_token, gateway_data, radio_data_,
                  wifi_data_, timestamp_, &post_body);

  url_fetcher_.reset(URLFetcher::Create(
      url_fetcher_id_for_tests, url_, URLFetcher::POST, this));
  url_fetcher_->set_upload_data(kMimeApplicationJson, post_body);
  url_fetcher_->set_request_context(url_context_);
  url_fetcher_->set_load_flags(
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
  return true;
}

void NetworkLocationRequest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(url_.possibly_invalid_spec() == url.possibly_invalid_spec());

  Geoposition position;
  string16 access_token;
  GetLocationFromResponse(status.is_success(), response_code, data,
                          timestamp_, url, &position, &access_token);
  const bool server_error =
      !status.is_success() || (response_code >= 500 && response_code < 600);
  url_fetcher_.reset();

  DCHECK(listener_);
  DVLOG(1) << "NetworkLocationRequest::Run() : Calling listener with position.";
  listener_->LocationResponseAvailable(position, server_error, access_token,
                                       gateway_data_, radio_data_, wifi_data_);
}

// Local functions.
namespace {

void FormRequestBody(const std::string& host_name,
                     const string16& access_token,
                     const GatewayData& gateway_data,
                     const RadioData& radio_data,
                     const WifiData& wifi_data,
                     const base::Time& timestamp,
                     std::string* data) {
  DCHECK(data);

  DictionaryValue body_object;
  // Version and host are required.
  COMPILE_ASSERT(sizeof(kGeoLocationNetworkProtocolVersion) > 1,
                 must_include_valid_version);
  DCHECK(!host_name.empty());
  body_object.SetString("version", kGeoLocationNetworkProtocolVersion);
  body_object.SetString("host", host_name);

  AddString("access_token", access_token, &body_object);

  body_object.SetBoolean("request_address", false);

  int age = kint32min;  // Invalid so AddInteger() will ignore.
  if (!timestamp.is_null()) {
    // Convert absolute timestamps into a relative age.
    int64 delta_ms = (base::Time::Now() - timestamp).InMilliseconds();
    if (delta_ms >= 0 && delta_ms < kint32max)
      age = static_cast<int>(delta_ms);
  }
  AddRadioData(radio_data, age, &body_object);
  AddWifiData(wifi_data, age, &body_object);
  AddGatewayData(gateway_data, age, &body_object);

  base::JSONWriter::Write(&body_object, false, data);
  DVLOG(1) << "NetworkLocationRequest::FormRequestBody(): Formed body " << *data
           << ".";
}

void FormatPositionError(const GURL& server_url,
                         const std::string& message,
                         Geoposition* position) {
    position->error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
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
                             Geoposition* position,
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
  if (!position->IsValidFix()) {
    FormatPositionError(server_url,
                        "Did not provide a good position fix", position);
    return;
  }
}

const char* RadioTypeToString(RadioType type) {
  switch (type) {
    case RADIO_TYPE_UNKNOWN:
      break;
    case RADIO_TYPE_GSM:
      return "gsm";
    case RADIO_TYPE_CDMA:
      return "cdma";
    case RADIO_TYPE_WCDMA:
      return "wcdma";
    default:
      LOG(DFATAL) << "Bad RadioType";
  }
  return "unknown";
}

void AddString(const std::string& property_name,
               const string16& value,
               DictionaryValue* object) {
  DCHECK(object);
  if (!value.empty()) {
    object->SetString(property_name, value);
  }
}

void AddInteger(const std::string& property_name,
                int value,
                DictionaryValue* object) {
  DCHECK(object);
  if (kint32min != value) {
    object->SetInteger(property_name, value);
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
                         Geoposition* position,
                         string16* access_token) {
  DCHECK(position);
  DCHECK(!position->IsInitialized());
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
      response_body, false, NULL, &error_msg));
  if (response_value == NULL) {
    LOG(WARNING) << "ParseServerResponse() : JSONReader failed : "
                 << error_msg;
    return false;
  }

  if (!response_value->IsType(Value::TYPE_DICTIONARY)) {
    VLOG(1) << "ParseServerResponse() : Unexpected resopnse type "
            << response_value->GetType();
    return false;
  }
  const DictionaryValue* response_object =
      static_cast<DictionaryValue*>(response_value.get());

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
  GetAsDouble(*location_object, kAccuracyString, &position->accuracy);
  GetAsDouble(*location_object, kAltitudeString, &position->altitude);
  GetAsDouble(*location_object, kAltitudeAccuracyString,
              &position->altitude_accuracy);

  return true;
}

void AddRadioData(const RadioData& radio_data,
                  int age_milliseconds,
                  DictionaryValue* body_object) {
  DCHECK(body_object);

  AddInteger("home_mobile_country_code", radio_data.home_mobile_country_code,
             body_object);
  AddInteger("home_mobile_network_code", radio_data.home_mobile_network_code,
             body_object);
  AddString("radio_type",
            ASCIIToUTF16(RadioTypeToString(radio_data.radio_type)),
            body_object);
  AddString("carrier", radio_data.carrier, body_object);

  const int num_cell_towers = static_cast<int>(radio_data.cell_data.size());
  if (num_cell_towers == 0) {
    return;
  }
  ListValue* cell_towers = new ListValue;
  for (int i = 0; i < num_cell_towers; ++i) {
    DictionaryValue* cell_tower = new DictionaryValue;
    AddInteger("cell_id", radio_data.cell_data[i].cell_id, cell_tower);
    AddInteger("location_area_code",
               radio_data.cell_data[i].location_area_code, cell_tower);
    AddInteger("mobile_country_code",
               radio_data.cell_data[i].mobile_country_code, cell_tower);
    AddInteger("mobile_network_code",
               radio_data.cell_data[i].mobile_network_code, cell_tower);
    AddInteger("age", age_milliseconds, cell_tower);
    AddInteger("signal_strength",
               radio_data.cell_data[i].radio_signal_strength, cell_tower);
    AddInteger("timing_advance", radio_data.cell_data[i].timing_advance,
               cell_tower);
    cell_towers->Append(cell_tower);
  }
  body_object->Set("cell_towers", cell_towers);
}

void AddWifiData(const WifiData& wifi_data,
                 int age_milliseconds,
                 DictionaryValue* body_object) {
  DCHECK(body_object);

  if (wifi_data.access_point_data.empty()) {
    return;
  }

  ListValue* wifi_towers = new ListValue;
  for (WifiData::AccessPointDataSet::const_iterator iter =
       wifi_data.access_point_data.begin();
       iter != wifi_data.access_point_data.end();
       iter++) {
    DictionaryValue* wifi_tower = new DictionaryValue;
    AddString("mac_address", iter->mac_address, wifi_tower);
    AddInteger("signal_strength", iter->radio_signal_strength, wifi_tower);
    AddInteger("age", age_milliseconds, wifi_tower);
    AddInteger("channel", iter->channel, wifi_tower);
    AddInteger("signal_to_noise", iter->signal_to_noise, wifi_tower);
    AddString("ssid", iter->ssid, wifi_tower);
    wifi_towers->Append(wifi_tower);
  }
  body_object->Set("wifi_towers", wifi_towers);
}

void AddGatewayData(const GatewayData& gateway_data,
                    int age_milliseconds,
                    DictionaryValue* body_object) {
  DCHECK(body_object);

  if (gateway_data.router_data.empty()) {
    return;
  }

  ListValue* gateways = new ListValue;
  for (GatewayData::RouterDataSet::const_iterator iter =
       gateway_data.router_data.begin();
       iter != gateway_data.router_data.end();
       iter++) {
    DictionaryValue* gateway = new DictionaryValue;
    AddString("mac_address", iter->mac_address, gateway);
    gateways->Append(gateway);
  }
  body_object->Set("gateways", gateways);
}
}  // namespace
