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

  // Load a given URL in the main frame.
  virtual Status Load(const std::string& url) = 0;

  // Evaluates a JavaScript expression in a specified frame and returns
  // the result. |frame| is a frame ID or an empty string for the main frame.
  // If the expression evaluates to a element, it will be bound to a unique ID
  // (per frame) and the ID will be returned.
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& expression,
                                scoped_ptr<base::Value>* result) = 0;

  // Calls a JavaScript function in a specified frame with the given args and
  // returns the result. |frame| is a frame ID or an empty string for the main
  // frame. |args| may contain IDs that refer to previously returned elements.
  // These will be translated back to their referred objects before invoking the
  // function.
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) = 0;

  // Gets the frame ID for a frame element returned by invoking the given
  // JavaScript function. |frame| is a frame ID or an empty string for the main
  // frame.
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::ListValue& args,
                                    std::string* out_frame) = 0;

  // Quits Chrome.
  virtual Status Quit() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_H_
