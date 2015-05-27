// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_thread_bundle.h"

#include "content/public/test/test_browser_thread_bundle.h"

namespace web {

namespace {

int TestBrowserThreadBundleOptionsFromTestWebThreadBundleOptions(int options) {
  int result = 0;
  if (options & TestWebThreadBundle::IO_MAINLOOP)
    result |= content::TestBrowserThreadBundle::IO_MAINLOOP;
  if (options & TestWebThreadBundle::REAL_DB_THREAD)
    result |= content::TestBrowserThreadBundle::REAL_DB_THREAD;
  if (options & TestWebThreadBundle::REAL_FILE_THREAD)
    result |= content::TestBrowserThreadBundle::REAL_FILE_THREAD;
  if (options & TestWebThreadBundle::REAL_FILE_USER_BLOCKING_THREAD)
    result |= content::TestBrowserThreadBundle::REAL_FILE_USER_BLOCKING_THREAD;
  if (options & TestWebThreadBundle::REAL_CACHE_THREAD)
    result |= content::TestBrowserThreadBundle::REAL_CACHE_THREAD;
  if (options & TestWebThreadBundle::REAL_IO_THREAD)
    result |= content::TestBrowserThreadBundle::REAL_IO_THREAD;
  return result;
}

}  // namespace

class TestWebThreadBundleImpl {
 public:
  explicit TestWebThreadBundleImpl(int options)
      : test_browser_thread_bundle_(
            TestBrowserThreadBundleOptionsFromTestWebThreadBundleOptions(
                options)) {}

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(TestWebThreadBundleImpl);
};

TestWebThreadBundle::TestWebThreadBundle()
    : impl_(new TestWebThreadBundleImpl(DEFAULT)) {
}

TestWebThreadBundle::TestWebThreadBundle(int options)
    : impl_(new TestWebThreadBundleImpl(options)) {
}

TestWebThreadBundle::~TestWebThreadBundle() {
}

}  // namespace web
