// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "url/gurl.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if defined(OS_ANDROID)
#error This file needs to be updated to run on Android.
#endif

#if defined(USE_PROPRIETARY_CODECS)
#define EXPECT_PROPRIETARY EXPECT_TRUE
#else
#define EXPECT_PROPRIETARY EXPECT_FALSE
#endif

// Expectations for External Clear Key.
#if defined(ENABLE_PEPPER_CDMS)
#define EXPECT_ECK EXPECT_TRUE
#define EXPECT_ECKPROPRIETARY EXPECT_PROPRIETARY
#else
#define EXPECT_ECK EXPECT_FALSE
#define EXPECT_ECKPROPRIETARY EXPECT_FALSE
#endif  // defined(ENABLE_PEPPER_CDMS)

// Expectations for Widevine.
// Note: Widevine is not available on platforms using components because
// RegisterPepperCdm() cannot set the codecs.
// TODO(ddorwin): Enable these tests after we have the ability to use the CUS
// in these tests. See http://crbug.com/311724.
#if defined(WIDEVINE_CDM_AVAILABLE) && !defined(WIDEVINE_CDM_IS_COMPONENT)
#define EXPECT_WV EXPECT_TRUE

#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
#define EXPECT_WVMP4 EXPECT_TRUE
#define EXPECT_WVAVC1 EXPECT_TRUE
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
#define EXPECT_WVAVC1AAC EXPECT_TRUE
#else
#define EXPECT_WVAVC1AAC EXPECT_FALSE
#endif  // defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
#else  // !defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
#define EXPECT_WVMP4 EXPECT_FALSE
#define EXPECT_WVAVC1 EXPECT_FALSE
#define EXPECT_WVAVC1AAC EXPECT_FALSE
#endif  // defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)

#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
#define EXPECT_WVAAC EXPECT_TRUE
#else
#define EXPECT_WVAAC EXPECT_FALSE
#endif

#else  // defined(WIDEVINE_CDM_AVAILABLE) && !defined(WIDEVINE_CDM_IS_COMPONENT)
#define EXPECT_WV EXPECT_FALSE
#define EXPECT_WVMP4 EXPECT_FALSE
#define EXPECT_WVAVC1 EXPECT_FALSE
#define EXPECT_WVAVC1AAC EXPECT_FALSE
#define EXPECT_WVAAC EXPECT_FALSE
#endif  // defined(WIDEVINE_CDM_AVAILABLE) &&
        // !defined(WIDEVINE_CDM_IS_COMPONENT)

namespace chrome {

const char kPrefixedClearKey[] = "webkit-org.w3.clearkey";
const char kPrefixedClearKeyParent[] = "webkit-org.w3";
// TODO(ddorwin): Duplicate prefixed tests for unprefixed.
const char kUnprefixedClearKey[] = "org.w3.clearkey";
const char kExternalClearKey[] = "org.chromium.externalclearkey";
const char kWidevineAlpha[] = "com.widevine.alpha";
const char kWidevine[] = "com.widevine";
const char kWidevineAlphaHr[] = "com.widevine.alpha.hr";
const char kWidevineAlphaHrNonCompositing[] =
    "com.widevine.alpha.hrnoncompositing";

// TODO(xhwang): Simplify this test! See http://crbug.com/367158

class EncryptedMediaIsTypeSupportedTest : public InProcessBrowserTest {
 protected:
  EncryptedMediaIsTypeSupportedTest()
      : is_test_page_loaded_(false), is_pepper_cdm_registered_(false) {
    vp8_codec_.push_back("vp8");

    vp80_codec_.push_back("vp8.0");

    vp9_codec_.push_back("vp9");

    vp90_codec_.push_back("vp9.0");

    vorbis_codec_.push_back("vorbis");

    vp8_and_vorbis_codecs_.push_back("vp8");
    vp8_and_vorbis_codecs_.push_back("vorbis");

    vp9_and_vorbis_codecs_.push_back("vp9");
    vp9_and_vorbis_codecs_.push_back("vorbis");

    avc1_codec_.push_back("avc1");

    avc1_extended_codec_.push_back("avc1.4D400C");

    avc1_dot_codec_.push_back("avc1.");

    avc2_codec_.push_back("avc2");

    avc3_codec_.push_back("avc3");

    avc3_extended_codec_.push_back("avc3.64001F");

    aac_codec_.push_back("mp4a.40.2");

    mp4a_invalid_no_extension_.push_back("mp4a");

    avc1_and_aac_codecs_.push_back("avc1");
    avc1_and_aac_codecs_.push_back("mp4a.40.2");

    avc3_and_aac_codecs_.push_back("avc3");
    avc3_and_aac_codecs_.push_back("mp4a.40.2");

    avc1_extended_and_aac_codecs_.push_back("avc1.4D400C");
    avc1_extended_and_aac_codecs_.push_back("mp4a.40.2");

    avc3_extended_and_aac_codecs_.push_back("avc3.64001F");
    avc3_extended_and_aac_codecs_.push_back("mp4a.40.2");

    unknown_codec_.push_back("foo");

    mixed_codecs_.push_back("vorbis");
    mixed_codecs_.push_back("avc1");

    vp8_invalid_extension_codec_.push_back("vp8.1");
  }

  typedef std::vector<std::string> CodecVector;

  const CodecVector& no_codecs() const { return no_codecs_; }
  const CodecVector& vp8_codec() const { return vp8_codec_; }
  const CodecVector& vp80_codec() const { return vp80_codec_; }
  const CodecVector& vp9_codec() const { return vp9_codec_; }
  const CodecVector& vp90_codec() const { return vp90_codec_; }
  const CodecVector& vorbis_codec() const { return vorbis_codec_; }
  const CodecVector& vp8_and_vorbis_codecs() const {
    return vp8_and_vorbis_codecs_;
  }
  const CodecVector& vp9_and_vorbis_codecs() const {
    return vp9_and_vorbis_codecs_;
  }
  const CodecVector& avc1_codec() const { return avc1_codec_; }
  const CodecVector& avc1_extended_codec() const {
    return avc1_extended_codec_;
  }
  const CodecVector& avc1_dot_codec() const { return avc1_dot_codec_; }
  const CodecVector& avc2_codec() const { return avc2_codec_; }
  const CodecVector& avc3_codec() const { return avc3_codec_; }
  const CodecVector& avc3_extended_codec() const {
    return avc3_extended_codec_;
  }
  const CodecVector& aac_codec() const { return aac_codec_; }
  const CodecVector& mp4a_invalid_no_extension() const {
    return mp4a_invalid_no_extension_;
  }
  const CodecVector& avc1_and_aac_codecs() const {
    return avc1_and_aac_codecs_;
  }
  const CodecVector& avc3_and_aac_codecs() const {
    return avc3_and_aac_codecs_;
  }
  const CodecVector& avc1_extended_and_aac_codecs() const {
    return avc1_extended_and_aac_codecs_;
  }
  const CodecVector& avc3_extended_and_aac_codecs() const {
    return avc3_extended_and_aac_codecs_;
  }
  const CodecVector& unknown_codec() const { return unknown_codec_; }
  const CodecVector& mixed_codecs() const { return mixed_codecs_; }
  const CodecVector& vp8_invalid_extension_codec() const {
    return vp8_invalid_extension_codec_;
  }

  // Update the command line to load |adapter_name| for
  // |pepper_type_for_key_system|.
  void RegisterPepperCdm(CommandLine* command_line,
                         const std::string& adapter_name,
                         const std::string& pepper_type_for_key_system,
                         bool expect_adapter_exists = true) {
    DCHECK(!is_pepper_cdm_registered_)
        << "RegisterPepperCdm() can only be called once.";
    is_pepper_cdm_registered_ = true;

    // Append the switch to register the appropriate adapter.
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    base::FilePath plugin_lib = plugin_dir.AppendASCII(adapter_name);
    EXPECT_EQ(expect_adapter_exists, base::PathExists(plugin_lib));
    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL("#CDM#0.1.0.0;"));
#if defined(OS_WIN)
    pepper_plugin.append(base::ASCIIToWide(pepper_type_for_key_system));
#else
    pepper_plugin.append(pepper_type_for_key_system);
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }

  void LoadTestPage() {
    // Load the test page needed. IsConcreteSupportedKeySystem() needs some
    // JavaScript and a video loaded in order to work.
    if (!is_test_page_loaded_) {
      scoped_ptr<net::SpawnedTestServer> http_test_server =
          media::StartMediaHttpTestServer();
      GURL gurl = http_test_server->GetURL(
          "files/test_key_system_instantiation.html");
      ui_test_utils::NavigateToURL(browser(), gurl);
      is_test_page_loaded_ = true;
    }
  }

  bool IsConcreteSupportedKeySystem(const std::string& key) {
    std::string command(
        "window.domAutomationController.send(testKeySystemInstantiation('");
    command.append(key);
    command.append("'));");

    // testKeySystemInstantiation() is a JavaScript function which needs to
    // be loaded.
    LoadTestPage();

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        command,
        &result));
    CHECK(result == "success" || result == "NotSupportedError") << result;
    return (result == "success");
  }

  bool IsSupportedKeySystemWithMediaMimeType(const std::string& type,
                                             const CodecVector& codecs,
                                             const std::string& keySystem) {
    std::string command("document.createElement('video').canPlayType(");
    if (type.empty()) {
      // Simple case, pass "null" as first argument.
      command.append("null");
      DCHECK(codecs.empty());
    } else {
      command.append("'");
      command.append(type);
      if (!codecs.empty()) {
        command.append("; codecs=\"");
        for (CodecVector::const_iterator it = codecs.begin();
             it != codecs.end();
             ++it) {
          command.append(*it);
          command.append(",");
        }
        command.replace(command.length() - 1, 1, "\"");
      }
      command.append("'");
    }
    command.append(",'");
    command.append(keySystem);
    command.append("')");

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(" + command + ");",
        &result));
    return (result == "maybe" || result == "probably");
  }

 private:
  const CodecVector no_codecs_;
  CodecVector vp8_codec_;
  CodecVector vp80_codec_;
  CodecVector vp9_codec_;
  CodecVector vp90_codec_;
  CodecVector vorbis_codec_;
  CodecVector vp8_and_vorbis_codecs_;
  CodecVector vp9_and_vorbis_codecs_;
  CodecVector avc1_codec_;
  CodecVector avc1_extended_codec_;
  CodecVector avc1_dot_codec_;
  CodecVector avc2_codec_;
  CodecVector avc3_codec_;
  CodecVector avc3_extended_codec_;
  CodecVector aac_codec_;
  CodecVector mp4a_invalid_no_extension_;
  CodecVector avc1_and_aac_codecs_;
  CodecVector avc3_and_aac_codecs_;
  CodecVector avc1_extended_and_aac_codecs_;
  CodecVector avc3_extended_and_aac_codecs_;
  CodecVector unknown_codec_;
  CodecVector mixed_codecs_;
  CodecVector vp8_invalid_extension_codec_;
  bool is_test_page_loaded_;
  bool is_pepper_cdm_registered_;
};

// For ExternalClearKey tests, ensure that the ClearKey adapter is loaded.
class EncryptedMediaIsTypeSupportedExternalClearKeyTest
    : public EncryptedMediaIsTypeSupportedTest {
#if defined(ENABLE_PEPPER_CDMS)
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Platform-specific filename relative to the chrome executable.
    const char adapter_file_name[] =
#if defined(OS_MACOSX)
        "clearkeycdmadapter.plugin";
#elif defined(OS_WIN)
        "clearkeycdmadapter.dll";
#elif defined(OS_POSIX)
        "libclearkeycdmadapter.so";
#endif

    const std::string pepper_name("application/x-ppapi-clearkey-cdm");
    RegisterPepperCdm(command_line, adapter_file_name, pepper_name);
  }
#endif  // defined(ENABLE_PEPPER_CDMS)
};

// For Widevine tests, ensure that the Widevine adapter is loaded.
class EncryptedMediaIsTypeSupportedWidevineTest
    : public EncryptedMediaIsTypeSupportedTest {
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) && \
    defined(WIDEVINE_CDM_IS_COMPONENT)
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // File name of the adapter on different platforms.
    const char adapter_file_name[] =
#if defined(OS_MACOSX)
        "widevinecdmadapter.plugin";
#elif defined(OS_WIN)
        "widevinecdmadapter.dll";
#else  // OS_LINUX, etc.
        "libwidevinecdmadapter.so";
#endif

    const std::string pepper_name("application/x-ppapi-widevine-cdm");
    RegisterPepperCdm(command_line, adapter_file_name, pepper_name);
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS) &&
        // defined(WIDEVINE_CDM_IS_COMPONENT)
};

#if defined(ENABLE_PEPPER_CDMS)
// Registers ClearKey CDM with the wrong path (filename).
class EncryptedMediaIsTypeSupportedClearKeyCDMRegisteredWithWrongPathTest
    : public EncryptedMediaIsTypeSupportedTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
   RegisterPepperCdm(command_line,
                     "clearkeycdmadapterwrongname.dll",
                     "application/x-ppapi-clearkey-cdm",
                     false);
  }
};

// Registers Widevine CDM with the wrong path (filename).
class EncryptedMediaIsTypeSupportedWidevineCDMRegisteredWithWrongPathTest
    : public EncryptedMediaIsTypeSupportedTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
   RegisterPepperCdm(command_line,
                     "widevinecdmadapterwrongname.dll",
                     "application/x-ppapi-widevine-cdm",
                     false);
  }
};
#endif  // defined(ENABLE_PEPPER_CDMS)

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest, ClearKey_Basic) {
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kPrefixedClearKey));

  // Not yet out from behind the vendor prefix.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kUnprefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kUnprefixedClearKey));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest, ClearKey_Parent) {
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kPrefixedClearKeyParent));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kPrefixedClearKeyParent));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest,
                       ClearKey_IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org.w3.ClEaRkEy"));
  // This should fail, but currently canPlayType() converts it to lowercase.
  // See http://crbug.com/286036.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.ClEaRkEy"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org."));
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org"));
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org."));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType("video/webm", no_codecs(), "org."));
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org"));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType("video/webm", no_codecs(), "org"));

  // Extra period.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org.w3."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3."));

  // Incomplete.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org.w3.clearke"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.clearke"));

  // Extra character.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org.w3.clearkeyz"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.clearkeyz"));

  // There are no child key systems for Clear Key.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("webkit-org.w3.clearkey.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "webkit-org.w3.clearkey.foo"));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest,
                       IsSupportedKeySystemWithMediaMimeType_ClearKey_NoType) {
  // These two should be true. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kPrefixedClearKeyParent));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "webkit-org.w3.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "webkit-org.w3.clearkey.foo"));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest,
                       IsSupportedKeySystemWithMediaMimeType_ClearKey_WebM) {
  // Valid video types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kPrefixedClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kPrefixedClearKeyParent));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_codec(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp90_codec(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_and_vorbis_codecs(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kPrefixedClearKey));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc1_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc3_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", unknown_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", mixed_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_invalid_extension_codec(), kPrefixedClearKey));

  // Valid audio types.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kPrefixedClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_and_vorbis_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp9_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp9_and_vorbis_codecs(), kPrefixedClearKey));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", aac_codec(), kPrefixedClearKey));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest,
                       IsSupportedKeySystemWithMediaMimeType_ClearKey_MP4) {
  // Valid video types.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kPrefixedClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kPrefixedClearKeyParent));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_codec(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_and_aac_codecs(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kPrefixedClearKey));

  // Extended codecs.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_codec(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_extended_codec(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_and_aac_codecs(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_extended_and_aac_codecs(), kPrefixedClearKey));

  // Invalid codec format: profile parameter must be present after the period.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_dot_codec(), kPrefixedClearKey));

  // Invalid or Non-MP4 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc2_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mp4a_invalid_no_extension(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", unknown_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mixed_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_invalid_extension_codec(), kPrefixedClearKey));

  // Valid audio types.
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kPrefixedClearKey));
  EXPECT_PROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kPrefixedClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_extended_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_extended_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_and_aac_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_and_aac_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_extended_and_aac_codecs(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_extended_and_aac_codecs(), kPrefixedClearKey));

  // Invalid or Non-MP4 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", vorbis_codec(), kPrefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", mp4a_invalid_no_extension(), kPrefixedClearKey));
}

//
// External Clear Key
//

// When defined(ENABLE_PEPPER_CDMS), this also tests the Pepper CDM check.
IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedExternalClearKeyTest,
                       ExternalClearKey_Basic) {
  EXPECT_ECK(IsConcreteSupportedKeySystem(kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKey));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedExternalClearKeyTest,
                       ExternalClearKey_Parent) {
  const char* const kExternalClearKeyParent = "org.chromium";

  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kExternalClearKeyParent));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKeyParent));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedExternalClearKeyTest,
                       ExternalClearKey_IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.chromium.ExTeRnAlClEaRkEy"));
  // This should fail, but currently canPlayType() converts it to lowercase.
  // See http://crbug.com/286036.
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium.ExTeRnAlClEaRkEy"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org."));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType("video/webm", no_codecs(), "org."));
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org"));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType("video/webm", no_codecs(), "org"));

  // Extra period.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.chromium."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium."));

  // Incomplete.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.chromium.externalclearke"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium.externalclearke"));

  // Extra character.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("org.chromium.externalclearkeyz"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium.externalclearkeyz"));

  // There are no child key systems for Clear Key.
  EXPECT_FALSE(
      IsConcreteSupportedKeySystem("org.chromium.externalclearkey.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium.externalclearkey.foo"));
}

IN_PROC_BROWSER_TEST_F(
    EncryptedMediaIsTypeSupportedExternalClearKeyTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalClearKey_NoType) {
  // These two should be true. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "org.chromium"));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "org.chromium.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "org.chromium.externalclearkey.foo"));
}

IN_PROC_BROWSER_TEST_F(
    EncryptedMediaIsTypeSupportedExternalClearKeyTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalClearKey_WebM) {
  // Valid video types.
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "org.chromium"));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_codec(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp90_codec(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_and_vorbis_codecs(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kExternalClearKey));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc1_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc3_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", unknown_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", mixed_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_invalid_extension_codec(), kExternalClearKey));

  // Valid audio types.
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kExternalClearKey));
  EXPECT_ECK(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kExternalClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_and_vorbis_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp9_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp9_and_vorbis_codecs(), kExternalClearKey));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", aac_codec(), kExternalClearKey));
}

IN_PROC_BROWSER_TEST_F(
    EncryptedMediaIsTypeSupportedExternalClearKeyTest,
    IsSupportedKeySystemWithMediaMimeType_ExternalClearKey_MP4) {
  // Valid video types.
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kExternalClearKey));
  // The parent should be supported but is not. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), "org.chromium"));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_codec(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_and_aac_codecs(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kExternalClearKey));

  // Extended codecs.
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_codec(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_extended_codec(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_and_aac_codecs(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_extended_and_aac_codecs(), kExternalClearKey));

  // Invalid codec format: profile parameter must be present after the period.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_dot_codec(), kExternalClearKey));

  // Invalid or Non-MP4 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc2_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mp4a_invalid_no_extension(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", unknown_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mixed_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_invalid_extension_codec(), kExternalClearKey));

  // Valid audio types.
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kExternalClearKey));
  EXPECT_ECKPROPRIETARY(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kExternalClearKey));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_extended_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_extended_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_and_aac_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_and_aac_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_extended_and_aac_codecs(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_extended_and_aac_codecs(), kExternalClearKey));

  // Invalid or Non-MP4 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", vorbis_codec(), kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", mp4a_invalid_no_extension(), kExternalClearKey));
}

//
// Widevine
//

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       Widevine_Basic) {
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kWidevineAlpha));
#else
  EXPECT_WV(IsConcreteSupportedKeySystem(kWidevineAlpha));
#endif
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlpha));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       Widevine_Parent) {
  // The parent system is not a concrete system but is supported.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevine));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       Widevine_IsSupportedKeySystem_InvalidVariants) {
  // Case sensitive.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com.widevine.AlPhA"));
  // This should fail, but currently canPlayType() converts it to lowercase.
  // See http://crbug.com/286036.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.AlPhA"));

  // TLDs are not allowed.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com."));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType("video/webm", no_codecs(), "com."));
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com"));
  EXPECT_FALSE(
      IsSupportedKeySystemWithMediaMimeType("video/webm", no_codecs(), "com"));

  // Extra period.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com.widevine."));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine."));

  // Incomplete.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com.widevine.alph"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.alph"));

  // Extra character.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com.widevine.alphab"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.alphab"));

  // There are no child key systems for Widevine Alpha.
  EXPECT_FALSE(IsConcreteSupportedKeySystem("com.widevine.alpha.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), "com.widevine.alpha.foo"));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       IsSupportedKeySystemWithMediaMimeType_Widevine_NoType) {
  // These two should be true. See http://crbug.com/164303.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), kWidevine));

  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "com.widevine.foo"));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      std::string(), no_codecs(), "com.widevine.alpha.foo"));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       IsSupportedKeySystemWithMediaMimeType_Widevine_WebM) {
  // Valid video types.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_codec(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp90_codec(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_and_vorbis_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kWidevineAlpha));

  // Valid video types - parent key system.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_codec(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp80_codec(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_and_vorbis_codecs(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_codec(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp90_codec(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp9_and_vorbis_codecs(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vorbis_codec(), kWidevine));

  // Non-Webm codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc1_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", avc3_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", unknown_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", mixed_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", vp8_invalid_extension_codec(), kWidevineAlpha));

  // Valid audio types.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kWidevineAlpha));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kWidevineAlpha));

  // Valid audio types - parent key system.
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", no_codecs(), kWidevine));
  EXPECT_WV(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vorbis_codec(), kWidevine));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp8_and_vorbis_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp9_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", vp9_and_vorbis_codecs(), kWidevineAlpha));

  // Non-Webm codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/webm", aac_codec(), kWidevineAlpha));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       IsSupportedKeySystemWithMediaMimeType_Widevine_MP4) {
  // Valid video types.
  EXPECT_WVMP4(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kWidevineAlpha));
  EXPECT_WVAVC1(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kWidevineAlpha));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kWidevineAlpha));
  EXPECT_WVAVC1(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_codec(), kWidevineAlpha));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_and_aac_codecs(), kWidevineAlpha));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kWidevineAlpha));

  // Valid video types - parent key system.
  EXPECT_WVMP4(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", no_codecs(), kWidevine));
  EXPECT_WVAVC1(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_codec(), kWidevine));
  EXPECT_WVAVC1(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_codec(), kWidevine));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_and_aac_codecs(), kWidevine));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_and_aac_codecs(), kWidevine));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", aac_codec(), kWidevine));

  // Extended codecs.
  EXPECT_WVAVC1(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_codec(), kWidevineAlpha));
  EXPECT_WVAVC1(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_extended_codec(), kWidevineAlpha));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_extended_and_aac_codecs(), kWidevineAlpha));
  EXPECT_WVAVC1AAC(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc3_extended_and_aac_codecs(), kWidevineAlpha));

  // Invalid codec format: profile paramter must be present after the period.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc1_dot_codec(), kWidevineAlpha));

  // Invalid or Non-MP4 codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", avc2_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mp4a_invalid_no_extension(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", unknown_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", mixed_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/mp4", vp8_invalid_extension_codec(), kWidevineAlpha));

  // Valid audio types.
  EXPECT_WVMP4(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kWidevineAlpha));
  EXPECT_WVAAC(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kWidevineAlpha));

  // Valid audio types - parent key system.
  EXPECT_WVMP4(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", no_codecs(), kWidevine));
  EXPECT_WVAAC(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", aac_codec(), kWidevine));

  // Non-audio codecs.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_extended_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_extended_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_and_aac_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_and_aac_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc1_extended_and_aac_codecs(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", avc3_extended_and_aac_codecs(), kWidevineAlpha));

  // Invalid or Non-MP4 codec.
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", vorbis_codec(), kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "audio/mp4", mp4a_invalid_no_extension(), kWidevineAlpha));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       Widevine_HR_Basic) {
  // HR support cannot be detected in tests, so this is expected to fail
  // everywhere.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kWidevineAlphaHr));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlphaHr));
}

IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedWidevineTest,
                       Widevine_HR_NonCompositing_Basic) {
  // HR non-compositing support cannot be detected in tests, so this is expected
  // to fail everywhere.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kWidevineAlphaHrNonCompositing));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlphaHrNonCompositing));
}

#if defined(ENABLE_PEPPER_CDMS)
// Since this test fixture does not register the CDMs on the command line, the
// check for the CDMs in chrome_key_systems.cc should fail, and they should not
// be registered with KeySystems.
IN_PROC_BROWSER_TEST_F(EncryptedMediaIsTypeSupportedTest,
                       PepperCDMsNotRegistered) {
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKey));

  // This will fail in all builds unless widevine is available but not a
  // component, in which case it is registered internally
#if !defined(WIDEVINE_CDM_AVAILABLE) || defined(WIDEVINE_CDM_IS_COMPONENT)
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlpha));
#endif

  // Clear Key should still be registered.
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kPrefixedClearKey));

  // Not yet out from behind the vendor prefix.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kUnprefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kUnprefixedClearKey));
}

// Since this test fixture does not register the CDMs on the command line, the
// check for the CDMs in chrome_key_systems.cc should fail, and they should not
// be registered with KeySystems.
IN_PROC_BROWSER_TEST_F(
    EncryptedMediaIsTypeSupportedClearKeyCDMRegisteredWithWrongPathTest,
    PepperCDMsRegisteredButAdapterNotPresent) {
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kExternalClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kExternalClearKey));

  // Clear Key should still be registered.
  EXPECT_TRUE(IsConcreteSupportedKeySystem(kPrefixedClearKey));
  EXPECT_TRUE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kPrefixedClearKey));

  // Not yet out from behind the vendor prefix.
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kUnprefixedClearKey));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kUnprefixedClearKey));
}

// This will fail in all builds unless Widevine is available but not a
// component, in which case it is registered internally.
// TODO(xhwang): Define EXPECT_WV and run this test in all cases.
#if !defined(WIDEVINE_CDM_AVAILABLE) || defined(WIDEVINE_CDM_IS_COMPONENT)
IN_PROC_BROWSER_TEST_F(
    EncryptedMediaIsTypeSupportedWidevineCDMRegisteredWithWrongPathTest,
    PepperCDMsRegisteredButAdapterNotPresent) {
  EXPECT_FALSE(IsConcreteSupportedKeySystem(kWidevineAlpha));
  EXPECT_FALSE(IsSupportedKeySystemWithMediaMimeType(
      "video/webm", no_codecs(), kWidevineAlpha));
}
#endif  // !defined(WIDEVINE_CDM_AVAILABLE) || defined(WIDEVINE_CDM_IS_COMPONENT)
#endif  // defined(ENABLE_PEPPER_CDMS)

}  // namespace chrome
