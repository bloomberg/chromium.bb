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
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "media/base/media_switches.h"
#include "webkit/media/crypto/key_systems.h"

// Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
static const wchar_t kLibraryName[] = L"clearkeycdmplugin.dll";
#elif defined(OS_MACOSX)
static const char kLibraryName[] = "clearkeycdmplugin.plugin";
#elif defined(OS_POSIX)
static const char kLibraryName[] = "libclearkeycdmplugin.so";
#endif

// Available key systems.
static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

static const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
static const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
static const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";

namespace content {

class EncryptedMediaTest : public testing::WithParamInterface<const char*>,
                           public ContentBrowserTest {
 public:
  void TestSimplePlayback(const char* encrypted_media, const char* media_type,
                          const char* key_system, const string16 expectation) {
    PlayEncryptedMedia("encrypted_media_player.html", encrypted_media,
                       media_type, key_system, expectation);
  }

  void TestFrameSizeChange(const char* key_system, const string16 expectation) {
    PlayEncryptedMedia("encrypted_frame_size_change.html",
                       "frame_size_change-av-enc-v.webm", kWebMAudioVideo,
                       key_system, expectation);
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
    command_line->AppendSwitch(switches::kDisableAudio);
    command_line->AppendSwitch(switches::kEnableEncryptedMedia);
    command_line->AppendSwitch(switches::kPpapiOutOfProcess);

    // Append the switch to register the Clear Key CDM plugin.
    FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    FilePath plugin_lib = plugin_dir.Append(kLibraryName);
    EXPECT_TRUE(file_util::PathExists(plugin_lib));
    FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL(
        "#Clear Key CDM#Clear Key CDM 0.1.0.0#0.1.0.0;"));
#if defined(OS_WIN)
      pepper_plugin.append(ASCIIToWide(
          webkit_media::GetPluginType(kExternalClearKeyKeySystem)));
#else
      pepper_plugin.append(
          webkit_media::GetPluginType(kExternalClearKeyKeySystem));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }
};

INSTANTIATE_TEST_CASE_P(ClearKey, EncryptedMediaTest,
                        ::testing::Values(kClearKeyKeySystem));

// http://crbug.com/152864 (Mac)
#if !defined(OS_MACOSX)
INSTANTIATE_TEST_CASE_P(ExternalClearKey, EncryptedMediaTest,
                        ::testing::Values(kExternalClearKeyKeySystem));
#endif

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, InvalidKeySystem) {
  const string16 kExpected = ASCIIToUTF16(
      StringToUpperASCII(std::string("GenerateKeyRequestException")));
  ASSERT_NO_FATAL_FAILURE(
      TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                         "com.example.invalid", kExpected));
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, BasicPlayback_AudioOnly) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(
      TestSimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly,
                         GetParam(), kExpected));
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, BasicPlayback_AudioClearVideo) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(
      TestSimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo,
                         GetParam(), kExpected));
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, BasicPlayback_VideoAudio) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(
      TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo,
                         GetParam(), kExpected));
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, BasicPlayback_VideoOnly) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(
      TestSimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly,
                         GetParam(), kExpected));
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, BasicPlayback_VideoClearAudio) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(
      TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo,
                         GetParam(), kExpected));
}

#if defined(OS_WIN)
// Times out on Windows. http://crbug.com/171937
#define MAYBE_FrameChangeVideo DISABLED_FrameChangeVideo
#else
#define MAYBE_FrameChangeVideo FrameChangeVideo
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, MAYBE_FrameChangeVideo) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(TestFrameSizeChange(GetParam(), kExpected));
}

}  // namespace content
