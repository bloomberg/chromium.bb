// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "content/browser/media/media_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "webkit/renderer/media/crypto/key_systems.h"

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

// Common test expectations.
static const char kKeyError[] = "KEYERROR";

namespace content {

class EncryptedMediaTest : public testing::WithParamInterface<const char*>,
                           public content::MediaBrowserTest {
 public:
  void TestSimplePlayback(const char* encrypted_media, const char* media_type,
                          const char* key_system, const char* expectation) {
    RunEncryptedMediaTest("encrypted_media_player.html", encrypted_media,
                          media_type, key_system, expectation);
  }

  void TestFrameSizeChange(const char* key_system, const char* expectation) {
    RunEncryptedMediaTest("encrypted_frame_size_change.html",
                          "frame_size_change-av-enc-v.webm", kWebMAudioVideo,
                          key_system, expectation);
  }

  void RunEncryptedMediaTest(const char* html_page, const char* media_file,
                             const char* media_type, const char* key_system,
                             const char* expectation) {
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("mediafile", media_file));
    query_params.push_back(std::make_pair("mediatype", media_type));
    query_params.push_back(std::make_pair("keysystem", key_system));
    RunMediaTestPage(html_page, &query_params, expectation, true);
  }

 protected:
#if defined(ENABLE_PEPPER_CDMS)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    RegisterPepperCdm(command_line, kClearKeyCdmAdapterFileName,
                      kExternalClearKeyKeySystem);
  }

  virtual void RegisterPepperCdm(CommandLine* command_line,
                                 const std::string& adapter_name,
                                 const std::string& key_system) {
    // Append the switch to register the Clear Key CDM Adapter.
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    base::FilePath plugin_lib = plugin_dir.AppendASCII(adapter_name);
    EXPECT_TRUE(file_util::PathExists(plugin_lib));
    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL("#CDM#0.1.0.0;"));
#if defined(OS_WIN)
    pepper_plugin.append(ASCIIToWide(webkit_media::GetPepperType(key_system)));
#else
    pepper_plugin.append(webkit_media::GetPepperType(key_system));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
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
  void TestSimplePlayback(const char* encrypted_media, const char* media_type,
                          const char* key_system) {
    // TODO(shadi): Remove after bots upgrade to precise.
    // Don't run on lucid bots since the CDM is not compatible (glibc < 2.14)
#if defined(OS_LINUX)
    if(strcmp(gnu_get_libc_version(), "2.11.1") == 0) {
      LOG(INFO) << "Skipping test; not supported on glibc version: "
          << gnu_get_libc_version();
      return;
    }
#endif  // defined(OS_LINUX)
    EncryptedMediaTest::TestSimplePlayback(encrypted_media, media_type,
                                           key_system, kKeyError);
    bool receivedKeyMessage = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        "window.domAutomationController.send(video.receivedKeyMessage);",
        &receivedKeyMessage));
    ASSERT_TRUE(receivedKeyMessage);
  }

 protected:
#if defined(ENABLE_PEPPER_CDMS)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    RegisterPepperCdm(command_line, kWidevineCdmAdapterFileName,
                      kWidevineKeySystem);
  }
#endif  // defined(ENABLE_PEPPER_CDMS)
};
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

INSTANTIATE_TEST_CASE_P(ClearKey, EncryptedMediaTest,
                        ::testing::Values(kClearKeyKeySystem));

// External Clear Key is currently only used on platforms that use Pepper CDMs.
#if defined(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(ExternalClearKey, EncryptedMediaTest,
                        ::testing::Values(kExternalClearKeyKeySystem));
#endif

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, InvalidKeySystem) {
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                     "com.example.invalid", "GENERATE_KEY_REQUEST_EXCEPTION");
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
  TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo,GetParam(),
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

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly, GetParam(),
                     kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly, GetParam(),
                     kEnded);
}
#endif

// Run only when WV CDM is available.
#if defined(WIDEVINE_CDM_AVAILABLE)
// See http://crbug.com/237636.
#if !defined(DISABLE_WIDEVINE_CDM_BROWSERTESTS)
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly, kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo,
                     kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                     kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly,
                     kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo,
                     kWidevineKeySystem);
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_VideoOnly_MP4) {
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly,
                     kWidevineKeySystem);
}

IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, Playback_AudioOnly_MP4) {
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly,
                     kWidevineKeySystem);
}
#endif  // defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
#endif  // !defined(DISABLE_WIDEVINE_CDM_BROWSERTESTS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

}  // namespace content
