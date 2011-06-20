// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_UTIL_H_
#define BASE_TEST_TEST_UTIL_H_
#pragma once

// Generic utilities used only by tests.

#include <locale.h>

#include <string>

namespace base {

#if defined(OS_POSIX)

// Sets the given |locale| on construction, and restores the previous locale
// on destruction.
class ScopedSetLocale {
 public:
  explicit ScopedSetLocale(const char* locale) {
    old_locale_ = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, locale);
  }
  ~ScopedSetLocale() {
    setlocale(LC_ALL, old_locale_.c_str());
  }

 private:
  std::string old_locale_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetLocale);
};

#endif

}  // namespace base

#endif  // BASE_TEST_TEST_UTIL_H_
