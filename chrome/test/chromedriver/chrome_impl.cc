// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_impl.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/test/chromedriver/status.h"

ChromeImpl::ChromeImpl(base::ProcessHandle process,
                       base::ScopedTempDir* user_data_dir)
    : process_(process) {
  if (user_data_dir->IsValid()) {
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  }
}

ChromeImpl::~ChromeImpl() {
  base::CloseProcessHandle(process_);
}

Status ChromeImpl::Quit() {
  if (!base::KillProcess(process_, 0, true))
    return Status(kUnknownError, "cannot kill Chrome");
  return Status(kOk);
}
