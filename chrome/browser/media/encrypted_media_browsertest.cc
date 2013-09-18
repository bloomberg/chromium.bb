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

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(OS_LINUX)
#include <gnu/libc-version.h>
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(OS_LINUX)

#if defined(ENABLE_PEPPER_CDMS)
// Platform-specific filename relative to the chrome executable.
static const char kClearKeyCdmAdapterFileName[] =
#if defined(OS_MACOSX)
    "clearkeycdmadapter.plugin";
#elif defined(OS_WIN)
    "clearkeycdmadapter.dll";
#elif defined(OS_POSIX)
    "libclearkeycdmadapter.so";
#endif
#endif  // defined(ENABLE_PEPPER_CDMS)

// Available key systems.
static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

// Supported media types.
static const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
static const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
static const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";
static const char kMP4AudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"";
static const char kMP4VideoOnly[] = "video/mp4; codecs=\"avc1.4D4041\"";

// EME-specific test results and errors.
static const char kEmeGkrException[] = "GENERATE_KEY_REQUEST_EXCEPTION";
static const char kEmeKeyError[] = "KEYERROR";

// The type of video src used to load media.
enum SrcType {
  SRC,
  MSE
};

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - bool: True to load media using MSE, otherwise use src.
class EncryptedMediaTest : public MediaBrowserTest,
  public testing::WithParamInterface<std::tr1::tuple<const char*, SrcType> > {
 public:
  void TestSimplePlayback(const char* encrypted_media, const char* media_type,
      const std::tr1::tuple<const char*, SrcType> test_params,
      const char* expectation) {
    const char* key_system = std::tr1::get<0>(test_params);
    SrcType src_type = std::tr1::get<1>(test_params);
    RunEncryptedMediaTest("encrypted_media_player.html", encrypted_media,
                          media_type, key_system, src_type, expectation);
  }

  void TestMSESimplePlayback(const char* encrypted_media,
                             const char* media_type, const char* key_system,
                             const char* expectation) {
    RunEncryptedMediaTest("encrypted_media_player.html", encrypted_media,
                          media_type, key_system, MSE, expectation);
  }

  void TestFrameSizeChange(
      const std::tr1::tuple<const char*, SrcType> test_params,
      const char* expectation) {
    const char* key_system = std::tr1::get<0>(test_params);
    SrcType src_type = std::tr1::get<1>(test_params);
    RunEncryptedMediaTest("encrypted_frame_size_change.html",
                          "frame_size_change-av-enc-v.webm", kWebMAudioVideo,
                          key_system, src_type, expectation);
  }

  void TestConfigChange(const char* key_system, const char* expectation) {
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("keysystem", key_system));
    query_params.push_back(std::make_pair("runencrypted", "1"));
    RunMediaTestPage("mse_config_change.html", &query_params, expectation,
                     true);
  }

  void RunEncryptedMediaTest(const char* html_page, const char* media_file,
                             const char* media_type, const char* key_system,
                             SrcType src_type, const char* expectation) {
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("mediafile", media_file));
    query_params.push_back(std::make_pair("mediatype", media_type));
    query_params.push_back(std::make_pair("keysystem", key_system));
    if (src_type == MSE)
      query_params.push_back(std::make_pair("usemse", "1"));
    RunMediaTestPage(html_page, &query_params, expectation, true);
  }

 protected:
  // We want to fail quickly when a test fails because an error is encountered.
  virtual void AddWaitForTitles(content::TitleWatcher* title_watcher) OVERRIDE {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(ASCIIToUTF16(kEmeGkrException));
    title_watcher->AlsoWaitForTitle(ASCIIToUTF16(kEmeKeyError));
  }

#if defined(ENABLE_PEPPER_CDMS)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    RegisterPepperCdm(command_line, kClearKeyCdmAdapterFileName,
                      kExternalClearKeyKeySystem);
// Disable GPU video decoder on ChromeOS since these tests run on linux_chromeos
// which do not support GPU video decoder.
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(switches::kDisableAcceleratedVideoDecode);
#endif  // defined(OS_CHROMEOS)
  }

  void RegisterPepperCdm(CommandLine* command_line,
                         const std::string& adapter_name,
                         const std::string& key_system) {
    // Append the switch to register the Clear Key CDM Adapter.
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    base::FilePath plugin_lib = plugin_dir.AppendASCII(adapter_name);
    EXPECT_TRUE(base::PathExists(plugin_lib));
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
      return "application/x-ppapi-clearkey-cdm";
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (key_system == kWidevineKeySystem)
      return "application/x-ppapi-widevine-cdm";
#endif  // WIDEVINE_CDM_AVAILABLE

    NOTREACHED();
    return "";
  }
#endif  // defined(ENABLE_PEPPER_CDMS)
};

#if defined(WIDEVINE_CDM_AVAILABLE)
class WVEncryptedMediaTest : public EncryptedMediaTest {
 public:
  // Tests that the following happen after trying to play encrypted media:
  // - webkitneedkey event is fired.
  // - webkitGenerateKeyRequest() does not fail.
  // - webkitkeymessage is fired
  // - webkitAddKey() generates a WebKitKeyError since no real WV key is added.
  void TestMSESimplePlayback(const char* encrypted_media,
                             const char* media_type, const char* key_system) {
    // TODO(shadi): Remove after bots upgrade to precise.
    // Don't run on lucid bots since the CDM is not compatible (glibc < 2.14)
#if defined(OS_LINUX)
    if (strcmp(gnu_get_libc_version(), "2.11.1") == 0) {
      LOG(INFO) << "Skipping test; not supported on glibc version: "
          << gnu_get_libc_version();
      return;
    }
#endif  // defined(OS_LINUX)
    EncryptedMediaTest::TestMSESimplePlayback(encrypted_media, media_type,
                                              key_system, kEmeKeyError);
    bool receivedKeyMessage = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(video.receivedKeyMessage);",
        &receivedKeyMessage));
    ASSERT_TRUE(receivedKeyMessage);
  }

 protected:
#if defined(ENABLE_PEPPER_CDMS) && defined(WIDEVINE_CDM_IS_COMPONENT)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    RegisterPepperCdm(command_line, kWidevineCdmAdapterFileName,
                      kWidevineKeySystem);
  }
#endif  // defined(ENABLE_PEPPER_CDMS) && defined(WIDEVINE_CDM_IS_COMPONENT)
};
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

INSTANTIATE_TEST_CASE_P(ClearKey, EncryptedMediaTest,
    ::testing::Combine(
        ::testing::Values(kClearKeyKeySystem), ::testing::Values(SRC, MSE)));

// External Clear Key is currently only used on platforms that use Pepper CDMs.
#if defined(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(ExternalClearKey, EncryptedMediaTest,
    ::testing::Combine(
        ::testing::Values(kExternalClearKeyKeySystem),
        ::testing::Values(SRC, MSE)));

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, ConfigChangeVideo_ExternalClearKey) {
  TestConfigChange(kExternalClearKeyKeySystem, kEnded);
}
#endif // defined(ENABLE_PEPPER_CDMS)

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, ConfigChangeVideo_ClearKey) {
  TestConfigChange(kClearKeyKeySystem, kEnded);
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, InvalidKeySystem) {
  TestMSESimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                        "com.example.invalid", kEmeGkrException);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly, GetParam(), kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo, GetParam(),
                     kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo, GetParam(),
                     kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly, GetParam(),
                     kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo, GetParam(),
                     kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameChangeVideo) {
  // Times out on Windows XP. http://crbug.com/171937
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif
  TestFrameSizeChange(GetParam(), kEnded);
}

#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  std::tr1::tuple<const char*, SrcType> test_params = GetParam();
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (std::tr1::get<1>(test_params) != MSE) {
    LOG(INFO) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestMSESimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly,
                        std::tr1::get<0>(test_params), kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  std::tr1::tuple<const char*, SrcType> test_params = GetParam();
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (std::tr1::get<1>(test_params) != MSE) {
    LOG(INFO) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestMSESimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly,
                        std::tr1::get<0>(test_params), kEnded);
}
#endif  // defined(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, UnknownKeySystemThrowsException) {
  RunEncryptedMediaTest("encrypted_media_player.html", "bear-a-enc_a.webm",
                        kWebMAudioOnly, "com.example.foo", SRC,
                        kEmeGkrException);
}

// Run only when WV CDM is available.
#if defined(WIDEVINE_CDM_AVAILABLE)
// The parent key system cannot be used in generateKeyRequest.
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, WVParentThrowsException) {
  RunEncryptedMediaTest("encrypted_media_player.html", "bear-a-enc_a.webm",
                        kWebMAudioOnly, "com.widevine", SRC, kEmeGkrException);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestMSESimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly,
                        kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestMSESimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo,
                        kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestMSESimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                        kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestMSESimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly,
                        kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestMSESimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo,
                        kWidevineKeySystem);
}

#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoOnly_MP4) {
  TestMSESimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly,
                        kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_AudioOnly_MP4) {
  TestMSESimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly,
                        kWidevineKeySystem);
}
#endif  // defined(USE_PROPRIETARY_CODECS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
