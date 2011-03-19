// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COOKIE_H_
#define CHROME_TEST_WEBDRIVER_COOKIE_H_
#pragma once

#include <string>

#include "base/time.h"
#include "base/values.h"

namespace webdriver {

// Class used to convert cookies to various formats.
class Cookie {
 public:
  explicit Cookie(const std::string& cookie);
  explicit Cookie(const DictionaryValue& dict);
  ~Cookie();

  // Converts a |time| object to a date time string, according to RFC 1123,
  // which is required by RFC 2616, section 14.21.
  static std::string ToDateString(const base::Time& time);

  DictionaryValue* ToDictionary();
  // ToJSONString() returns a string form of a JSON object with the required
  // WebDriver tags.  Note that this format cannot be used to set a cookie in
  // the chrome browser.  Example { "name"="foo", "value"="bar"}
  std::string ToJSONString();
  // ToString() returns a string format expected by chrome to for a cookie.
  // Example: "foo=bar"
  std::string ToString();

  bool valid() const { return valid_; }
  const std::string& name() const { return name_; }
  const std::string& value() const { return value_; }
  const std::string& path() const { return path_; }
  const std::string& domain() const { return domain_; }
  const base::Time& expiration() const { return expiration_; }
  bool secure() const { return secure_; }
  bool http_only() const { return http_; }

 private:
  std::string name_;
  std::string value_;
  std::string path_;
  std::string domain_;
  base::Time expiration_;
  bool secure_;
  bool http_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(Cookie);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COOKIE_H_
