// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_unit_test_base.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"

PrintPreviewUnitTestBase::PrintPreviewUnitTestBase() {
}

PrintPreviewUnitTestBase::~PrintPreviewUnitTestBase() {
}

void PrintPreviewUnitTestBase::SetUp() {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePrintPreview);
  BrowserWithTestWindowTest::SetUp();

  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());
}
