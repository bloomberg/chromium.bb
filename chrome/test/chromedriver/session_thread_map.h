// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_THREAD_MAP_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_THREAD_MAP_H_

#include <map>
#include <memory>
#include <string>

#include "base/threading/thread.h"

using SessionThreadMap = std::map<std::string, std::unique_ptr<base::Thread>>;

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_THREAD_MAP_H_
