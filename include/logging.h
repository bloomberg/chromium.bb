// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_LOGGING_H__
#define GESTURES_LOGGING_H__

// TODO(adlr): Let clients specify style of logging rather than using #defines

#ifndef XLOGGING

#include <stdio.h>

#define Log(format, ...) \
  fprintf(stderr, "%s:%d:" format "\n", __FILE__, __LINE__, ## __VA_ARGS__)
#else

extern "C" {
#define bool bool_
#define class class_
#define new new_
#define private private_
#include <xorg/xf86.h>
#undef bool
#undef class
#undef new
#undef private
}

#define Log(format, ...) \
  xf86MsgVerb(X_NONE, 0, "%s:%d:" format "\n", \
              __FILE__, __LINE__, ## __VA_ARGS__)
#endif

#endif  // GESTURES_LOGGING_H__
