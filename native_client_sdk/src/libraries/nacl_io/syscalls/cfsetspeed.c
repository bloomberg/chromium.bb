/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap.h"

int cfsetspeed(struct termios *termios_p, speed_t speed) {
  termios_p->c_ispeed = speed;
  termios_p->c_ospeed = speed;
  return 0;
}
