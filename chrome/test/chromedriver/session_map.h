// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SESSION_MAP_H_
#define CHROME_TEST_CHROMEDRIVER_SESSION_MAP_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/synchronized_map.h"

class SessionAccessor;

typedef SynchronizedMap<std::string, scoped_refptr<SessionAccessor> >
    SessionMap;

#endif  // CHROME_TEST_CHROMEDRIVER_SESSION_MAP_H_
