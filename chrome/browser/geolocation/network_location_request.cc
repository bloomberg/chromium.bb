// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// TODO(joth): port to chromium
#if 0

#include "gears/geolocation/network_location_request.h"

#include "gears/blob/blob_utils.h"
#include "gears/blob/buffer_blob.h"
#include "gears/localserver/common/http_constants.h"
#include "third_party/jsoncpp/reader.h"
#include "third_party/jsoncpp/value.h"
#include "third_party/jsoncpp/writer.h"

static const char *kGearsNetworkLocationProtocolVersion = "1.1.0";

static const char *kAccessTokenString = "access_token";
static const char *kLatitudeString = "latitude";
static const char *kLongitudeString = "longitude";
static const char *kAltitudeString = "altitude";
static const char *kAccuracyString = "accuracy";
static const char *kAltitudeAccuracyString = "altitude_accuracy";
// Note that the corresponding JavaScript Position property is 'gearsAddress'.
static const char *kAddressString = "address";
static const char *kStreetNumberString = "street_number";
static const char *kStreetString = "street";
static const char *kPremisesString = "premises";
static const char *kCityString = "city";
static const char *kCountyString = "county";
static const char *kRegionString = "region";
static const char *kCountryString = "country";
static const char *kCountryCodeString = "country_code";
static const char *kPostalCodeString = "postal_code";

// Local functions
static const char16* RadioTypeToString(RadioType type);
// Adds a string if it's valid to the JSON object.
static void AddString(const std::string &property_name,
                      const std::string16 &value,
                      Json::Value *object);
// Adds an integer if it's valid to the JSON object.
static void AddInteger(const std::string &property_name,
                       const int &value,
                       Json::Value *object);
// Returns true if the value is a valid angle.
static bool IsValidAngle(const double &value);
// Parses the server response body. Returns true if parsing was successful.
static bool ParseServerResponse(const std::string &response_body,
                                int64 timestamp,
                                bool is_reverse_geocode,
                                Position *position,
                                std::string16 *access_token);
static void AddRadioData(const RadioData &radio_data, Json::Value *body_object);
static void AddWifiData(const WifiData &wifi_data, Json::Value *body_object);

// static
NetworkLocationRequest *NetworkLocationRequest::Create(
    BrowsingContext *browsing_context,
    const std::string16 &url,
    const std::string16 &host_name,
    ListenerInterface *listener) {
  scoped_ptr<NetworkLocationRequest> request(
      new NetworkLocationRequest(browsing_context, url, host_name, listener));
  assert(request.get());
  if (!request.get() || !request->Init() || !request->Start()) {
    return NULL;
  }
  return request.release();
}

NetworkLocationRequest::NetworkLocationRequest(
    BrowsingContext *browsing_context,
    const std::string16 &url,
    const std::string16 &host_name,
    ListenerInterface *listener)
    : AsyncTask(browsing_context),
      listener_(listener),
      url_(url),
      host_name_(host_name),
      is_shutting_down_(false) {
}

bool NetworkLocationRequest::MakeRequest(const std::string16 &access_token,
                                         const RadioData &radio_data,
                                         const WifiData &wifi_data,
                                         bool request_address,
                                         const std::string16 &address_language,
                                         double latitude,
                                         double longitude,
                                         int64 timestamp) {
  is_reverse_geocode_ = request_address &&
                        IsValidAngle(latitude) &&
                        IsValidAngle(longitude);
  if (!FormRequestBody(host_name_, access_token, radio_data, wifi_data,
                       request_address, address_language, latitude, longitude,
                       is_reverse_geocode_, &post_body_)) {
    return false;
  }
  timestamp_ = timestamp;

  thread_event_.Signal();
  return true;
}

// AsyncTask implementation.
void NetworkLocationRequest::Run() {
  while (true) {
    thread_event_.Wait();
    if (is_shutting_down_) {
      break;
    }
    MakeRequestImpl();
  }
}

void NetworkLocationRequest::MakeRequestImpl() {
  WebCacheDB::PayloadInfo payload;
  // TODO(andreip): remove this once WebCacheDB::PayloadInfo.data is a Blob.
  scoped_refptr<BlobInterface> payload_data;
  bool result = HttpPost(url_.c_str(),
                         false,            // Not capturing, so follow redirects
                         NULL,             // reason_header_value
                         HttpConstants::kMimeApplicationJson,  // Content-Type
                         NULL,             // mod_since_date
                         NULL,             // required_cookie
                         true,             // disable_browser_cookies
                         post_body_.get(),
                         &payload,
                         &payload_data,
                         NULL,             // was_redirected
                         NULL,             // full_redirect_url
                         NULL);            // error_message

  MutexLock lock(&is_processing_response_mutex_);
  // is_aborted_ may be true even if HttpPost succeeded.
  if (is_aborted_) {
    LOG(("NetworkLocationRequest::Run() : HttpPost request was cancelled.\n"));
    return;
  }

  if (listener_) {
    Position position;
    std::string response_body;
    if (result) {
      // If HttpPost succeeded, payload_data is guaranteed to be non-NULL.
      assert(payload_data.get());
      if (!payload_data->Length() ||
          !BlobToString(payload_data.get(), &response_body)) {
        LOG(("NetworkLocationRequest::Run() : Failed to get response body.\n"));
      }
    }
    std::string16 access_token;
    GetLocationFromResponse(result, payload.status_code, response_body,
                            timestamp_, url_, is_reverse_geocode_,
                            &position, &access_token);

    LOG(("NetworkLocationRequest::Run() : Calling listener with position.\n"));
    bool server_error =
        !result || (payload.status_code >= 500 && payload.status_code < 600);
    listener_->LocationResponseAvailable(position, server_error, access_token);
  }
}

// static
bool NetworkLocationRequest::FormRequestBody(
    const std::string16 &host_name,
    const std::string16 &access_token,
    const RadioData &radio_data,
    const WifiData &wifi_data,
    bool request_address,
    std::string16 address_language,
    double latitude,
    double longitude,
    bool is_reverse_geocode,
    scoped_refptr<BlobInterface> *blob) {
  assert(blob);
  Json::Value body_object;
  assert(body_object.isObject());
  // Version and host are required.
  if (host_name.empty()) {
    return false;
  }
  body_object["version"] = Json::Value(kGearsNetworkLocationProtocolVersion);
  AddString("host", host_name, &body_object);

  AddString("access_token", access_token, &body_object);

  body_object["request_address"] = request_address;
  AddString("address_language", address_language, &body_object);

  if (is_reverse_geocode) {
    assert(request_address);
    assert(IsValidAngle(latitude) && IsValidAngle(longitude));
    Json::Value location;
    location["latitude"] = Json::Value(latitude);
    location["longitude"] = Json::Value(longitude);
    body_object["location"] = location;
  } else {
    AddRadioData(radio_data, &body_object);
    AddWifiData(wifi_data, &body_object);
  }

  Json::FastWriter writer;
  // We always use the platform independent 'C' locale when writing the JSON
  // request, irrespective of the browser's locale. This avoids the need for
  // the network location provider to determine the locale of the request and
  // parse the JSON accordingly.
#ifdef OS_WINCE
  // WinCE does not support setlocale.
#else
  char *current_locale = setlocale(LC_NUMERIC, "C");
#endif
  std::string body_string = writer.write(body_object);
#ifdef OS_WINCE
  // WinCE does not support setlocale.
#else
  setlocale(LC_NUMERIC, current_locale);
#endif
  LOG(("NetworkLocationRequest::FormRequestBody(): Formed body %s.\n",
       body_string.c_str()));

  blob->reset(new BufferBlob(body_string.c_str(), body_string.size()));
  return true;
}

// static
void NetworkLocationRequest::GetLocationFromResponse(
    bool http_post_result,
    int status_code,
    const std::string &response_body,
    int64 timestamp,
    const std::string16 &server_url,
    bool is_reverse_geocode,
    Position *position,
    std::string16 *access_token) {
  assert(position);
  assert(access_token);

  // HttpPost can fail for a number of reasons. Most likely this is because
  // we're offline, or there was no response.
  if (!http_post_result) {
    LOG(("NetworkLocationRequest::GetLocationFromResponse() : HttpPost request "
         "failed.\n"));
    position->error_code = Position::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = STRING16(L"No response from network provider "
                                       L"at ");
    position->error_message += server_url.c_str();
    position->error_message += STRING16(L".");
  } else if (status_code == HttpConstants::HTTP_OK) {
    // We use the timestamp from the device data that was used to generate
    // this position fix.
    if (ParseServerResponse(response_body, timestamp, is_reverse_geocode,
                            position, access_token)) {
      // The response was successfully parsed, but it may not be a valid
      // position fix.
      if (!position->IsGoodFix()) {
        position->error_code = Position::ERROR_CODE_POSITION_UNAVAILABLE;
        position->error_message = STRING16(L"Network provider at ");
        position->error_message += server_url.c_str();
        position->error_message += STRING16(L" did not provide a good position "
                                            L"fix.");
      }
    } else {
      // We failed to parse the repsonse.
      LOG(("NetworkLocationRequest::GetLocationFromResponse() : Response "
           "malformed.\n"));
      position->error_code = Position::ERROR_CODE_POSITION_UNAVAILABLE;
      position->error_message = STRING16(L"Response from network provider at ");
      position->error_message += server_url.c_str();
      position->error_message += STRING16(L" was malformed.");
    }
  } else {
    // The response was bad.
    LOG(("NetworkLocationRequest::GetLocationFromResponse() : HttpPost "
         "response was bad.\n"));
    position->error_code = Position::ERROR_CODE_POSITION_UNAVAILABLE;
    position->error_message = STRING16(L"Network provider at ");
    position->error_message += server_url.c_str();
    position->error_message += STRING16(L" returned error code ");
    position->error_message += IntegerToString16(status_code);
    position->error_message += STRING16(L".");
  }
}

void NetworkLocationRequest::StopThreadAndDelete() {
  // The FF implementation of AsyncTask::Abort() delivers a message to the UI
  // thread to cancel the request. So if we call this method on the UI thread,
  // we must return to the OS before the call to Abort() will take effect. In
  // particular, we can't call Abort() then block here waiting for HttpPost to
  // return.
  is_shutting_down_ = true;
  thread_event_.Signal();
  is_processing_response_mutex_.Lock();
  Abort();
  is_processing_response_mutex_.Unlock();
  DeleteWhenDone();
}

// Local functions.

static const char16* RadioTypeToString(RadioType type) {
  switch (type) {
    case RADIO_TYPE_UNKNOWN:
      return STRING16(L"unknown");
    case RADIO_TYPE_GSM:
      return STRING16(L"gsm");
    case RADIO_TYPE_CDMA:
      return STRING16(L"cdma");
    case RADIO_TYPE_WCDMA:
      return STRING16(L"wcdma");
    default:
      assert(false);
  }
  return NULL;
}

static void AddString(const std::string &property_name,
                      const std::string16 &value,
                      Json::Value *object) {
  assert(object);
  assert(object->isObject());
  if (!value.empty()) {
    std::string value_utf8;
    if (String16ToUTF8(value.c_str(), value.size(), &value_utf8)) {
      (*object)[property_name] = Json::Value(value_utf8);
    }
  }
}

static void AddInteger(const std::string &property_name,
                       const int &value,
                       Json::Value *object) {
  assert(object);
  assert(object->isObject());
  if (kint32min != value) {
    (*object)[property_name] = Json::Value(value);
  }
}

static bool IsValidAngle(const double &value) {
  return value >= -180.0 && value <= 180.0;
}

// Numeric values without a decimal point have type integer and IsDouble() will
// return false. This is convenience function for detecting integer or floating
// point numeric values. Note that isIntegral() includes boolean values, which
// is not what we want.
static bool IsDoubleOrInt(const Json::Value &object,
                          const std::string &property_name) {
  return object[property_name].isDouble() || object[property_name].isInt();
}

// The JsValue::asXXX() methods return zero if a property isn't specified. For
// our purposes, zero is a valid value, so we have to test for existence.

// Gets a double if it's present.
static bool GetAsDouble(const Json::Value &object,
                        const std::string &property_name,
                        double *out) {
  assert(out);
  if (!IsDoubleOrInt(object, property_name)) {
    return false;
  }
  *out = object[property_name].asDouble();
  return true;
}

// Gets a string if it's present.
static bool GetAsString(const Json::Value &object,
                        const std::string &property_name,
                        std::string16 *out) {
  assert(out);
  if (!object[property_name].isString()) {
    return false;
  }
  std::string out_utf8 = object[property_name].asString();
  return UTF8ToString16(out_utf8.c_str(), out_utf8.size(), out);
}

static bool ParseServerResponse(const std::string &response_body,
                                int64 timestamp,
                                bool is_reverse_geocode,
                                Position *position,
                                std::string16 *access_token) {
  assert(position);
  assert(access_token);

  if (response_body.empty()) {
    LOG(("ParseServerResponse() : Response was empty.\n"));
    return false;
  }
  LOG(("ParseServerResponse() : Parsing response %s.\n",
       response_body.c_str()));

  // Parse the response, ignoring comments. The JSON reposne from the network
  // location provider should always use the 'C' locale.
  Json::Reader reader;
  Json::Value response_object;
#ifdef OS_WINCE
  // WinCE does not support setlocale.
#else
  char *current_locale = setlocale(LC_NUMERIC, "C");
#endif
  bool res = reader.parse(response_body, response_object, false);
#ifdef OS_WINCE
  // WinCE does not support setlocale.
#else
  setlocale(LC_NUMERIC, current_locale);
#endif
  if (!res) {
    LOG(("ParseServerResponse() : Failed to parse response : %s.\n",
        reader.getFormatedErrorMessages().c_str()));
    return false;
  }

  if (!response_object.isObject()) {
    LOG(("ParseServerResponse() : Unexpected response type: %d.\n",
        response_object.type()));
    return false;
  }

  // Get the access token.
  GetAsString(response_object, kAccessTokenString, access_token);

  // Get the location
  Json::Value location = response_object["location"];

  // If the network provider was unable to provide a position fix, it should
  // return a 200 with location == null.
  if (location.type() == Json::nullValue) {
    LOG(("ParseServerResponse() : Location is null.\n"));
    return true;
  }

  // If location is not null, it must be an object.
  if (!location.isObject()) {
    return false;
  }

  // latitude and longitude fields are always required.
  if (!GetAsDouble(location, kLatitudeString, &position->latitude) ||
      !GetAsDouble(location, kLongitudeString, &position->longitude)) {
    return false;
  }

  // If it's not a reverse geocode request, the accuracy field is also required.
  if (is_reverse_geocode) {
    position->accuracy = 0.0;
  } else {
    if (!GetAsDouble(location, kAccuracyString, &position->accuracy)) {
      return false;
    }
  }

  // Other fields are optional.
  GetAsDouble(location, kAltitudeString, &position->altitude);
  GetAsDouble(location, kAltitudeAccuracyString, &position->altitude_accuracy);
  Json::Value address = location[kAddressString];
  if (address.isObject()) {
    GetAsString(address, kStreetNumberString, &position->address.street_number);
    GetAsString(address, kStreetString, &position->address.street);
    GetAsString(address, kPremisesString, &position->address.premises);
    GetAsString(address, kCityString, &position->address.city);
    GetAsString(address, kCountyString, &position->address.county);
    GetAsString(address, kRegionString, &position->address.region);
    GetAsString(address, kCountryString, &position->address.country);
    GetAsString(address, kCountryCodeString, &position->address.country_code);
    GetAsString(address, kPostalCodeString, &position->address.postal_code);
  }

  position->timestamp = timestamp;
  return true;
}

static void AddRadioData(const RadioData &radio_data,
                         Json::Value *body_object) {
  assert(body_object);

  AddInteger("home_mobile_country_code", radio_data.home_mobile_country_code,
             body_object);
  AddInteger("home_mobile_network_code", radio_data.home_mobile_network_code,
             body_object);
  AddString("radio_type", RadioTypeToString(radio_data.radio_type),
            body_object);
  AddString("carrier", radio_data.carrier, body_object);

  Json::Value cell_towers;
  assert(cell_towers.isArray());
  int num_cell_towers = static_cast<int>(radio_data.cell_data.size());
  for (int i = 0; i < num_cell_towers; ++i) {
    Json::Value cell_tower;
    assert(cell_tower.isObject());
    AddInteger("cell_id", radio_data.cell_data[i].cell_id, &cell_tower);
    AddInteger("location_area_code", radio_data.cell_data[i].location_area_code,
               &cell_tower);
    AddInteger("mobile_country_code",
               radio_data.cell_data[i].mobile_country_code, &cell_tower);
    AddInteger("mobile_network_code",
               radio_data.cell_data[i].mobile_network_code, &cell_tower);
    AddInteger("age", radio_data.cell_data[i].age, &cell_tower);
    AddInteger("signal_strength", radio_data.cell_data[i].radio_signal_strength,
               &cell_tower);
    AddInteger("timing_advance", radio_data.cell_data[i].timing_advance,
               &cell_tower);
    cell_towers[i] = cell_tower;
  }
  if (num_cell_towers > 0) {
    (*body_object)["cell_towers"] = cell_towers;
  }
}

static void AddWifiData(const WifiData &wifi_data, Json::Value *body_object) {
  assert(body_object);

  if (wifi_data.access_point_data.empty()) {
    return;
  }

  Json::Value wifi_towers;
  assert(wifi_towers.isArray());
  for (WifiData::AccessPointDataSet::const_iterator iter =
       wifi_data.access_point_data.begin();
       iter != wifi_data.access_point_data.end();
       iter++) {
    Json::Value wifi_tower;
    assert(wifi_tower.isObject());
    AddString("mac_address", iter->mac_address, &wifi_tower);
    AddInteger("signal_strength", iter->radio_signal_strength, &wifi_tower);
    AddInteger("age", iter->age, &wifi_tower);
    AddInteger("channel", iter->channel, &wifi_tower);
    AddInteger("signal_to_noise", iter->signal_to_noise, &wifi_tower);
    AddString("ssid", iter->ssid, &wifi_tower);
    wifi_towers.append(wifi_tower);
  }
  (*body_object)["wifi_towers"] = wifi_towers;
}

#endif  // if 0
