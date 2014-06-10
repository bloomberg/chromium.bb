// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/webkit_support.h"

#include <string>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "content/test/test_webkit_platform_support.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "url/url_util.h"

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "net/android/network_library.h"
#endif

#if defined(OS_MACOSX)
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace content {

namespace {

void EnableBlinkPlatformLogChannels(const std::string& channels) {
  if (channels.empty())
    return;
  base::StringTokenizer t(channels, ", ");
  while (t.GetNext())
    blink::enableLogChannel(t.token().c_str());
}

void ParseBlinkCommandLineArgumentsForUnitTests() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  EnableBlinkPlatformLogChannels(
      command_line.GetSwitchValueASCII(switches::kBlinkPlatformLogChannels));
}

class TestEnvironment {
 public:
#if defined(OS_ANDROID)
  // Android UI message loop goes through Java, so don't use it in tests.
  typedef base::MessageLoop MessageLoopType;
#else
  typedef base::MessageLoopForUI MessageLoopType;
#endif

  TestEnvironment() {
    main_message_loop_.reset(new MessageLoopType);

    // TestWebKitPlatformSupport must be instantiated after MessageLoopType.
    webkit_platform_support_.reset(new TestWebKitPlatformSupport);
  }

  ~TestEnvironment() {
  }

  TestWebKitPlatformSupport* webkit_platform_support() const {
    return webkit_platform_support_.get();
  }

 private:
  scoped_ptr<MessageLoopType> main_message_loop_;
  scoped_ptr<TestWebKitPlatformSupport> webkit_platform_support_;
};

TestEnvironment* test_environment;

}  // namespace

void SetUpTestEnvironmentForUnitTests() {
  ParseBlinkCommandLineArgumentsForUnitTests();

  blink::WebRuntimeFeatures::enableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::enableTestOnlyFeatures(true);

#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  net::android::RegisterNetworkLibrary(env);
#endif

#if defined(OS_MACOSX)
  mock_cr_app::RegisterMockCrApp();
#endif

#if defined(OS_WIN)
  gfx::InitDeviceScaleFactor(1.0f);
#endif

  // Explicitly initialize the GURL library before spawning any threads.
  // Otherwise crash may happend when different threads try to create a GURL
  // at same time.
  url::Initialize();
  test_environment = new TestEnvironment;
}

void TearDownTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  base::RunLoop().RunUntilIdle();

  if (RunningOnValgrind())
    blink::WebCache::clear();
  delete test_environment;
  test_environment = NULL;
}

}  // namespace content
