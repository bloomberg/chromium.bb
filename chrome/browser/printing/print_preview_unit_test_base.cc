// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_unit_test_base.h"

#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"

PrintPreviewUnitTestBase::PrintPreviewUnitTestBase() {
}

PrintPreviewUnitTestBase::~PrintPreviewUnitTestBase() {
}

void PrintPreviewUnitTestBase::SetUp() {
  BrowserWithTestWindowTest::SetUp();

  testing_local_state_.reset(new TestingPrefService);
  testing_local_state_->SetUserPref(prefs::kPrintPreviewDisabled,
                                    Value::CreateBooleanValue(false));

  browser::RegisterLocalState(testing_local_state_.get());
  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  EXPECT_FALSE(testing_browser_process->local_state());
  testing_browser_process->SetLocalState(testing_local_state_.get());
}

void PrintPreviewUnitTestBase::TearDown() {
  EXPECT_EQ(testing_local_state_.get(), g_browser_process->local_state());
  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  testing_browser_process->SetLocalState(NULL);
  BrowserWithTestWindowTest::TearDown();
}
