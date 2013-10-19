// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/media/media_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test_utils.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(OS_LINUX)
#include <gnu/libc-version.h>
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(OS_LINUX)

#if defined(ENABLE_PEPPER_CDMS)
// Platform-specific filename relative to the chrome executable.
const char kClearKeyCdmAdapterFileName[] =
#if defined(OS_MACOSX)
    "clearkeycdmadapter.plugin";
#elif defined(OS_WIN)
    "clearkeycdmadapter.dll";
#elif defined(OS_POSIX)
    "libclearkeycdmadapter.so";
#endif

const char kClearKeyCdmPluginMimeType[] = "application/x-ppapi-clearkey-cdm";
#endif  // defined(ENABLE_PEPPER_CDMS)

// Available key systems.
const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

// Supported media types.
const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";
#if defined(USE_PROPRIETARY_CODECS)
const char kMP4AudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"";
const char kMP4VideoOnly[] = "video/mp4; codecs=\"avc1.4D4041\"";
#endif  // defined(USE_PROPRIETARY_CODECS)

// EME-specific test results and errors.
const char kEmeKeyError[] = "KEYERROR";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";

// The type of video src used to load media.
enum SrcType {
  SRC,
  MSE
};

// MSE is available on all desktop platforms and on Android 4.1 and later.
static bool IsMSESupported() {
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16) {
    LOG(INFO) << "MSE is only supported in Android 4.1 and later.";
    return false;
  }
#endif  // defined(OS_ANDROID)
  return true;
}

// Base class for encrypted media tests.
class EncryptedMediaTestBase : public MediaBrowserTest {
 public:
  EncryptedMediaTestBase() : is_pepper_cdm_registered_(false) {}

  bool IsExternalClearKey(const char* key_system) {
    return (strcmp(key_system, kExternalClearKeyKeySystem) == 0);
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  bool IsWidevine(const char* key_system) {
    return (strcmp(key_system, kWidevineKeySystem) == 0);
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

  void RunEncryptedMediaTest(const char* html_page,
                             const char* media_file,
                             const char* media_type,
                             const char* key_system,
                             SrcType src_type,
                             const char* expectation) {
    if (src_type == MSE && !IsMSESupported()) {
      LOG(INFO) << "Skipping test - MSE not supported.";
      return;
    }

    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("mediafile", media_file));
    query_params.push_back(std::make_pair("mediatype", media_type));
    query_params.push_back(std::make_pair("keysystem", key_system));
    if (src_type == MSE)
      query_params.push_back(std::make_pair("usemse", "1"));
    RunMediaTestPage(html_page, &query_params, expectation, true);
  }

  void RunSimpleEncryptedMediaTest(const char* media_file,
                                   const char* media_type,
                                   const char* key_system,
                                   SrcType src_type) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system)) {
      // Tests that the following happen after trying to play encrypted media:
      // - webkitneedkey event is fired.
      // - webkitGenerateKeyRequest() does not fail.
      // - webkitkeymessage is fired.
      // - webkitAddKey() triggers a WebKitKeyError since no real key is added.
      // TODO(shadi): Remove after bots upgrade to precise.
      // Don't run on lucid bots since the CDM is not compatible (glibc < 2.14)
#if defined(OS_LINUX)
      if (strcmp(gnu_get_libc_version(), "2.11.1") == 0) {
        LOG(INFO) << "Skipping test; not supported on glibc version: "
            << gnu_get_libc_version();
        return;
      }
#endif  // defined(OS_LINUX)

      RunEncryptedMediaTest("encrypted_media_player.html", media_file,
                            media_type, key_system, src_type, kEmeKeyError);

      bool receivedKeyMessage = false;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
          browser()->tab_strip_model()->GetActiveWebContents(),
          "window.domAutomationController.send(video.receivedKeyMessage);",
          &receivedKeyMessage));
      EXPECT_TRUE(receivedKeyMessage);
      return;
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

    RunEncryptedMediaTest("encrypted_media_player.html", media_file,
                          media_type, key_system, src_type, kEnded);
  }

 protected:
  // We want to fail quickly when a test fails because an error is encountered.
  virtual void AddWaitForTitles(content::TitleWatcher* title_watcher) OVERRIDE {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(ASCIIToUTF16(kEmeKeyError));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if defined(OS_ANDROID)
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
    // Disable GPU video decoder on ChromeOS since these tests run on
    // linux_chromeos which does not support GPU video decoder.
    command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
#endif  // defined(OS_CHROMEOS)
  }

  void SetUpCommandLineForKeySystem(const char* key_system,
                                    CommandLine* command_line) {
#if defined(ENABLE_PEPPER_CDMS)
    if (IsExternalClearKey(key_system)) {
      RegisterPepperCdm(command_line, kClearKeyCdmAdapterFileName,
                        kExternalClearKeyKeySystem);
    }
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    else if (IsWidevine(key_system)) {
      RegisterPepperCdm(command_line, kWidevineCdmAdapterFileName,
                        kWidevineKeySystem);
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
#endif  // defined(ENABLE_PEPPER_CDMS)
  }

 private:
#if defined(ENABLE_PEPPER_CDMS)
  void RegisterPepperCdm(CommandLine* command_line,
                         const std::string& adapter_name,
                         const std::string& key_system) {
    DCHECK(!is_pepper_cdm_registered_)
        << "RegisterPepperCdm() can only be called once.";
    is_pepper_cdm_registered_ = true;

    // Append the switch to register the Clear Key CDM Adapter.
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    base::FilePath plugin_lib = plugin_dir.AppendASCII(adapter_name);
    EXPECT_TRUE(base::PathExists(plugin_lib)) << plugin_lib.value();
    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL("#CDM#0.1.0.0;"));
#if defined(OS_WIN)
    pepper_plugin.append(ASCIIToWide(GetPepperType(key_system)));
#else
    pepper_plugin.append(GetPepperType(key_system));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }

  // Adapted from key_systems.cc.
  std::string GetPepperType(const std::string& key_system) {
    if (key_system == kExternalClearKeyKeySystem)
      return kClearKeyCdmPluginMimeType;
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (key_system == kWidevineKeySystem)
      return kWidevineCdmPluginMimeType;
#endif  // WIDEVINE_CDM_AVAILABLE

    NOTREACHED();
    return "";
  }
#endif  // defined(ENABLE_PEPPER_CDMS)

  bool is_pepper_cdm_registered_;
};

#if defined(ENABLE_PEPPER_CDMS)
// Tests encrypted media playback using ExternalClearKey key system.
class ECKEncryptedMediaTest : public EncryptedMediaTestBase {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kExternalClearKeyKeySystem, command_line);
  }
};

#if defined(WIDEVINE_CDM_AVAILABLE)
// Tests encrypted media playback using Widevine key system.
class WVEncryptedMediaTest : public EncryptedMediaTestBase {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kWidevineKeySystem, command_line);
  }
};
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // defined(ENABLE_PEPPER_CDMS)

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - bool: True to load media using MSE, otherwise use src.
//
// Note: Only parameterized (*_P) tests can be used. Non-parameterized (*_F)
// tests will crash at GetParam(). To add non-parameterized tests, use
// EncryptedMediaTestBase or one of its subclasses (e.g. WVEncryptedMediaTest).
class EncryptedMediaTest : public EncryptedMediaTestBase,
  public testing::WithParamInterface<std::tr1::tuple<const char*, SrcType> > {
 public:
  const char* CurrentKeySystem() {
    return std::tr1::get<0>(GetParam());
  }

  SrcType CurrentSourceType() {
    return std::tr1::get<1>(GetParam());
  }

  void TestSimplePlayback(const char* encrypted_media, const char* media_type) {
    RunSimpleEncryptedMediaTest(
        encrypted_media, media_type, CurrentKeySystem(), CurrentSourceType());
  }

  void TestFrameSizeChange() {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(CurrentKeySystem())) {
      LOG(INFO) << "FrameSizeChange test cannot run with Widevine.";
      return;
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    RunEncryptedMediaTest("encrypted_frame_size_change.html",
                          "frame_size_change-av-enc-v.webm", kWebMAudioVideo,
                          CurrentKeySystem(), CurrentSourceType(), kEnded);
  }

  void TestConfigChange() {
    if (CurrentSourceType() != MSE || !IsMSESupported()) {
      LOG(INFO) << "Skipping test - config change test requires MSE.";
      return;
    }
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(CurrentKeySystem())) {
      LOG(INFO) << "ConfigChange test cannot run with Widevine.";
      return;
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("keysystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runencrypted", "1"));
    RunMediaTestPage("mse_config_change.html", &query_params, kEnded, true);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(CurrentKeySystem(), command_line);
  }
};

using ::testing::Combine;
using ::testing::Values;

#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(SRC_ClearKey, EncryptedMediaTest,
    Combine(Values(kClearKeyKeySystem), Values(SRC)));
#endif  // !defined(OS_ANDROID)

INSTANTIATE_TEST_CASE_P(MSE_ClearKey, EncryptedMediaTest,
    Combine(Values(kClearKeyKeySystem), Values(MSE)));

// External Clear Key is currently only used on platforms that use Pepper CDMs.
#if defined(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey, EncryptedMediaTest,
    Combine(Values(kExternalClearKeyKeySystem), Values(SRC)));
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey, EncryptedMediaTest,
    Combine(Values(kExternalClearKeyKeySystem), Values(MSE)));
#endif // defined(ENABLE_PEPPER_CDMS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// This test doesn't fully test playback with Widevine. So we only run Widevine
// test with MSE (no SRC) to reduce test time. Also, on Android EME only works
// with MSE and we cannot run this test with SRC.
INSTANTIATE_TEST_CASE_P(MSE_Widevine, EncryptedMediaTest,
    Combine(Values(kWidevineKeySystem), Values(MSE)));
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo) {
  TestConfigChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  // Times out on Windows XP. http://crbug.com/171937
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif
  TestFrameSizeChange();
}

#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    LOG(INFO) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    LOG(INFO) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly);
}
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// The parent key system cannot be used in generateKeyRequest.
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, ParentThrowsException) {
  RunEncryptedMediaTest("encrypted_media_player.html",
                        "bear-a-enc_a.webm",
                        kWebMAudioOnly,
                        "com.widevine",
                        MSE,
                        kEmeNotSupportedError);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#if defined(ENABLE_PEPPER_CDMS)
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest,
                       ExternalClearKeyInitializeCDMFail) {
  RunEncryptedMediaTest("encrypted_media_player.html",
                        "bear-a-enc_a.webm",
                        kWebMAudioOnly,
                        "org.chromium.externalclearkey.initializefail",
                        SRC,
                        kEmeKeyError);
}
#endif  // defined(ENABLE_PEPPER_CDMS)
