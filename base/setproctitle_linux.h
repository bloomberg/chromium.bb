// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SETPROCTITLE_LINUX_H_
#define BASE_SETPROCTITLE_LINUX_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Set the process title that will show in "ps" and similar tools. Takes
// printf-style format string and arguments. After calling setproctitle()
// the original main() argv[] array should not be used. By default, the
// original argv[0] is prepended to the format; this can be disabled by
// including a '-' as the first character of the format string.
void setproctitle(const char* fmt, ...);

// Initialize state needed for setproctitle() on Linux. Pass the argv pointer
// from main() to setproctitle_init() before calling setproctitle().
void setproctitle_init(char** main_argv);

#ifdef __cplusplus
}
#endif

#endif  // BASE_SETPROCTITLE_LINUX_H_
