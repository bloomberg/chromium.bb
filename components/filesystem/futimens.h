// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILES_FUTIMENS_H_
#define SERVICES_FILES_FUTIMENS_H_

#include <sys/stat.h>

#include "build/build_config.h"

// For Android: The NDK/C library we currently build against doesn't have
// |futimens()|, but has |utimensat()|. Provide the former (in terms of the
// latter) for now. Remove this file and futimens_android.cc when |futimens()|
// becomes generally available (see
// https://android-review.googlesource.com/#/c/63321/).

#if defined(OS_ANDROID)
extern "C" {

int futimens(int fd, const struct timespec times[2]);

}  // extern "C"
#endif

#endif  // SERVICES_FILES_FUTIMENS_H_
