// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/preload_check_test_util.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

PreloadCheckRunner::PreloadCheckRunner() : called_(false) {}
PreloadCheckRunner::~PreloadCheckRunner() {}

void PreloadCheckRunner::OnCheckComplete(PreloadCheck::Errors errors) {
  ASSERT_FALSE(called_);
  called_ = true;
  errors_ = errors;
  if (run_loop_)
    run_loop_->Quit();
}

void PreloadCheckRunner::Run(PreloadCheck* check) {
  check->Start(GetCallback());
}

void PreloadCheckRunner::RunUntilComplete(PreloadCheck* check) {
  Run(check);
  ASSERT_FALSE(called_);

  run_loop_ = base::MakeUnique<base::RunLoop>();
  run_loop_->Run();
  ASSERT_TRUE(called_);
}

PreloadCheck::ResultCallback PreloadCheckRunner::GetCallback() {
  return base::Bind(&PreloadCheckRunner::OnCheckComplete,
                    base::Unretained(this));
}

void PreloadCheckRunner::WaitForIdle() {
  run_loop_ = base::MakeUnique<base::RunLoop>();
  run_loop_->RunUntilIdle();
}

}  // namespace extensions
