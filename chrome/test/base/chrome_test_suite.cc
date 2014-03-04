// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_suite.h"

#if defined(OS_CHROMEOS)
#include <stdio.h>
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/public/test/test_launcher.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/android/chrome_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#include "ui/gl/android/gl_jni_registrar.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/process/process_metrics.h"
#include "chromeos/chromeos_paths.h"
#endif

#if !defined(OS_IOS)
#include "ui/gl/gl_surface.h"
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
#include "base/memory/shared_memory.h"
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

// Initializes services needed by both unit tests and browser tests.
// See also ChromeUnitTestSuite for additional services created for unit tests.
class ChromeTestSuiteInitializer : public testing::EmptyTestEventListener {
 public:
  ChromeTestSuiteInitializer() {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    content_client_.reset(new ChromeContentClient);
    content::SetContentClient(content_client_.get());
    // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
    browser_content_client_.reset(new chrome::ChromeContentBrowserClient());
    content::SetBrowserClientForTesting(browser_content_client_.get());
    utility_content_client_.reset(new chrome::ChromeContentUtilityClient());
    content::SetUtilityClientForTesting(utility_content_client_.get());
#endif
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
    browser_content_client_.reset();
    utility_content_client_.reset();
#endif
    content_client_.reset();
    content::SetContentClient(NULL);
 }

 private:
  // Client implementations for the content module.
  scoped_ptr<ChromeContentClient> content_client_;
  // TODO(ios): Bring this back once ChromeContentBrowserClient is building.
#if !defined(OS_IOS)
  scoped_ptr<chrome::ChromeContentBrowserClient> browser_content_client_;
  scoped_ptr<chrome::ChromeContentUtilityClient> utility_content_client_;
#endif

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
  gfx::android::RegisterJni(base::android::AttachCurrentThread());
  net::android::RegisterJni(base::android::AttachCurrentThread());
  ui::android::RegisterJni(base::android::AttachCurrentThread());
  ui::gl::android::RegisterJni(base::android::AttachCurrentThread());
  chrome::android::RegisterJni(base::android::AttachCurrentThread());
#endif

  chrome::RegisterPathProvider();
#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif
  if (!browser_dir_.empty()) {
    PathService::Override(base::DIR_EXE, browser_dir_);
    PathService::Override(base::DIR_MODULE, browser_dir_);
  }

#if !defined(OS_IOS)
  extensions::RegisterPathProvider();

  extensions::ExtensionsClient::Set(
      extensions::ChromeExtensionsClient::GetInstance());

  // Only want to do this for unit tests.
  if (!content::GetCurrentTestLauncherDelegate()) {
    // For browser tests, this won't create the right object since
    // TestChromeWebUIControllerFactory is used. That's created and
    // registered in ChromeBrowserMainParts as in normal startup.
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

#if !defined(OS_IOS)
  // For browser tests, a full chrome instance is initialized which will set up
  // GLSurface itself. For unit tests, we need to set this up for them.
  if (!IsBrowserTestSuite())
    gfx::GLSurface::InitializeOneOffForTests();
#endif

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
  return new ChromeContentClient();
}

void ChromeTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif

  base::StatsTable::set_current(NULL);
  stats_table_.reset();
  RemoveSharedMemoryFile(stats_filename_);

  content::ContentTestSuiteBase::Shutdown();
}
