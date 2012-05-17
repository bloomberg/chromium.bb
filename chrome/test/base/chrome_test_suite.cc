// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_suite.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/common/content_paths.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

#if defined(OS_POSIX)
#include "base/shared_memory.h"
#endif

#include "ui/compositor/compositor_setup.h"

namespace {

void RemoveSharedMemoryFile(const std::string& filename) {
  // Stats uses SharedMemory under the hood. On posix, this results in a file
  // on disk.
#if defined(OS_POSIX)
  base::SharedMemory memory;
  memory.Delete(filename);
#endif
}

// In many cases it may be not obvious that a test makes a real DNS lookup.
// We generally don't want to rely on external DNS servers for our tests,
// so this host resolver procedure catches external queries and returns a failed
// lookup result.
class LocalHostResolverProc : public net::HostResolverProc {
 public:
  LocalHostResolverProc() : HostResolverProc(NULL) {}

  virtual int Resolve(const std::string& host,
                      net::AddressFamily address_family,
                      net::HostResolverFlags host_resolver_flags,
                      net::AddressList* addrlist,
                      int* os_error) {
    const char* kLocalHostNames[] = {"localhost", "127.0.0.1", "::1"};
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

    // To avoid depending on external resources and to reduce (if not preclude)
    // network interactions from tests, we simulate failure for non-local DNS
    // queries, rather than perform them.
    // If you really need to make an external DNS query, use
    // net::RuleBasedHostResolverProc and its AllowDirectLookup method.
    if (!local) {
      DVLOG(1) << "To avoid external dependencies, simulating failure for "
          "external DNS lookup of " << host;
      return net::ERR_NOT_IMPLEMENTED;
    }

    return ResolveUsingPrevious(host, address_family, host_resolver_flags,
                                addrlist, os_error);
  }

 private:
  virtual ~LocalHostResolverProc() {}
};

class ChromeTestSuiteInitializer : public testing::EmptyTestEventListener {
 public:
  ChromeTestSuiteInitializer() {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    DCHECK(!g_browser_process);
    g_browser_process = new TestingBrowserProcess;

    DCHECK(!content::GetContentClient());
    content_client_.reset(new chrome::ChromeContentClient);
    browser_content_client_.reset(new chrome::ChromeContentBrowserClient());
    content_client_->set_browser(browser_content_client_.get());
    content::SetContentClient(content_client_.get());

    SetUpHostResolver();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    if (g_browser_process) {
      delete g_browser_process;
      g_browser_process = NULL;
    }

    DCHECK_EQ(content_client_.get(), content::GetContentClient());
    browser_content_client_.reset();
    content_client_.reset();
    content::SetContentClient(NULL);

    TearDownHostResolver();
  }

 private:
  void SetUpHostResolver() {
    host_resolver_proc_ = new LocalHostResolverProc;
    scoped_host_resolver_proc_.reset(
        new net::ScopedDefaultHostResolverProc(host_resolver_proc_.get()));
  }

  void TearDownHostResolver() {
    scoped_host_resolver_proc_.reset();
    host_resolver_proc_ = NULL;
  }

  scoped_ptr<BrowserProcess> browser_process_;

  scoped_ptr<chrome::ChromeContentClient> content_client_;
  scoped_ptr<chrome::ChromeContentBrowserClient> browser_content_client_;

  scoped_refptr<LocalHostResolverProc> host_resolver_proc_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> scoped_host_resolver_proc_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTestSuiteInitializer);
};

}  // namespace

const char ChromeTestSuite::kLaunchAsBrowser[] = "as-browser";

ChromeTestSuite::ChromeTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

ChromeTestSuite::~ChromeTestSuite() {
}

void ChromeTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
  chrome_browser_application_mac::RegisterBrowserCrApp();
#endif

  base::TestSuite::Initialize();

  chrome::ChromeContentClient client_for_init;
  content::SetContentClient(&client_for_init);
  content::RegisterContentSchemes(false);
  content::SetContentClient(NULL);

  chrome::RegisterPathProvider();
  content::RegisterPathProvider();
  ui::RegisterPathProvider();

  if (!browser_dir_.empty()) {
    PathService::Override(base::DIR_EXE, browser_dir_);
    PathService::Override(base::DIR_MODULE, browser_dir_);
  }

#if defined(OS_MACOSX)
  // Look in the framework bundle for resources.
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::mac::SetOverrideFrameworkBundlePath(path);
#endif

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  FilePath resources_pack_path;
  PathService::Get(base::DIR_MODULE, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
  ResourceBundle::GetSharedInstance().AddDataPack(
      resources_pack_path, ui::SCALE_FACTOR_100P);

  // Mock out the compositor on platforms that use it.
  ui::SetupTestCompositor();

  stats_filename_ = base::StringPrintf("unit_tests-%d",
                                       base::GetCurrentProcId());
  RemoveSharedMemoryFile(stats_filename_);
  stats_table_.reset(new base::StatsTable(stats_filename_, 20, 200));
  base::StatsTable::set_current(stats_table_.get());

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ChromeTestSuiteInitializer);
}

void ChromeTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif

  base::StatsTable::set_current(NULL);
  stats_table_.reset();
  RemoveSharedMemoryFile(stats_filename_);

  base::TestSuite::Shutdown();
}
