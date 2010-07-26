// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
#define CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
#pragma once

#include <string>

#include "build/build_config.h"

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "base/stats_table.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/test/test_suite.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/scoped_ole_initializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/testing_browser_process.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"

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

// In many cases it may be not obvious that a test makes a real DNS lookup.
// We generally don't want to rely on external DNS servers for our tests,
// so this host resolver procedure catches external queries.
class WarningHostResolverProc : public net::HostResolverProc {
 public:
  WarningHostResolverProc() : HostResolverProc(NULL) {}

  virtual int Resolve(const std::string& host,
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
};

class ChromeTestSuite : public TestSuite {
 public:
  ChromeTestSuite(int argc, char** argv)
      : TestSuite(argc, argv),
        stats_table_(NULL),
        created_user_data_dir_(false) {
  }

 protected:

  virtual void Initialize() {
    base::ScopedNSAutoreleasePool autorelease_pool;

    TestSuite::Initialize();

    chrome::RegisterChromeSchemes();
    host_resolver_proc_ = new WarningHostResolverProc();
    scoped_host_resolver_proc_.Init(host_resolver_proc_.get());

    chrome::RegisterPathProvider();
    app::RegisterPathProvider();
    g_browser_process = new TestingBrowserProcess;

    // Notice a user data override, and otherwise default to using a custom
    // user data directory that lives alongside the current app.
    // NOTE: The user data directory will be erased before each UI test that
    //       uses it, in order to ensure consistency.
    FilePath user_data_dir =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kUserDataDir);
    if (user_data_dir.empty() &&
        file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_test_"),
                                          &user_data_dir)) {
      user_data_dir = user_data_dir.AppendASCII("test_user_data");
      created_user_data_dir_ = true;
    }
    if (!user_data_dir.empty())
      PathService::Override(chrome::DIR_USER_DATA, user_data_dir);

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
    ResourceBundle::InitSharedInstance(L"en-US");

    // initialize the global StatsTable for unit_tests (make sure the file
    // doesn't exist before opening it so the test gets a clean slate)
    stats_filename_ = "unit_tests";
    std::string pid_string = StringPrintf("-%d", base::GetCurrentProcId());
    stats_filename_ += pid_string;
    RemoveSharedMemoryFile(stats_filename_);
    stats_table_ = new StatsTable(stats_filename_, 20, 200);
    StatsTable::set_current(stats_table_);
  }

  virtual void Shutdown() {
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

    // Delete the test_user_data dir recursively
    // NOTE: user_data_dir will be deleted only if it was automatically
    // created.
    FilePath user_data_dir;
    if (created_user_data_dir_ &&
        PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) &&
        !user_data_dir.empty()) {
      file_util::Delete(user_data_dir, true);
      file_util::Delete(user_data_dir.DirName(), false);
    }
    TestSuite::Shutdown();
  }

  void SetBrowserDirectory(const FilePath& browser_dir) {
    browser_dir_ = browser_dir;
  }

  StatsTable* stats_table_;
  // The name used for the stats file so it can be cleaned up on posix during
  // test shutdown.
  std::string stats_filename_;

  // Alternative path to browser binaries.
  FilePath browser_dir_;

  ScopedOleInitializer ole_initializer_;
  scoped_refptr<WarningHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;

  // Flag indicating whether user_data_dir was automatically created or not.
  bool created_user_data_dir_;
};

#endif  // CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
