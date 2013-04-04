// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_LOGGING_H__
#define GESTURES_LOGGING_H__

#include "gestures.h"

#define Assert(condition) \
  do { \
    if (!(condition)) \
      Err("Assertion '" #condition "' failed"); \
  } while(false)

#define AssertWithReturn(condition) \
  do { \
    if (!(condition)) { \
      Err("Assertion '" #condition "' failed"); \
      return; \
    } \
  } while(false)

#define AssertWithReturnValue(condition, returnValue) \
  do { \
    if (!(condition)) { \
      Err("Assertion '" #condition "' failed"); \
      return (returnValue); \
    } \
  } while(false)

#define Log(format, ...) \
  gestures_log(GESTURES_LOG_INFO, "INFO:%s:%d:" format "\n", \
               __FILE__, __LINE__, ## __VA_ARGS__)
#define Err(format, ...) \
  gestures_log(GESTURES_LOG_ERROR, "ERROR:%s:%d:" format "\n", \
               __FILE__, __LINE__, ## __VA_ARGS__)

#endif  // GESTURES_LOGGING_H__
