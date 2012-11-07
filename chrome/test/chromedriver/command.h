// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_COMMAND_H_
#define CHROME_TEST_CHROMEDRIVER_COMMAND_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class Value;
}

class Status;

typedef base::Callback<Status(
    const base::DictionaryValue&,
    const std::string&,
    scoped_ptr<base::Value>*,
    std::string*)> Command;

#endif  // CHROME_TEST_CHROMEDRIVER_COMMAND_H_
