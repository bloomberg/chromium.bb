// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_npapi.h"

TEST(ChromeFrameNPAPI, DoesNotCrashOnConstruction) {
  ChromeFrameNPAPI* api = new ChromeFrameNPAPI();
  delete api;
}

// All mocks in the anonymous namespace.
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;

const char* kMimeType = "application/chromeframe";
// The default profile name is by default derived from the currently
// running executable's name.
const wchar_t* kDefaultProfileName = L"chrome_frame_unittests";


class MockNPAPI: public ChromeFrameNPAPI {
 public:
  MockNPAPI() : mock_automation_client_(NULL) {}
  MOCK_METHOD0(GetLocation, std::string());
  MOCK_METHOD0(GetBrowserIncognitoMode, bool());

  MOCK_METHOD1(JavascriptToNPObject, virtual NPObject*(const std::string&));

  // Make public for test purposes
  void OnAutomationServerReady() {
    ChromeFrameNPAPI::OnAutomationServerReady();
  }

  ChromeFrameAutomationClient* CreateAutomationClient() {
    return mock_automation_client_;
  }

  ChromeFrameAutomationClient* mock_automation_client_;
};

class MockAutomationClient: public ChromeFrameAutomationClient {
 public:
  MOCK_METHOD2(Initialize, bool(ChromeFrameDelegate*,
                                ChromeFrameLaunchParams*));
};

namespace {

MATCHER_P4(LaunchParamEq, version_check, extra, incognito, widget,
           "Basic check for ChromeFrameLaunchParams") {
  return arg->version_check() == version_check &&
         arg->extra_arguments().compare(extra) == 0 &&
         arg->incognito() == incognito,
         arg->widget_mode() == widget;
}


static const NPIdentifier kOnPrivateMessageId =
    reinterpret_cast<NPIdentifier>(0x100);
static const NPIdentifier kPostPrivateMessageId =
    reinterpret_cast<NPIdentifier>(0x100);
}

class MockNetscapeFuncs {
 public:
  MockNetscapeFuncs() {
    CHECK(NULL == current_);
    current_ = this;
  }

  ~MockNetscapeFuncs() {
    CHECK(this == current_);
    current_ = NULL;
  }

  MOCK_METHOD3(GetValue, NPError(NPP, NPNVariable, void *));
  MOCK_METHOD3(GetStringIdentifiers, void(const NPUTF8 **,
                                          int32_t,
                                          NPIdentifier *));  // NOLINT
  MOCK_METHOD1(RetainObject, NPObject*(NPObject*));  // NOLINT
  MOCK_METHOD1(ReleaseObject, void(NPObject*));  // NOLINT

  static const NPNetscapeFuncs* netscape_funcs() {
    return &netscape_funcs_;
  }

 private:
  static NPError MockGetValue(NPP instance,
                              NPNVariable variable,
                              void *ret_value) {
    DCHECK(current_);
    return current_->GetValue(instance, variable, ret_value);
  }

  static void MockGetStringIdentifiers(const NPUTF8 **names,
                                       int32_t name_count,
                                       NPIdentifier *identifiers) {
    DCHECK(current_);
    return current_->GetStringIdentifiers(names, name_count, identifiers);
  }

  static NPObject* MockRetainObject(NPObject* obj) {
    DCHECK(current_);
    return current_->RetainObject(obj);
  }

  static void MockReleaseObject(NPObject* obj) {
    DCHECK(current_);
    current_->ReleaseObject(obj);
  }

  static MockNetscapeFuncs* current_;
  static NPNetscapeFuncs netscape_funcs_;
};

// Test fixture to allow testing the privileged NPAPI APIs
class TestNPAPIPrivilegedApi: public ::testing::Test {
 public:
  virtual void SetUp() {
    memset(&instance, 0, sizeof(instance));
    npapi::InitializeBrowserFunctions(
        const_cast<NPNetscapeFuncs*>(mock_funcs.netscape_funcs()));

    // Gets owned & destroyed by mock_api (in the
    // ChromeFramePlugin<T>::Uninitialize() function).
    mock_automation = new MockAutomationClient;

    mock_api.mock_automation_client_ = mock_automation;
  }

  virtual void TearDown() {
    // Make sure to uninitialize the mock NPAPI before we uninitialize the
    // browser function, otherwise we will get a DCHECK in the NPAPI dtor
    // when it tries to perform the uninitialize there.
    mock_api.Uninitialize();
    npapi::UninitializeBrowserFunctions();
  }

  void SetupPrivilegeTest(bool is_incognito,
                          bool expect_privilege_check,
                          bool is_privileged,
                          const std::wstring& profile_name,
                          const std::wstring& language,
                          const std::wstring& extra_args) {
    EXPECT_CALL(mock_api, GetLocation())
        .WillOnce(Return(std::string("http://www.google.com")));
    EXPECT_CALL(mock_api, GetBrowserIncognitoMode())
        .WillOnce(Return(is_incognito));

    scoped_refptr<ChromeFrameLaunchParams> launch_params(
        new ChromeFrameLaunchParams(GURL(), GURL(), FilePath(), profile_name,
            language, extra_args, is_incognito, true, false));

    EXPECT_CALL(*mock_automation,
      Initialize(_, LaunchParamEq(true, extra_args, is_incognito, true)))
        .WillOnce(Return(true));
  }

 public:
  MockNetscapeFuncs mock_funcs;
  MockNPAPI mock_api;
  MockAutomationClient* mock_automation;
  NPP_t instance;
};

}  // namespace
