// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_suite.h"

#if defined(OS_CHROMEOS)
#include <stdio.h>
#include <unistd.h>
#endif

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
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_launcher.h"
#include "extensions/common/extension_paths.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/android/chrome_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/android/ui_jni_registrar.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#if !defined(OS_IOS)
#include "base/mac/mac_util.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // !defined(OS_IOS)
#endif

#if defined(OS_POSIX)
#include "base/shared_memory.h"
#endif

namespace {

void RemoveSharedMemoryFile(const std::string& filename) {
  // Stats uses SharedMemory under the hood. On posix, this results in a file
  // on disk.
#if defined(OS_POSIX)
  base::SharedMemory memory;
  memory.Delete(filename);
#endif
}

bool IsCrosPythonProcess() {
#if defined(OS_CHROMEOS)
  char buf[80];
  int num_read = readlink(base::kProcSelfExe, buf, sizeof(buf) - 1);
  if (num_read == -1)
    return false;
  buf[num_read] = 0;
  const char kPythonPrefix[] = "/python";
  return !strncmp(strrchr(buf, '/'), kPythonPrefix, sizeof(kPythonPrefix) - 1);
#else
  return false;
#endif  // defined(OS_CHROMEOS)
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
                      int* os_error) OVERRIDE {
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
    // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
    browser_content_client_.reset(new chrome::ChromeContentBrowserClient());
    content_client_->set_browser_for_testing(browser_content_client_.get());
#endif
    content::SetContentClient(content_client_.get());

    SetUpHostResolver();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    if (g_browser_process) {
      delete g_browser_process;
      g_browser_process = NULL;
    }

    DCHECK_EQ(content_client_.get(), content::GetContentClient());
    // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
    browser_content_client_.reset();
#endif
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

  scoped_ptr<chrome::ChromeContentClient> content_client_;
  // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
  scoped_ptr<chrome::ChromeContentBrowserClient> browser_content_client_;
#endif

  scoped_refptr<LocalHostResolverProc> host_resolver_proc_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> scoped_host_resolver_proc_;

  DISALLOW_COPY_AND_ASSIGN(ChromeTestSuiteInitializer);
};

}  // namespace

ChromeTestSuite::ChromeTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {
}

ChromeTestSuite::~ChromeTestSuite() {
}

void ChromeTestSuite::Initialize() {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#if !defined(OS_IOS)
  chrome_browser_application_mac::RegisterBrowserCrApp();
#endif  // !defined(OS_IOS)
#endif

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  net::android::RegisterJni(base::android::AttachCurrentThread());
  ui::android::RegisterJni(base::android::AttachCurrentThread());
  chrome::android::RegisterJni(base::android::AttachCurrentThread());
#endif

  chrome::RegisterPathProvider();
  if (!browser_dir_.empty()) {
    PathService::Override(base::DIR_EXE, browser_dir_);
    PathService::Override(base::DIR_MODULE, browser_dir_);
  }

#if !defined(OS_IOS)
  extensions::RegisterPathProvider();

  if (!content::GetCurrentTestLauncherDelegate()) {
    // Only want to do this for unit tests. For browser tests, this won't create
    // the right object since TestChromeWebUIControllerFactory is used. That's
    // created and registered in ChromeBrowserMainParts as in normal startup.
    content::WebUIControllerFactory::RegisterFactory(
        ChromeWebUIControllerFactory::GetInstance());
  }
#endif

  // Disable external libraries load if we are under python process in
  // ChromeOS.  That means we are autotest and, if ASAN is used,
  // external libraries load crashes.
  content::ContentTestSuiteBase::set_external_libraries_enabled(
      !IsCrosPythonProcess());

  // Initialize after overriding paths as some content paths depend on correct
  // values for DIR_EXE and DIR_MODULE.
  content::ContentTestSuiteBase::Initialize();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Look in the framework bundle for resources.
  base::FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  base::mac::SetOverrideFrameworkBundlePath(path);
#endif

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
  base::FilePath resources_pack_path;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  PathService::Get(base::DIR_MODULE, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
#else
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
#endif
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_NONE);

  stats_filename_ = base::StringPrintf("unit_tests-%d",
                                       base::GetCurrentProcId());
  RemoveSharedMemoryFile(stats_filename_);
  stats_table_.reset(new base::StatsTable(stats_filename_, 20, 200));
  base::StatsTable::set_current(stats_table_.get());

  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new ChromeTestSuiteInitializer);
}

content::ContentClient* ChromeTestSuite::CreateClientForInitialization() {
  return new chrome::ChromeContentClient();
}

void ChromeTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif

  base::StatsTable::set_current(NULL);
  stats_table_.reset();
  RemoveSharedMemoryFile(stats_filename_);

  base::TestSuite::Shutdown();
}
