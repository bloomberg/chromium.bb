// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_COOKIE_H_
#define CHROME_TEST_WEBDRIVER_COOKIE_H_
#pragma once

#include <string>

#include "base/values.h"

namespace webdriver {

// Class used to convert cookies to various formats.
class Cookie {
 public:
  explicit Cookie(const std::string& cookie);
  explicit Cookie(const DictionaryValue& dict);

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

 private:
  std::string name_;
  std::string value_;
  std::string path_;
  std::string domain_;
  std::string expires_;
  bool secure_;
  bool http_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(Cookie);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_COOKIE_H_
