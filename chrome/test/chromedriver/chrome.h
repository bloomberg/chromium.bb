// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class ListValue;
class Value;
}

class Status;

class Chrome {
 public:
  virtual ~Chrome() {}

  virtual Status Load(const std::string& url) = 0;
  virtual Status EvaluateScript(const std::string& expression,
                                scoped_ptr<base::Value>* result) = 0;
  virtual Status CallFunction(const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) = 0;
  virtual Status Quit() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_H_
