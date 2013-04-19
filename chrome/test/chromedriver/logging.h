// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_LOGGING_H_
#define CHROME_TEST_CHROMEDRIVER_LOGGING_H_

#include "base/memory/scoped_vector.h"

struct Capabilities;
class DevToolsEventLogger;
class Status;

Status CreateLoggers(const Capabilities& capabilities,
                     ScopedVector<DevToolsEventLogger>* out_loggers);

#endif  // CHROME_TEST_CHROMEDRIVER_LOGGING_H_
