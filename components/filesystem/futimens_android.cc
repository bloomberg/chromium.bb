// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/futimens.h"

extern "C" {

int futimens(int fd, const struct timespec times[2]) {
  return utimensat(fd, nullptr, times, 0);
}

}  // extern "C"
