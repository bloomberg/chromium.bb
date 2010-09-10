// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/unit/chrome_test_suite.h"

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/stats_table.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/test_timeouts.h"
#include "chrome/test/testing_browser_process.h"

#if defined(OS_MACOSX)
#include "base/mac_util.h"
#endif

#if defined(OS_POSIX)
#include "base/shared_memory.h"
#endif

static void RemoveSharedMemoryFile(const std::string& filename) {
  // Stats uses SharedMemory under the hood. On posix, this results in a file
  // on disk.
#if defined(OS_POSIX)
  base::SharedMemory memory;
  memory.Delete(UTF8ToWide(filename));
#endif
}

WarningHostResolverProc::WarningHostResolverProc()
    : HostResolverProc(NULL) {
}

WarningHostResolverProc::~WarningHostResolverProc() {
}

int WarningHostResolverProc::Resolve(const std::string& host,
                                     net::AddressFamily address_family,
                                     net::HostResolverFlags host_resolver_flags,
                                     net::AddressList* addrlist,
                                     int* os_error) {
  const char* kLocalHostNames[] = {"localhost", "127.0.0.1"};
  bool local = false;

  if (host == net::GetHostName()) {
    local = true;
  } else {
    for (size_t i = 0; i < arraysize(kLocalHostNames); i++)
      if (host == kLocalHostNames[i]) {
        local = true;
        break;
      }
  }

  // Make the test fail so it's harder to ignore.
  // If you really need to make real DNS query, use
  // net::RuleBasedHostResolverProc and its AllowDirectLookup method.
  EXPECT_TRUE(local) << "Making external DNS lookup of " << host;

  return ResolveUsingPrevious(host, address_family, host_resolver_flags,
                              addrlist, os_error);
}

ChromeTestSuite::ChromeTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv),
      stats_table_(NULL) {
}

ChromeTestSuite::~ChromeTestSuite() {
}

void ChromeTestSuite::Initialize() {
  base::ScopedNSAutoreleasePool autorelease_pool;

  base::TestSuite::Initialize();

  chrome::RegisterChromeSchemes();
  host_resolver_proc_ = new WarningHostResolverProc();
  scoped_host_resolver_proc_.Init(host_resolver_proc_.get());

  chrome::RegisterPathProvider();
  app::RegisterPathProvider();
  g_browser_process = new TestingBrowserProcess;

  TestTimeouts::Initialize();

  if (!browser_dir_.empty()) {
    PathService::Override(base::DIR_EXE, browser_dir_);
    PathService::Override(base::DIR_MODULE, browser_dir_);
  }

#if defined(OS_MACOSX)
  // Look in the framework bundle for resources.
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  mac_util::SetOverrideAppBundlePath(path);
#endif

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ResourceBundle::InitSharedInstance("en-US");

  // initialize the global StatsTable for unit_tests (make sure the file
  // doesn't exist before opening it so the test gets a clean slate)
  stats_filename_ = "unit_tests";
  std::string pid_string = StringPrintf("-%d", base::GetCurrentProcId());
  stats_filename_ += pid_string;
  RemoveSharedMemoryFile(stats_filename_);
  stats_table_ = new StatsTable(stats_filename_, 20, 200);
  StatsTable::set_current(stats_table_);
}

void ChromeTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX)
  mac_util::SetOverrideAppBundle(NULL);
#endif

  delete g_browser_process;
  g_browser_process = NULL;

  // Tear down shared StatsTable; prevents unit_tests from leaking it.
  StatsTable::set_current(NULL);
  delete stats_table_;
  RemoveSharedMemoryFile(stats_filename_);

  base::TestSuite::Shutdown();
}
