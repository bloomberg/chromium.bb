// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/media/media_browsertest.h"
#include "chrome/browser/media/test_license_server.h"
#include "chrome/browser/media/wv_test_license_server_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#include "widevine_cdm_version.h"  //  In SHARED_INTERMEDIATE_DIR.

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
const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kPrefixedClearKeyKeySystem[] = "webkit-org.w3.clearkey";
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
const char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";
const char kExternalClearKeyInitializeFailKeySystem[] =
    "org.chromium.externalclearkey.initializefail";
const char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";

// Supported media types.
const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";
const char kWebMVP9VideoOnly[] = "video/webm; codecs=\"vp9\"";
#if defined(USE_PROPRIETARY_CODECS)
const char kMP4AudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"";
const char kMP4VideoOnly[] = "video/mp4; codecs=\"avc1.4D4041\"";
#endif  // defined(USE_PROPRIETARY_CODECS)

// Sessions to load.
const char kNoSessionToLoad[] = "";
const char kLoadableSession[] = "LoadableSession";
const char kUnknownSession[] = "UnknownSession";

// EME-specific test results and errors.
const char kFileIOTestSuccess[] = "FILE_IO_TEST_SUCCESS";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";
const char kEmeGenerateRequestFailed[] = "EME_GENERATEREQUEST_FAILED";
const char kEmeSessionNotFound[] = "EME_SESSION_NOT_FOUND";
const char kEmeLoadFailed[] = "EME_LOAD_FAILED";
const char kEmeUpdateFailed[] = "EME_UPDATE_FAILED";
const char kEmeErrorEvent[] = "EME_ERROR_EVENT";
const char kEmeMessageUnexpectedType[] = "EME_MESSAGE_UNEXPECTED_TYPE";
const char kPrefixedEmeRenewalMissingHeader[] =
    "PREFIXED_EME_RENEWAL_MISSING_HEADER";
const char kPrefixedEmeErrorEvent[] = "PREFIXED_EME_ERROR_EVENT";

const char kDefaultEmePlayer[] = "eme_player.html";

// The type of video src used to load media.
enum SrcType {
  SRC,
  MSE
};

// Whether to use prefixed or unprefixed EME.
enum EmeVersion {
  PREFIXED,
  UNPREFIXED
};

// Whether the video should be played once or twice.
enum class PlayTwice { NO, YES };

// Format of a container when testing different streams.
enum class EncryptedContainer {
  CLEAR_WEBM,
  CLEAR_MP4,
  ENCRYPTED_WEBM,
  ENCRYPTED_MP4
};

// MSE is available on all desktop platforms and on Android 4.1 and later.
static bool IsMSESupported() {
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16) {
    DVLOG(0) << "MSE is only supported in Android 4.1 and later.";
    return false;
  }
#endif  // defined(OS_ANDROID)
  return true;
}

static bool IsParentKeySystemOf(const std::string& parent_key_system,
                                const std::string& key_system) {
  std::string prefix = parent_key_system + '.';
  return key_system.substr(0, prefix.size()) == prefix;
}

// Base class for encrypted media tests.
class EncryptedMediaTestBase : public MediaBrowserTest {
 public:
  EncryptedMediaTestBase() : is_pepper_cdm_registered_(false) {}

  bool IsExternalClearKey(const std::string& key_system) {
    return key_system == kExternalClearKeyKeySystem ||
           IsParentKeySystemOf(kExternalClearKeyKeySystem, key_system);
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  bool IsWidevine(const std::string& key_system) {
    return key_system == kWidevineKeySystem;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

  void RunEncryptedMediaTestPage(
      const std::string& html_page,
      const std::string& key_system,
      base::StringPairs& query_params,
      const std::string& expected_title) {
    base::StringPairs new_query_params = query_params;
    StartLicenseServerIfNeeded(key_system, &new_query_params);
    RunMediaTestPage(html_page, new_query_params, expected_title, true);
  }

  // Tests |html_page| using |media_file| (with |media_type|) and |key_system|.
  // When |session_to_load| is not empty, the test will try to load
  // |session_to_load| with stored keys, instead of creating a new session
  // and trying to update it with licenses.
  // When |force_invalid_response| is true, the test will provide invalid
  // responses, which should trigger errors.
  // TODO(xhwang): Find an easier way to pass multiple configuration test
  // options.
  void RunEncryptedMediaTest(const std::string& html_page,
                             const std::string& media_file,
                             const std::string& media_type,
                             const std::string& key_system,
                             SrcType src_type,
                             EmeVersion eme_version,
                             const std::string& session_to_load,
                             bool force_invalid_response,
                             PlayTwice play_twice,
                             const std::string& expected_title) {
    if (src_type == MSE && !IsMSESupported()) {
      DVLOG(0) << "Skipping test - MSE not supported.";
      return;
    }
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("mediaFile", media_file));
    query_params.push_back(std::make_pair("mediaType", media_type));
    query_params.push_back(std::make_pair("keySystem", key_system));
    if (src_type == MSE)
      query_params.push_back(std::make_pair("useMSE", "1"));
    if (eme_version == PREFIXED)
      query_params.push_back(std::make_pair("usePrefixedEME", "1"));
    if (force_invalid_response)
      query_params.push_back(std::make_pair("forceInvalidResponse", "1"));
    if (!session_to_load.empty())
      query_params.push_back(std::make_pair("sessionToLoad", session_to_load));
    if (play_twice == PlayTwice::YES)
      query_params.push_back(std::make_pair("playTwice", "1"));
    RunEncryptedMediaTestPage(html_page, key_system, query_params,
                              expected_title);
  }

  void RunSimpleEncryptedMediaTest(const std::string& media_file,
                                   const std::string& media_type,
                                   const std::string& key_system,
                                   SrcType src_type,
                                   EmeVersion eme_version) {
    std::string expected_title = kEnded;
    if (!IsPlayBackPossible(key_system)) {
      expected_title = (eme_version == EmeVersion::UNPREFIXED)
                           ? kEmeUpdateFailed
                           : kPrefixedEmeErrorEvent;
    }

    RunEncryptedMediaTest(kDefaultEmePlayer, media_file, media_type, key_system,
                          src_type, eme_version, kNoSessionToLoad, false,
                          PlayTwice::NO, expected_title);
    // Check KeyMessage received for all key systems.
    bool receivedKeyMessage = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send("
        "document.querySelector('video').receivedKeyMessage);",
        &receivedKeyMessage));
    EXPECT_TRUE(receivedKeyMessage);
  }

  // Starts a license server if available for the |key_system| and adds a
  // 'licenseServerURL' query parameter to |query_params|.
  void StartLicenseServerIfNeeded(const std::string& key_system,
                                  base::StringPairs* query_params) {
    scoped_ptr<TestLicenseServerConfig> config = GetServerConfig(key_system);
    if (!config)
      return;
    license_server_.reset(new TestLicenseServer(config.Pass()));
    EXPECT_TRUE(license_server_->Start());
    query_params->push_back(
        std::make_pair("licenseServerURL", license_server_->GetServerURL()));
  }

  bool IsPlayBackPossible(const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system) && !GetServerConfig(key_system))
      return false;
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    return true;
  }

  scoped_ptr<TestLicenseServerConfig> GetServerConfig(
      const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system)) {
      scoped_ptr<TestLicenseServerConfig> config =
         scoped_ptr<TestLicenseServerConfig>(new WVTestLicenseServerConfig());
      if (config->IsPlatformSupported())
        return config.Pass();
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    return scoped_ptr<TestLicenseServerConfig>();
  }

 protected:
  scoped_ptr<TestLicenseServer> license_server_;

  // We want to fail quickly when a test fails because an error is encountered.
  void AddWaitForTitles(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeGenerateRequestFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeSessionNotFound));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeLoadFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeUpdateFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeErrorEvent));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeMessageUnexpectedType));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kPrefixedEmeRenewalMissingHeader));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kPrefixedEmeErrorEvent));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
    // For simplicity with respect to parameterized tests, enable for all tests.
    command_line->AppendSwitch(switches::kEnablePrefixedEncryptedMedia);
  }

  void SetUpCommandLineForKeySystem(const std::string& key_system,
                                    base::CommandLine* command_line) {
    if (GetServerConfig(key_system))
      // Since the web and license servers listen on different ports, we need to
      // disable web-security to send license requests to the license server.
      // TODO(shadi): Add port forwarding to the test web server configuration.
      command_line->AppendSwitch(switches::kDisableWebSecurity);

#if defined(ENABLE_PEPPER_CDMS)
    if (IsExternalClearKey(key_system)) {
      RegisterPepperCdm(command_line, kClearKeyCdmAdapterFileName, key_system);
    }
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    else if (IsWidevine(key_system)) {  // NOLINT
      RegisterPepperCdm(command_line, kWidevineCdmAdapterFileName, key_system);
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
#endif  // defined(ENABLE_PEPPER_CDMS)
  }

 private:
#if defined(ENABLE_PEPPER_CDMS)
  void RegisterPepperCdm(base::CommandLine* command_line,
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
    pepper_plugin.append(base::ASCIIToUTF16(GetPepperType(key_system)));
#else
    pepper_plugin.append(GetPepperType(key_system));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }

  // Adapted from key_systems.cc.
  std::string GetPepperType(const std::string& key_system) {
    if (IsExternalClearKey(key_system))
      return kClearKeyCdmPluginMimeType;
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system))
      return kWidevineCdmPluginMimeType;
#endif  // WIDEVINE_CDM_AVAILABLE

    NOTREACHED();
    return "";
  }
#endif  // defined(ENABLE_PEPPER_CDMS)

  bool is_pepper_cdm_registered_;
};

#if defined(ENABLE_PEPPER_CDMS)
// Tests encrypted media playback using ExternalClearKey key system in
// decrypt-and-decode mode.
class ECKEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  // We use special |key_system| names to do non-playback related tests, e.g.
  // kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(
        kDefaultEmePlayer, "bear-a_enc-a.webm", kWebMAudioOnly, key_system, SRC,
        UNPREFIXED, kNoSessionToLoad, false, PlayTwice::NO, expected_title);
  }

  void TestPlaybackCase(const std::string& session_to_load,
                        const std::string& expected_title) {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-v_enc-v.webm",
                          kWebMVideoOnly, kExternalClearKeyKeySystem, SRC,
                          UNPREFIXED, session_to_load, false, PlayTwice::NO,
                          expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kExternalClearKeyKeySystem, command_line);
  }
};

// Tests encrypted media playback using ExternalClearKey key system in
// decrypt-and-decode mode for prefixed EME.
class ECKPrefixedEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  // We use special |key_system| names to do non-playback related tests, e.g.
  // kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(
        kDefaultEmePlayer, "bear-a_enc-a.webm", kWebMAudioOnly, key_system, SRC,
        PREFIXED, kNoSessionToLoad, false, PlayTwice::NO, expected_title);
  }

  void TestPlaybackCase(const std::string& session_to_load,
                        const std::string& expected_title) {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-v_enc-v.webm",
                          kWebMVideoOnly, kExternalClearKeyKeySystem, SRC,
                          PREFIXED, session_to_load, false, PlayTwice::NO,
                          expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kExternalClearKeyKeySystem, command_line);
  }
};

#if defined(WIDEVINE_CDM_AVAILABLE)
// Tests encrypted media playback using Widevine key system.
class WVEncryptedMediaTest : public EncryptedMediaTestBase {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kWidevineKeySystem, command_line);
  }
};

#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // defined(ENABLE_PEPPER_CDMS)

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - SrcType: Use MSE or SRC.
// - EmeVersion: Use PREFIXED or UNPREFIXED EME.
//
// Note: Only parameterized (*_P) tests can be used. Non-parameterized (*_F)
// tests will crash at GetParam(). To add non-parameterized tests, use
// EncryptedMediaTestBase or one of its subclasses (e.g. WVEncryptedMediaTest).
class EncryptedMediaTest
    : public EncryptedMediaTestBase,
      public testing::WithParamInterface<
          std::tr1::tuple<const char*, SrcType, EmeVersion> > {
 public:
  std::string CurrentKeySystem() {
    return std::tr1::get<0>(GetParam());
  }

  SrcType CurrentSourceType() {
    return std::tr1::get<1>(GetParam());
  }

  EmeVersion CurrentEmeVersion() {
    return std::tr1::get<2>(GetParam());
  }

  void TestSimplePlayback(const std::string& encrypted_media,
                          const std::string& media_type) {
    RunSimpleEncryptedMediaTest(encrypted_media,
                                media_type,
                                CurrentKeySystem(),
                                CurrentSourceType(),
                                CurrentEmeVersion());
  }

  void TestMultiplePlayback(const std::string& encrypted_media,
                            const std::string& media_type) {
    DCHECK(IsPlayBackPossible(CurrentKeySystem()));
    RunEncryptedMediaTest(kDefaultEmePlayer, encrypted_media, media_type,
                          CurrentKeySystem(), CurrentSourceType(),
                          CurrentEmeVersion(), kNoSessionToLoad, false,
                          PlayTwice::YES, kEnded);
  }

  void RunInvalidResponseTest() {
    std::string expected_response =
        (CurrentEmeVersion() == EmeVersion::UNPREFIXED)
            ? kEmeUpdateFailed
            : kPrefixedEmeErrorEvent;
    RunEncryptedMediaTest(
        kDefaultEmePlayer, "bear-320x240-av_enc-av.webm", kWebMAudioVideo,
        CurrentKeySystem(), CurrentSourceType(), CurrentEmeVersion(),
        kNoSessionToLoad, true, PlayTwice::NO, expected_response);
  }

  void TestFrameSizeChange() {
    RunEncryptedMediaTest(
        "encrypted_frame_size_change.html", "frame_size_change-av_enc-v.webm",
        kWebMAudioVideo, CurrentKeySystem(), CurrentSourceType(),
        CurrentEmeVersion(), kNoSessionToLoad, false, PlayTwice::NO, kEnded);
  }

  void TestConfigChange() {
    DCHECK(IsMSESupported());
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("keySystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runEncrypted", "1"));
    if (CurrentEmeVersion() == PREFIXED)
      query_params.push_back(std::make_pair("usePrefixedEME", "1"));
    RunEncryptedMediaTestPage("mse_config_change.html",
                              CurrentKeySystem(),
                              query_params,
                              kEnded);
  }

  std::string ConvertContainerFormat(EncryptedContainer format) {
    switch (format) {
      case EncryptedContainer::CLEAR_MP4:
        return "CLEAR_MP4";
      case EncryptedContainer::CLEAR_WEBM:
        return "CLEAR_WEBM";
      case EncryptedContainer::ENCRYPTED_MP4:
        return "ENCRYPTED_MP4";
      case EncryptedContainer::ENCRYPTED_WEBM:
        return "ENCRYPTED_WEBM";
    }
    NOTREACHED();
    return "UNKNOWN";
  }

  void TestDifferentContainers(EncryptedContainer video_format,
                               EncryptedContainer audio_format) {
    DCHECK(IsMSESupported());
    DCHECK_NE(CurrentEmeVersion(), PREFIXED);
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("keySystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runEncrypted", "1"));
    query_params.push_back(
        std::make_pair("videoFormat", ConvertContainerFormat(video_format)));
    query_params.push_back(
        std::make_pair("audioFormat", ConvertContainerFormat(audio_format)));
    RunEncryptedMediaTestPage("mse_different_containers.html",
                              CurrentKeySystem(), query_params, kEnded);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(CurrentKeySystem(), command_line);
  }
};

using ::testing::Combine;
using ::testing::Values;

#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(SRC_ClearKey_Prefixed,
                        EncryptedMediaTest,
                        Combine(Values(kPrefixedClearKeyKeySystem),
                                Values(SRC),
                                Values(PREFIXED)));

INSTANTIATE_TEST_CASE_P(SRC_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem),
                                Values(SRC),
                                Values(UNPREFIXED)));
#endif  // !defined(OS_ANDROID)

INSTANTIATE_TEST_CASE_P(MSE_ClearKey_Prefixed,
                        EncryptedMediaTest,
                        Combine(Values(kPrefixedClearKeyKeySystem),
                                Values(MSE),
                                Values(PREFIXED)));
INSTANTIATE_TEST_CASE_P(MSE_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem),
                                Values(MSE),
                                Values(UNPREFIXED)));

// External Clear Key is currently only used on platforms that use Pepper CDMs.
#if defined(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey_Prefixed,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SRC),
                                Values(PREFIXED)));
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SRC),
                                Values(UNPREFIXED)));

INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey_Prefixed,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(MSE),
                                Values(PREFIXED)));
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(MSE),
                                Values(UNPREFIXED)));

const char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";

// To reduce test time, only run ExternalClearKeyDecryptOnly with MSE.
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKeyDecryptOnly_Prefixed,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyDecryptOnlyKeySystem),
                                Values(MSE),
                                Values(PREFIXED)));
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKeyDecryptOnly,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyDecryptOnlyKeySystem),
                                Values(MSE),
                                Values(UNPREFIXED)));
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(WIDEVINE_CDM_AVAILABLE)

// Prefixed Widevine tests fail in Chrome OS official builds due to the request
// for permissions. Since prefixed EME is deprecated and will be removed soon,
// don't run these tests. http://crbug.com/430711
#if !(defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD))

// This test doesn't fully test playback with Widevine. So we only run Widevine
// test with MSE (no SRC) to reduce test time. Also, on Android EME only works
// with MSE and we cannot run this test with SRC.
INSTANTIATE_TEST_CASE_P(MSE_Widevine_Prefixed,
                        EncryptedMediaTest,
                        Combine(Values(kWidevineKeySystem),
                                Values(MSE),
                                Values(PREFIXED)));
#endif  // !(defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD))

#if !defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(MSE_Widevine,
                        EncryptedMediaTest,
                        Combine(Values(kWidevineKeySystem),
                                Values(MSE),
                                Values(UNPREFIXED)));
#endif  // !defined(OS_CHROMEOS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a_enc-a.webm", kWebMAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v_enc-v.webm", kWebMVideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VP9Video_WebM) {
  TestSimplePlayback("bear-320x240-v-vp9_enc-v.webm", kWebMVP9VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-a_enc-a.webm", kWebMAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Multiple_VideoAudio_WebM) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Playback_Multiple test requires playback.";
    return;
  }
  TestMultiplePlayback("bear-320x240-av_enc-av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, InvalidResponseKeyError) {
  RunInvalidResponseTest();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo) {
  if (CurrentSourceType() != MSE || !IsMSESupported()) {
    DVLOG(0) << "Skipping test - ConfigChange test requires MSE.";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - ConfigChange test requires video playback.";
    return;
  }
  TestConfigChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  // Times out on Windows XP. http://crbug.com/171937
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - FrameSizeChange test requires video playback.";
    return;
  }
  TestFrameSizeChange();
}

#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_MP4_ClearAudio_WEBM) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  if (CurrentEmeVersion() != UNPREFIXED) {
    DVLOG(0) << "Skipping test; Only supported by unprefixed EME";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Test requires video playback.";
    return;
  }
  TestDifferentContainers(EncryptedContainer::ENCRYPTED_MP4,
                          EncryptedContainer::CLEAR_WEBM);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_ClearVideo_WEBM_EncryptedAudio_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  if (CurrentEmeVersion() != UNPREFIXED) {
    DVLOG(0) << "Skipping test; Only supported by unprefixed EME";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Test requires video playback.";
    return;
  }
  TestDifferentContainers(EncryptedContainer::CLEAR_WEBM,
                          EncryptedContainer::ENCRYPTED_MP4);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  if (CurrentEmeVersion() != UNPREFIXED) {
    DVLOG(0) << "Skipping test; Only supported by unprefixed EME";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Test requires video playback.";
    return;
  }
  TestDifferentContainers(EncryptedContainer::ENCRYPTED_WEBM,
                          EncryptedContainer::ENCRYPTED_MP4);
}
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// The parent key system cannot be used in generateKeyRequest.
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, ParentThrowsException_Prefixed) {
  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm", kWebMAudioOnly,
                        "com.widevine", MSE, PREFIXED, kNoSessionToLoad, false,
                        PlayTwice::NO, kEmeNotSupportedError);
}

// The parent key system cannot be used when creating MediaKeys.
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, ParentThrowsException) {
  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm", kWebMAudioOnly,
                        "com.widevine", MSE, UNPREFIXED, kNoSessionToLoad,
                        false, PlayTwice::NO, kEmeNotSupportedError);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#if defined(ENABLE_PEPPER_CDMS)
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, InitializeCDMFail) {
  TestNonPlaybackCases(kExternalClearKeyInitializeFailKeySystem,
                       kEmeNotSupportedError);
}

// When CDM crashes, we should still get a decode error. |kError| is reported
// when the HTMLVideoElement error event fires, indicating an error happened
// during playback.
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, CDMCrashDuringDecode) {
  IgnorePluginCrash();
  TestNonPlaybackCases(kExternalClearKeyCrashKeySystem, kError);
}

// Testing that the media browser test does fail on plugin crash.
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, CDMExpectedCrash) {
  // Plugin crash is not ignored by default, the test is expected to fail.
  EXPECT_NONFATAL_FAILURE(
      TestNonPlaybackCases(kExternalClearKeyCrashKeySystem, kError),
      "Failing test due to plugin crash.");
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, FileIOTest) {
  TestNonPlaybackCases(kExternalClearKeyFileIOTestKeySystem,
                       kFileIOTestSuccess);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, LoadLoadableSession) {
  TestPlaybackCase(kLoadableSession, kEnded);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, LoadUnknownSession) {
  TestPlaybackCase(kUnknownSession, kEmeSessionNotFound);
}

IN_PROC_BROWSER_TEST_F(ECKPrefixedEncryptedMediaTest, InitializeCDMFail) {
  TestNonPlaybackCases(kExternalClearKeyInitializeFailKeySystem,
                       kPrefixedEmeErrorEvent);
}

// When CDM crashes, we should still get a decode error.
// crbug.com/386657
IN_PROC_BROWSER_TEST_F(ECKPrefixedEncryptedMediaTest,
                       DISABLED_CDMCrashDuringDecode) {
  IgnorePluginCrash();
  TestNonPlaybackCases(kExternalClearKeyCrashKeySystem, kError);
}

// Testing that the media browser test does fail on plugin crash.
// crbug.com/386657
IN_PROC_BROWSER_TEST_F(ECKPrefixedEncryptedMediaTest,
                       DISABLED_CDMExpectedCrash) {
  // Plugin crash is not ignored by default, the test is expected to fail.
  EXPECT_NONFATAL_FAILURE(
      TestNonPlaybackCases(kExternalClearKeyCrashKeySystem, kError),
      "plugin crash");
}

IN_PROC_BROWSER_TEST_F(ECKPrefixedEncryptedMediaTest, FileIOTest) {
  TestNonPlaybackCases(kExternalClearKeyFileIOTestKeySystem,
                       kFileIOTestSuccess);
}

IN_PROC_BROWSER_TEST_F(ECKPrefixedEncryptedMediaTest, LoadLoadableSession) {
  TestPlaybackCase(kLoadableSession, kEnded);
}

IN_PROC_BROWSER_TEST_F(ECKPrefixedEncryptedMediaTest, LoadUnknownSession) {
  TestPlaybackCase(kUnknownSession, kPrefixedEmeErrorEvent);
}
#endif  // defined(ENABLE_PEPPER_CDMS)
