// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "media/base/media_switches.h"
#include "webkit/media/crypto/key_systems.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(OS_LINUX)
#include <gnu/libc-version.h>
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(OS_LINUX)

// Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
static const char kClearKeyLibraryName[] = "clearkeycdmadapter.dll";
static const char kWidevineLibraryName[] = "widevinecdmadapter.dll";
#elif defined(OS_MACOSX)
static const char kClearKeyLibraryName[] = "clearkeycdmadapter.plugin";
static const char kWidevineLibraryName[] = "widevinecdmadapter.plugin";
#elif defined(OS_POSIX)
static const char kClearKeyLibraryName[] = "libclearkeycdmadapter.so";
static const char kWidevineLibraryName[] = "libwidevinecdmadapter.so";
#endif

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
const string16 kExpectedEnded = ASCIIToUTF16("ENDED");
const string16 kExpectedWebKitError = ASCIIToUTF16("WEBKITKEYERROR");

namespace content {

class EncryptedMediaTest : public testing::WithParamInterface<const char*>,
                           public ContentBrowserTest {
 public:
  void TestSimplePlayback(const char* encrypted_media, const char* media_type,
                          const char* key_system, const string16 expectation) {
    ASSERT_NO_FATAL_FAILURE(
        PlayEncryptedMedia("encrypted_media_player.html", encrypted_media,
                           media_type, key_system, expectation));
  }

  void TestFrameSizeChange(const char* key_system, const string16 expectation) {
    ASSERT_NO_FATAL_FAILURE(
        PlayEncryptedMedia("encrypted_frame_size_change.html",
                           "frame_size_change-av-enc-v.webm", kWebMAudioVideo,
                           key_system, expectation));
  }

  void PlayEncryptedMedia(const char* html_page, const char* media_file,
                          const char* media_type, const char* key_system,
                          const string16 expectation) {
    // TODO(shadi): Add non-HTTP tests once src is supported for EME.
    ASSERT_TRUE(test_server()->Start());

    const string16 kError = ASCIIToUTF16("ERROR");
    const string16 kFailed = ASCIIToUTF16("FAILED");
    GURL player_gurl = test_server()->GetURL(base::StringPrintf(
        "files/media/%s?keysystem=%s&mediafile=%s&mediatype=%s", html_page,
        key_system, media_file, media_type));
    TitleWatcher title_watcher(shell()->web_contents(), expectation);
    title_watcher.AlsoWaitForTitle(kError);
    title_watcher.AlsoWaitForTitle(kFailed);

    NavigateToURL(shell(), player_gurl);

    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expectation, final_title);

    if (final_title == kFailed) {
      std::string fail_message;
      EXPECT_TRUE(ExecuteScriptAndExtractString(
          shell()->web_contents(),
          "window.domAutomationController.send(failMessage);",
          &fail_message));
      LOG(INFO) << "Test failed: " << fail_message;
    }
  }

 protected:
  // Registers any CDM plugins not registered by default.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    RegisterPepperPlugin(command_line, kClearKeyLibraryName,
                         kExternalClearKeyKeySystem);
  }

  virtual void RegisterPepperPlugin(CommandLine* command_line,
                                    const std::string& library_name,
                                    const std::string& key_system) {
    // Append the switch to register the Clear Key CDM Adapter.
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
#if defined(OS_WIN)
    base::FilePath plugin_lib = plugin_dir.Append(ASCIIToWide(library_name));
#else
    base::FilePath plugin_lib = plugin_dir.Append(library_name);
#endif
    EXPECT_TRUE(file_util::PathExists(plugin_lib));
    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL("#Key CDM#0.1.0.0;"));
#if defined(OS_WIN)
    pepper_plugin.append(ASCIIToWide(webkit_media::GetPluginType(key_system)));
#else
    pepper_plugin.append(webkit_media::GetPluginType(key_system));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }
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
                                           key_system, kExpectedWebKitError);
    bool receivedKeyMessage = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(),
        "window.domAutomationController.send(video.receivedKeyMessage);",
        &receivedKeyMessage));
    ASSERT_TRUE(receivedKeyMessage);
  }

 protected:
  // Registers any CDM plugins not registered by default.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
    RegisterPepperPlugin(command_line, kWidevineLibraryName,
                         kWidevineKeySystem);
  }
};
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

INSTANTIATE_TEST_CASE_P(ClearKey, EncryptedMediaTest,
                        ::testing::Values(kClearKeyKeySystem));

INSTANTIATE_TEST_CASE_P(ExternalClearKey, EncryptedMediaTest,
                        ::testing::Values(kExternalClearKeyKeySystem));

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, InvalidKeySystem) {
  const string16 kExpected = ASCIIToUTF16(
      StringToUpperASCII(std::string("GenerateKeyRequestException")));
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                     "com.example.invalid", kExpected);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly, GetParam(),
                     kExpectedEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo, GetParam(),
                     kExpectedEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo, GetParam(),
                     kExpectedEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly, GetParam(),
                     kExpectedEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo,GetParam(),
                     kExpectedEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameChangeVideo) {
  // Times out on Windows XP. http://crbug.com/171937
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif
  TestFrameSizeChange(GetParam(), kExpectedEnded);
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly, GetParam(),
                     kExpectedEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly, GetParam(),
                     kExpectedEnded);
}
#endif

// Run only when WV CDM is available.
#if defined(WIDEVINE_CDM_AVAILABLE)
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
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

}  // namespace content
