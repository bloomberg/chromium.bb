// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/cookie.h"

#include <time.h>

#include "base/string_util.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "net/base/cookie_monster.h"

namespace webdriver {

// Convert from a string format.
Cookie::Cookie(const std::string& cookie) {
  net::CookieMonster::ParsedCookie pc(cookie);
  expires_ = "";

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
    expires_ = pc.Expires();
  }
}

// Convert from a webdriver JSON representation of a cookie.
Cookie::Cookie(const DictionaryValue& dict) {
  int expiry;
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

  // Convert the time passed into human-readable format.
  if (dict.GetInteger("expiry", &expiry)) {
    time_t clock = (time_t) expiry;
#if defined(OS_WIN)
    char* buff = ctime(&clock);
    if (NULL == buff) {
      valid_ = false;
      return;
    }
#else
    char buff[128];
    memset(buff, 0, sizeof(buff));

    if (NULL == ctime_r(&clock, buff)) {
      valid_ = false;
      return;
    }
#endif
    expires_ = std::string(buff);
  }

  valid_ = true;
}

Cookie::~Cookie() {}

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
  cookie->SetString("expiry", expires_);

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
    cookie += "path=" + path_ + ";";
  }
  if (domain_ != "") {
    cookie += "domain=" + domain_ + ";";
  }
  if (secure_) {
    cookie += "secure;";
  }
  if (expires_ != "") {
    cookie += expires_ + ";";
  }
  if (http_) {
    cookie += "http_only;";
  }

  return cookie;
}

}  // namespace webdriver

