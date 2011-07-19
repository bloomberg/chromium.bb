// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains code shared by all browsing data browsertests.

#ifndef CHROME_BROWSER_BROWSING_DATA_HELPER_BROWSERTEST_H_
#define CHROME_BROWSER_BROWSING_DATA_HELPER_BROWSERTEST_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"

// This template can be used for the StartFetching methods of the browsing data
// helper classes. It is supposed to be instantiated with the respective
// browsing data info type.
template <typename T>
class BrowsingDataHelperCallback {
 public:
  BrowsingDataHelperCallback()
      : has_result_(false) {
  }

  const std::vector<T>& result() {
    MessageLoop::current()->Run();
    DCHECK(has_result_);
    return result_;
  }

  void callback(const std::vector<T>& info) {
    result_ = info;
    has_result_ = true;
    MessageLoop::current()->Quit();
  }

 private:
  bool has_result_;
  std::vector<T> result_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataHelperCallback);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_HELPER_BROWSERTEST_H_
