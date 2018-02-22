// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mash_service_registry.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"

namespace {

void VerifyProcessGroupOnIOThread() {
  EXPECT_TRUE(content::HasValidProcessForProcessGroup(
      mash_service_registry::kAshAndUiProcessGroup));
}

}  // namespace

using MashServiceRegistryTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MashServiceRegistryTest, AshAndUiInSameProcess) {
  // Test only applies to mash (out-of-process ash).
  if (chromeos::GetAshConfig() != ash::Config::MASH)
    return;

  // Process group information is owned by the IO thread.
  base::RunLoop run_loop;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&VerifyProcessGroupOnIOThread), run_loop.QuitClosure());
  run_loop.Run();
}
