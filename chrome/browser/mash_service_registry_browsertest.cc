// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mash_service_registry.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/test/browser_test_utils.h"

namespace {

void VerifyProcessGroupOnIOThread() {
  // Ash and ui are in the same process group.
  std::map<std::string, base::WeakPtr<content::UtilityProcessHost>>* groups =
      content::GetServiceManagerProcessGroups();
  ASSERT_TRUE(groups);
  ASSERT_TRUE(
      base::ContainsKey(*groups, mash_service_registry::kAshAndUiProcessGroup));

  // The process group has a process host.
  base::WeakPtr<content::UtilityProcessHost> host =
      groups->at(mash_service_registry::kAshAndUiProcessGroup);
  ASSERT_TRUE(host);

  // The host is associated with a real process.
  EXPECT_NE(base::kNullProcessHandle, host->GetData().handle);
}

}  // namespace

using MashServiceRegistryTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(MashServiceRegistryTest, AshAndUiInSameProcess) {
  // Test only applies to --mash (out-of-process ash).
  if (chromeos::GetAshConfig() != ash::Config::MASH)
    return;

  // Process group information is owned by the IO thread.
  base::RunLoop run_loop;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&VerifyProcessGroupOnIOThread), run_loop.QuitClosure());
  run_loop.Run();
}
