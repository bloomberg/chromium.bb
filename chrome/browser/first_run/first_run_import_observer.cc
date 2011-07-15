// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_import_observer.h"

#include "base/message_loop.h"
#include "content/common/result_codes.h"

FirstRunImportObserver::FirstRunImportObserver()
    : loop_running_(false), import_result_(ResultCodes::NORMAL_EXIT) {
}

FirstRunImportObserver::~FirstRunImportObserver() {
}

void FirstRunImportObserver::RunLoop() {
  loop_running_ = true;
  MessageLoop::current()->Run();
}

void FirstRunImportObserver::Finish() {
  if (loop_running_)
    MessageLoop::current()->Quit();
}

void FirstRunImportObserver::ImportCompleted() {
  import_result_ = ResultCodes::NORMAL_EXIT;
  Finish();
}
