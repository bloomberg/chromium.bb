// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/cookie.h"

#include <time.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "net/base/cookie_monster.h"

namespace webdriver {

// Convert from a string format.
Cookie::Cookie(const std::string& cookie) {
  net::CookieMonster::ParsedCookie pc(cookie);

  valid_ = pc.IsValid();
  if (!valid_)
    return;

  name_ = pc.Name();
  value_ = pc.Value();
  secure_ = pc.IsSecure();
  http_ = pc.IsHttpOnly();

  if (pc.HasPath()) {
    path_ = pc.Path();
  }

  if (pc.HasDomain()) {
    domain_ = pc.Domain();
  }

  if (pc.HasExpires()) {
    base::Time parsed_time;
    if (base::Time::FromString(UTF8ToWide(pc.Expires()).c_str(), &parsed_time))
      expiration_ = parsed_time;
  }
}

// Convert from a webdriver JSON representation of a cookie.
Cookie::Cookie(const DictionaryValue& dict) {
  valid_ = false;
  http_ = false;

  // Name and Value are required paramters.
  if (!dict.GetString("name", &name_)) {
    return;
  }
  if (!dict.GetString("value", &value_)) {
    return;
  }
  if (!dict.GetString("path", &path_)) {
    path_ = "";
  }
  if (!dict.GetString("domain", &domain_)) {
    domain_ = "";
  }
  if (!dict.GetBoolean("secure", &secure_)) {
    secure_ = false;
  }

  if (dict.HasKey("expiry")) {
    // The expiration time need only be a number with seconds since UTC epcoh;
    // it could be either a floating point or integer number.
    double expiry;
    if (!dict.GetDouble("expiry", &expiry)) {
      return;  // Expiry is not a number, so the cookie is invalid.
    }

    expiration_ = base::Time::FromDoubleT(expiry);
  }

  valid_ = true;
}

Cookie::~Cookie() {}

// static
std::string Cookie::ToDateString(const base::Time& time) {
  static const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  struct base::Time::Exploded exploded;
  time.UTCExplode(&exploded);

  // DD MMM YYYY HH:mm:ss GMT
  return base::StringPrintf("%02d %s %04d %02d:%02d:%02d GMT",
      exploded.day_of_month, kMonths[exploded.month - 1], exploded.year,
      exploded.hour, exploded.minute, exploded.second);
}

// Convert's to webdriver's cookie spec, see:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol#/session/:sessionId/cookie
DictionaryValue* Cookie::ToDictionary() {
  DictionaryValue* cookie = new DictionaryValue();

  // Required parameters for WebDriver protocol.
  cookie->SetString("name", name_);
  cookie->SetString("value", value_);
  cookie->SetString("path", path_);
  cookie->SetString("domain", domain_);
  cookie->SetBoolean("secure", secure_);

  if (!expiration_.is_null()) {
    cookie->SetDouble("expiry",
        (expiration_ - base::Time::UnixEpoch()).InSecondsF());
  }

  // Chrome specific additons which are not a part of the JSON over HTTP spec.
  cookie->SetBoolean("http_only", http_);

  return cookie;
}

// Returns a string representation of the JSON webdriver format.  This is
// used when logging cookies sent/recieved on the server.
std::string Cookie::ToJSONString() {
  scoped_ptr<DictionaryValue> cookie(ToDictionary());
  std::string json;

  base::JSONWriter::Write(cookie.get(), false, &json);
  return json;
}

// Returns a string representation of the cookie to be used by Automation Proxy.
// TODO(jmikhail): Add function to Automation Proxy that can set a cookie by
// using a DictionaryValue object.
std::string Cookie::ToString() {
  std::string cookie = "";
  cookie += name_ + "=" + value_ + ";";

  if (path_ != "") {
    cookie += " path=" + path_ + ";";
  }
  if (domain_ != "") {
    // The domain must contain at least (2) periods to be valid. This will not
    // be the case for localhost, so we special case that and omit it from the
    // cookie string.
    // See http://curl.haxx.se/rfc/cookie_spec.html
    cookie += " domain=" + (domain_ != "localhost" ? domain_ : "") + ";";
  }
  if (!expiration_.is_null()) {
    cookie += " expires=" + ToDateString(expiration_) + ";";
  }
  if (secure_) {
    cookie += " secure;";
  }
  if (http_) {
    cookie += " http_only;";
  }

  return cookie;
}

}  // namespace webdriver
