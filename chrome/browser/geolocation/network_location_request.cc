// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/network_location_request.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/geolocation/geoposition.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace {
const char* const kMimeApplicationJson = "application/json";

// See http://code.google.com/apis/gears/geolocation_network_protocol.html
const char* kGeoLocationNetworkProtocolVersion = "1.1.0";

const wchar_t* kAccessTokenString = L"access_token";
const wchar_t* kLocationString = L"location";
const wchar_t* kLatitudeString = L"latitude";
const wchar_t* kLongitudeString = L"longitude";
const wchar_t* kAltitudeString = L"altitude";
const wchar_t* kAccuracyString = L"accuracy";
const wchar_t* kAltitudeAccuracyString = L"altitude_accuracy";

// Local functions
// Creates the request payload to send to the server.
bool FormRequestBody(const string16& host_name,
                     const string16& access_token,
                     const RadioData& radio_data,
                     const WifiData& wifi_data,
                     std::string* data);
// Parsers the server response.
void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             int64 timestamp,
                             const GURL& server_url,
                             Position* position,
                             string16* access_token);

const char* RadioTypeToString(RadioType type);
// Adds a string if it's valid to the JSON object.
void AddString(const std::wstring& property_name,
               const string16& value,
               DictionaryValue* object);
// Adds an integer if it's valid to the JSON object.
void AddInteger(const std::wstring& property_name,
                int value,
                DictionaryValue* object);
// Parses the server response body. Returns true if parsing was successful.
bool ParseServerResponse(const std::string& response_body,
                         int64 timestamp,
                         Position* position,
                         string16* access_token);
void AddRadioData(const RadioData& radio_data, DictionaryValue* body_object);
void AddWifiData(const WifiData& wifi_data, DictionaryValue* body_object);
}  // namespace

NetworkLocationRequest::NetworkLocationRequest(URLRequestContextGetter* context,
                                               const GURL& url,
                                               const string16& host_name,
                                               ListenerInterface* listener)
    : url_context_(context), timestamp_(kint64min), listener_(listener),
      url_(url), host_name_(host_name) {
//  DCHECK(url_context_);
  DCHECK(listener);
}

NetworkLocationRequest::~NetworkLocationRequest() {
}

bool NetworkLocationRequest::MakeRequest(const string16& access_token,
                                         const RadioData& radio_data,
                                         const WifiData& wifi_data,
                                         int64 timestamp) {
  if (url_fetcher_ != NULL) {
    DLOG(INFO) << "NetworkLocationRequest : Cancelling pending request";
    url_fetcher_.reset();
  }
  std::string post_body;
  if (!FormRequestBody(host_name_, access_token, radio_data, wifi_data,
                       &post_body)) {
    return false;
  }
  timestamp_ = timestamp;

  url_fetcher_.reset(URLFetcher::Create(
      host_name_.size(),  // Used for testing
      url_, URLFetcher::POST, this));
  url_fetcher_->set_upload_data(kMimeApplicationJson, post_body);
  url_fetcher_->set_request_context(url_context_);
  url_fetcher_->Start();
  return true;
}

void NetworkLocationRequest::OnURLFetchComplete(const URLFetcher* source,
                                                const GURL& url,
                                                const URLRequestStatus& status,
                                                int response_code,
                                                const ResponseCookies& cookies,
                                                const std::string& data) {
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(url_.possibly_invalid_spec() == url.possibly_invalid_spec());

  Position position;
  string16 access_token;
  GetLocationFromResponse(status.is_success(), response_code, data,
                          timestamp_, url, &position, &access_token);
  const bool server_error =
      !status.is_success() || (response_code >= 500 && response_code < 600);
  url_fetcher_.reset();

  DCHECK(listener_);
  DLOG(INFO) << "NetworkLocationRequest::Run() : "
                "Calling listener with position.\n";
  listener_->LocationResponseAvailable(position, server_error, access_token);
}

// Local functions.
namespace {

bool FormRequestBody(const string16& host_name,
                     const string16& access_token,
                     const RadioData& radio_data,
                     const WifiData& wifi_data,
                     std::string* data) {
  DCHECK(data);

  DictionaryValue body_object;
  // Version and host are required.
  if (host_name.empty()) {
    return false;
  }
  body_object.SetString(L"version", kGeoLocationNetworkProtocolVersion);
  AddString(L"host", host_name, &body_object);

  AddString(L"access_token", access_token, &body_object);

  body_object.SetBoolean(L"request_address", false);

  AddRadioData(radio_data, &body_object);
  AddWifiData(wifi_data, &body_object);

  // TODO(joth): Do we need to mess with locales?
  // We always use the platform independent 'C' locale when writing the JSON
  // request, irrespective of the browser's locale. This avoids the need for
  // the network location provider to determine the locale of the request and
  // parse the JSON accordingly.
//  char* current_locale = setlocale(LC_NUMERIC, "C");

  base::JSONWriter::Write(&body_object, false, data);

//  setlocale(LC_NUMERIC, current_locale);

  DLOG(INFO) << "NetworkLocationRequest::FormRequestBody(): Formed body "
             << data << ".\n";
  return true;
}

void FormatPositionError(const GURL& server_url,
                         const std::wstring& message,
                         Position* position) {
    position->error_code = Position::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = L"Network location provider at '";
    position->error_message += ASCIIToWide(server_url.possibly_invalid_spec());
    position->error_message += L"' : ";
    position->error_message += message;
    position->error_message += L".";
    LOG(INFO) << "NetworkLocationRequest::GetLocationFromResponse() : "
              << position->error_message;
}

void GetLocationFromResponse(bool http_post_result,
                             int status_code,
                             const std::string& response_body,
                             int64 timestamp,
                             const GURL& server_url,
                             Position* position,
                             string16* access_token) {
  DCHECK(position);
  DCHECK(access_token);

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (!http_post_result) {
    FormatPositionError(server_url, L"No response received", position);
    return;
  }
  if (status_code != 200) {  // XXX is '200' in a constant? Can't see it
    // The response was bad.
    std::wstring message = L"Returned error code ";
    message += IntToWString(status_code);
    FormatPositionError(server_url, message, position);
    return;
  }
  // We use the timestamp from the device data that was used to generate
  // this position fix.
  if (!ParseServerResponse(response_body, timestamp, position, access_token)) {
    // We failed to parse the repsonse.
    FormatPositionError(server_url, L"Response was malformed", position);
    return;
  }
  // The response was successfully parsed, but it may not be a valid
  // position fix.
  if (!position->IsValidFix()) {
    FormatPositionError(server_url,
                        L"Did not provide a good position fix", position);
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

void AddString(const std::wstring& property_name,
               const string16& value,
               DictionaryValue* object) {
  DCHECK(object);
  if (!value.empty()) {
    object->SetStringFromUTF16(property_name, value);
  }
}

void AddInteger(const std::wstring& property_name,
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
                 const std::wstring& property_name,
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
  return value->GetAsReal(out);
}

bool ParseServerResponse(const std::string& response_body,
                         int64 timestamp,
                         Position* position,
                         string16* access_token) {
  DCHECK(position);
  DCHECK(access_token);
  DCHECK(timestamp != kint64min);

  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse() : Response was empty.\n";
    return false;
  }
  DLOG(INFO) << "ParseServerResponse() : Parsing response "
             << response_body << ".\n";

  // Parse the response, ignoring comments.
  // TODO(joth): Gears version stated: The JSON reponse from the network
  // location provider should always use the 'C' locale.
  // Chromium JSON parser works in UTF8 so hopefully we can ignore setlocale?

//  char* current_locale = setlocale(LC_NUMERIC, "C");
  std::string error_msg;
  scoped_ptr<Value> response_value(base::JSONReader::ReadAndReturnError(
      response_body, false, &error_msg));

//  setlocale(LC_NUMERIC, current_locale);

  if (response_value == NULL) {
    LOG(WARNING) << "ParseServerResponse() : JSONReader failed : "
                 << error_msg << ".\n";
    return false;
  }

  if (!response_value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(INFO) << "ParseServerResponse() : Unexpected resopnse type "
              << response_value->GetType() <<  ".\n";
    return false;
  }
  const DictionaryValue* response_object =
      static_cast<DictionaryValue*>(response_value.get());

  // Get the access token, if any.
  response_object->GetStringAsUTF16(kAccessTokenString, access_token);

  // Get the location
  DictionaryValue* location_object;
  if (!response_object->GetDictionary(kLocationString, &location_object)) {
    Value* value = NULL;
    // If the network provider was unable to provide a position fix, it should
    // return a 200 with location == null. Otherwise it's an error.
    return response_object->Get(kLocationString, &value)
           && value && value->IsType(Value::TYPE_NULL);
  }
  DCHECK(location_object);

  // latitude and longitude fields are always required.
  double latitude, longitude;
  if (!GetAsDouble(*location_object, kLatitudeString, &latitude) ||
      !GetAsDouble(*location_object, kLongitudeString, &longitude)) {
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

void AddRadioData(const RadioData& radio_data, DictionaryValue* body_object) {
  DCHECK(body_object);

  AddInteger(L"home_mobile_country_code", radio_data.home_mobile_country_code,
             body_object);
  AddInteger(L"home_mobile_network_code", radio_data.home_mobile_network_code,
             body_object);
  AddString(L"radio_type",
            ASCIIToUTF16(RadioTypeToString(radio_data.radio_type)),
            body_object);
  AddString(L"carrier", radio_data.carrier, body_object);

  const int num_cell_towers = static_cast<int>(radio_data.cell_data.size());
  if (num_cell_towers == 0) {
    return;
  }
  ListValue* cell_towers = new ListValue;
  for (int i = 0; i < num_cell_towers; ++i) {
    DictionaryValue* cell_tower = new DictionaryValue;
    AddInteger(L"cell_id", radio_data.cell_data[i].cell_id, cell_tower);
    AddInteger(L"location_area_code",
               radio_data.cell_data[i].location_area_code, cell_tower);
    AddInteger(L"mobile_country_code",
               radio_data.cell_data[i].mobile_country_code, cell_tower);
    AddInteger(L"mobile_network_code",
               radio_data.cell_data[i].mobile_network_code, cell_tower);
    AddInteger(L"age", radio_data.cell_data[i].age, cell_tower);
    AddInteger(L"signal_strength",
               radio_data.cell_data[i].radio_signal_strength, cell_tower);
    AddInteger(L"timing_advance", radio_data.cell_data[i].timing_advance,
               cell_tower);
    cell_towers->Append(cell_tower);
  }
  body_object->Set(L"cell_towers", cell_towers);
}

void AddWifiData(const WifiData& wifi_data, DictionaryValue* body_object) {
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
    AddString(L"mac_address", iter->mac_address, wifi_tower);
    AddInteger(L"signal_strength", iter->radio_signal_strength, wifi_tower);
    AddInteger(L"age", iter->age, wifi_tower);
    AddInteger(L"channel", iter->channel, wifi_tower);
    AddInteger(L"signal_to_noise", iter->signal_to_noise, wifi_tower);
    AddString(L"ssid", iter->ssid, wifi_tower);
    wifi_towers->Append(wifi_tower);
  }
  body_object->Set(L"wifi_towers", wifi_towers);
}
}  // namespace
