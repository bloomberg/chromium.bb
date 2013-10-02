// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_KEEP_ALIVE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_KEEP_ALIVE_SERVICE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/app_list/keep_alive_service.h"

class FakeKeepAliveService : public KeepAliveService {
 public:
  FakeKeepAliveService();

  bool is_keeping_alive() const { return is_keeping_alive_; }

  // KeepAliveService overrides.
  virtual void EnsureKeepAlive() OVERRIDE;
  virtual void FreeKeepAlive() OVERRIDE;

 private:
  bool is_keeping_alive_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_KEEP_ALIVE_SERVICE_H_
