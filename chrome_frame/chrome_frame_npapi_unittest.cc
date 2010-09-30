// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_npapi.h"
#include "chrome_frame/ff_privilege_check.h"

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

// Make mocking privilege test easy.
class MockPrivilegeTest {
 public:
  MockPrivilegeTest() {
    CHECK(current_ == NULL);
    current_ = this;
  }
  ~MockPrivilegeTest() {
    CHECK(current_ == this);
    current_ = NULL;
  }

  MOCK_METHOD1(IsFireFoxPrivilegedInvocation, bool(NPP));

  static MockPrivilegeTest* current() { return current_; }

 private:
  static MockPrivilegeTest* current_;
};

MockPrivilegeTest* MockPrivilegeTest::current_ = NULL;

const char* kMimeType = "application/chromeframe";
// The default profile name is by default derived from the currently
// running executable's name.
const wchar_t* kDefaultProfileName = L"chrome_frame_unittests";


class MockNPAPI: public ChromeFrameNPAPI {
 public:
  MockNPAPI() : mock_automation_client_(NULL) {}

  MOCK_METHOD0(CreatePrefService, NpProxyService*());

  MOCK_METHOD0(GetLocation, std::string());
  MOCK_METHOD0(GetBrowserIncognitoMode, bool());

  MOCK_METHOD1(JavascriptToNPObject, virtual NPObject*(const std::string&));

  // Make public for test purposes
  void OnAutomationServerReady() {
    ChromeFrameNPAPI::OnAutomationServerReady();
  }

  // Neuter this (or it dchecks during testing).
  void SetReadyState(READYSTATE new_state) {}

  ChromeFrameAutomationClient* CreateAutomationClient() {
    return mock_automation_client_;
  }

  ChromeFrameAutomationClient* mock_automation_client_;
};

class MockAutomationClient: public ChromeFrameAutomationClient {
 public:
  MOCK_METHOD2(Initialize, bool(ChromeFrameDelegate*,
                                ChromeFrameLaunchParams*));
  MOCK_METHOD1(SetEnableExtensionAutomation,
               void(const std::vector<std::string>&));  // NOLINT
};

class MockProxyService: public NpProxyService {
 public:
  MOCK_METHOD2(Initialize, bool(NPP instance, ChromeFrameAutomationClient*));
};

namespace {

MATCHER_P4(LaunchParamEq, version_check, extra, incognito, widget,
           "Basic check for ChromeFrameLaunchParams") {
  return arg->version_check() == version_check &&
         arg->extra_arguments().compare(extra) == 0 &&
         arg->incognito() == incognito,
         arg->widget_mode() == widget;
}
}

// Test fixture to allow testing the privileged NPAPI APIs
class TestNPAPIPrivilegedApi: public ::testing::Test {
 public:
  virtual void SetUp() {
    memset(&instance, 0, sizeof(instance));

    // Gets owned & destroyed by mock_api (in the
    // ChromeFramePlugin<T>::Uninitialize() function).
    mock_automation = new MockAutomationClient;

    mock_api.mock_automation_client_ = mock_automation;
    mock_proxy = new MockProxyService;
    mock_proxy->AddRef();
    mock_proxy_holder.Attach(mock_proxy);
  }

  virtual void TearDown() {
  }

  void SetupPrivilegeTest(bool is_incognito,
                          bool expect_privilege_check,
                          bool is_privileged,
                          const std::wstring& profile_name,
                          const std::wstring& extra_args) {
    EXPECT_CALL(mock_api, GetLocation())
        .WillOnce(Return(std::string("http://www.google.com")));
    EXPECT_CALL(mock_api, CreatePrefService())
        .WillOnce(Return(mock_proxy));
    EXPECT_CALL(mock_api, GetBrowserIncognitoMode())
        .WillOnce(Return(is_incognito));

    EXPECT_CALL(*mock_proxy, Initialize(_, _)).WillRepeatedly(Return(false));

    scoped_refptr<ChromeFrameLaunchParams> launch_params(
        new ChromeFrameLaunchParams(GURL(), GURL(), FilePath(), profile_name,
            extra_args, is_incognito, true, false));

    EXPECT_CALL(*mock_automation,
      Initialize(_, LaunchParamEq(true, extra_args, is_incognito, true)))
        .WillOnce(Return(true));

    if (expect_privilege_check) {
      EXPECT_CALL(mock_priv, IsFireFoxPrivilegedInvocation(_))
          .WillOnce(Return(is_privileged));
    } else {
      EXPECT_CALL(mock_priv, IsFireFoxPrivilegedInvocation(_))
          .Times(0);  // Fail if privilege check invoked.
    }
  }

 public:
  MockNPAPI mock_api;
  MockAutomationClient* mock_automation;
  MockProxyService* mock_proxy;
  ScopedNsPtr<nsISupports> mock_proxy_holder;
  MockPrivilegeTest mock_priv;
  NPP_t instance;
};

}  // namespace

// Stub for unittesting.
bool IsFireFoxPrivilegedInvocation(NPP npp) {
  MockPrivilegeTest* mock = MockPrivilegeTest::current();
  if (!mock)
    return false;

  return mock->IsFireFoxPrivilegedInvocation(npp);
}

TEST_F(TestNPAPIPrivilegedApi, NoPrivilegeCheckWhenNoArguments) {
  SetupPrivilegeTest(false,  // Not incognito
                     false,  // Fail if privilege check is invoked.
                     false,
                     kDefaultProfileName,
                     L"");   // No extra args to initialize.

  // No arguments, no privilege requested.
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  0, 0, 0));
}

TEST_F(TestNPAPIPrivilegedApi, NoPrivilegeCheckWhenZeroArgument) {
  SetupPrivilegeTest(false,  // Not incognito
                     false,  // Fail if privilege check is invoked.
                     false,
                     kDefaultProfileName,
                     L"");   // No extra args to initialize.

  // Privileged mode explicitly zero.
  char* argn = "is_privileged";
  char* argv = "0";
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  1, &argn, &argv));
}

TEST_F(TestNPAPIPrivilegedApi, NotPrivilegedDoesNotAllowArgsOrProfile) {
  SetupPrivilegeTest(false,  // Not incognito.
                     true,   // Fail unless privilege check is invoked.
                     false,  // Not privileged.
                     kDefaultProfileName,
                     L"");   // No extra arguments allowed.

  char* argn[] = {
    "privileged_mode",
    "chrome_extra_arguments",
    "chrome_profile_name",
  };
  char *argv[] = {
    "1",
    "foo",
    "bar",
  };
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  arraysize(argn), argn, argv));
}

TEST_F(TestNPAPIPrivilegedApi, PrivilegedAllowsArgsAndProfile) {
  SetupPrivilegeTest(false,  // Not incognito.
                     true,  // Fail unless privilege check is invoked.
                     true,  // Privileged mode.
                     L"custom_profile_name",  // Custom profile expected.
                     L"-bar=far");  // Extra arguments expected

  // With privileged mode we expect automation to be enabled.
  EXPECT_CALL(*mock_automation, SetEnableExtensionAutomation(_))
      .Times(1);

  char* argn[] = {
    "privileged_mode",
    "chrome_extra_arguments",
    "chrome_profile_name",
  };
  char *argv[] = {
    "1",
    "-bar=far",
    "custom_profile_name",
  };
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  arraysize(argn), argn, argv));

  // Since we're mocking out ChromeFrameAutomationClient::Initialize, we need
  // to tickle this explicitly.
  mock_api.OnAutomationServerReady();
}


namespace {

static const NPIdentifier kOnPrivateMessageId =
    reinterpret_cast<NPIdentifier>(0x100);
static const NPIdentifier kPostPrivateMessageId =
    reinterpret_cast<NPIdentifier>(0x100);


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


  void GetPrivilegedStringIdentifiers(const NPUTF8 **names,
                                      int32_t name_count,
                                      NPIdentifier *identifiers) {
    for (int32_t i = 0; i < name_count; ++i) {
      if (0 == strcmp(names[i], "onprivatemessage")) {
        identifiers[i] = kOnPrivateMessageId;
      } else if (0 == strcmp(names[i], "postPrivateMessage")) {
        identifiers[i] = kPostPrivateMessageId;
      } else {
        identifiers[i] = 0;
      }
    }
  }

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

MockNetscapeFuncs* MockNetscapeFuncs::current_ = NULL;
NPNetscapeFuncs MockNetscapeFuncs::netscape_funcs_ = {
  0,   // size
  0,   // version
  NULL,   // geturl
  NULL,   // posturl
  NULL,   // requestread
  NULL,   // newstream
  NULL,   // write
  NULL,   // destroystream
  NULL,   // status
  NULL,   // uagent
  NULL,   // memalloc
  NULL,   // memfree
  NULL,   // memflush
  NULL,   // reloadplugins
  NULL,   // getJavaEnv
  NULL,   // getJavaPeer
  NULL,   // geturlnotify
  NULL,   // posturlnotify
  MockGetValue,   // getvalue
  NULL,   // setvalue
  NULL,   // invalidaterect
  NULL,   // invalidateregion
  NULL,   // forceredraw
  NULL,   // getstringidentifier
  MockGetStringIdentifiers,   // getstringidentifiers
  NULL,   // getintidentifier
  NULL,   // identifierisstring
  NULL,   // utf8fromidentifier
  NULL,   // intfromidentifier
  NULL,   // createobject
  MockRetainObject,   // retainobject
  MockReleaseObject,   // releaseobject
  NULL,   // invoke
  NULL,   // invokeDefault
  NULL,   // evaluate
  NULL,   // getproperty
  NULL,   // setproperty
  NULL,   // removeproperty
  NULL,   // hasproperty
  NULL,   // hasmethod
  NULL,   // releasevariantvalue
  NULL,   // setexception
  NULL,   // pushpopupsenabledstate
  NULL,   // poppopupsenabledstate
  NULL,   // enumerate
  NULL,   // pluginthreadasynccall
  NULL,   // construct
};

NPObject* const kMockNPObject = reinterpret_cast<NPObject*>(0xCafeBabe);

class TestNPAPIPrivilegedProperty: public TestNPAPIPrivilegedApi {
 public:
  virtual void SetUp() {
    TestNPAPIPrivilegedApi::SetUp();
    npapi::InitializeBrowserFunctions(
        const_cast<NPNetscapeFuncs*>(mock_funcs.netscape_funcs()));

    // Expect calls to release and retain objects.
    EXPECT_CALL(mock_funcs, RetainObject(kMockNPObject))
        .WillRepeatedly(Return(kMockNPObject));
    EXPECT_CALL(mock_funcs, ReleaseObject(kMockNPObject))
        .WillRepeatedly(Return());

    // And we should expect SetEnableExtensionAutomation to be called
    // for privileged tests.
    EXPECT_CALL(*mock_automation, SetEnableExtensionAutomation(_))
       .WillRepeatedly(Return());

    // Initializes identifiers.
    EXPECT_CALL(mock_funcs, GetStringIdentifiers(_, _, _))
        .WillRepeatedly(
            Invoke(&mock_funcs,
                   &MockNetscapeFuncs::GetPrivilegedStringIdentifiers));
    MockNPAPI::InitializeIdentifiers();
  }

  virtual void TearDown() {
    npapi::UninitializeBrowserFunctions();
    TestNPAPIPrivilegedApi::TearDown();
  }

 public:
  MockNetscapeFuncs mock_funcs;
};


}  // namespace

TEST_F(TestNPAPIPrivilegedProperty,
       NonPrivilegedOnPrivateMessageInitializationFails) {
  // Attempt setting onprivatemessage when not privileged.
  SetupPrivilegeTest(false,  // not incognito.
                     true,   // expect privilege check.
                     false,   // not privileged.
                     kDefaultProfileName,
                     L"");

  char* on_private_message_str = "onprivatemessage()";
  EXPECT_CALL(mock_api, JavascriptToNPObject(StrEq(on_private_message_str)))
      .Times(0);  // this should not be called.

  char* argn[] = {
    "privileged_mode",
    "onprivatemessage",
  };
  char* argv[] = {
    "1",
    on_private_message_str,
  };
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  arraysize(argn), argn, argv));
  // Shouldn't be able to retrieve it.
  NPVariant var;
  VOID_TO_NPVARIANT(var);
  EXPECT_FALSE(mock_api.GetProperty(kOnPrivateMessageId, &var));
  EXPECT_TRUE(NPVARIANT_IS_VOID(var));

  mock_api.Uninitialize();
}

TEST_F(TestNPAPIPrivilegedProperty,
       PrivilegedOnPrivateMessageInitializationSucceeds) {
  // Set onprivatemessage argument when privileged.
  SetupPrivilegeTest(false,  // not incognito.
                     true,   // expect privilege check.
                     true,   // privileged.
                     kDefaultProfileName,
                     L"");

  char* on_private_message_str = "onprivatemessage()";
  NPObject* on_private_object = kMockNPObject;
  EXPECT_CALL(mock_api, JavascriptToNPObject(StrEq(on_private_message_str)))
      .WillOnce(Return(on_private_object));

  char* argn[] = {
    "privileged_mode",
    "onprivatemessage",
  };
  char* argv[] = {
    "1",
    on_private_message_str,
  };
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  arraysize(argn), argn, argv));
  // The property should have been set, verify that
  // we can retrieve it and test it for correct value.
  NPVariant var;
  VOID_TO_NPVARIANT(var);
  EXPECT_TRUE(mock_api.GetProperty(kOnPrivateMessageId, &var));
  EXPECT_TRUE(NPVARIANT_IS_OBJECT(var));
  EXPECT_EQ(kMockNPObject, NPVARIANT_TO_OBJECT(var));

  mock_api.Uninitialize();
}

TEST_F(TestNPAPIPrivilegedProperty,
       NonPrivilegedOnPrivateMessageAssignmentFails) {
  // Assigning to onprivatemessage when not privileged should fail.
  SetupPrivilegeTest(false,  // not incognito.
                     true,   // expect privilege check.
                     false,   // not privileged.
                     kDefaultProfileName,
                     L"");

  char* argn = "privileged_mode";
  char* argv = "1";
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  1, &argn, &argv));

  NPVariant var = {};
  OBJECT_TO_NPVARIANT(kMockNPObject, var);
  // Setting should fail.
  EXPECT_FALSE(mock_api.SetProperty(kOnPrivateMessageId, &var));

  // And so should getting.
  NULL_TO_NPVARIANT(var);
  EXPECT_FALSE(mock_api.GetProperty(kOnPrivateMessageId, &var));

  mock_api.Uninitialize();
}

TEST_F(TestNPAPIPrivilegedProperty,
       PrivilegedOnPrivateMessageAssignmentSucceeds) {
  // Assigning to onprivatemessage when privileged should succeed.
  SetupPrivilegeTest(false,  // not incognito.
                     true,   // expect privilege check.
                     true,   // privileged.
                     kDefaultProfileName,
                     L"");

  char* argn = "privileged_mode";
  char* argv = "1";
  EXPECT_TRUE(mock_api.Initialize(const_cast<NPMIMEType>(kMimeType),
                                  &instance,
                                  NP_EMBED,
                                  1, &argn, &argv));

  NPVariant var = {};
  VOID_TO_NPVARIANT(var);
  // Getting the property when NULL fails under current implementation.
  // I shouldn't have thought this is correct behavior, e.g. I should
  // have thought retrieving the NULL should succeed, but this is consistent
  // with how other properties behave.
  // TODO(robertshield): investigate and/or fix.
  EXPECT_FALSE(mock_api.GetProperty(kOnPrivateMessageId, &var));
  // EXPECT_TRUE(NPVARIANT_IS_OBJECT(var));
  // EXPECT_EQ(NULL, NPVARIANT_TO_OBJECT(var));

  // Setting the property should succeed.
  OBJECT_TO_NPVARIANT(kMockNPObject, var);
  EXPECT_TRUE(mock_api.SetProperty(kOnPrivateMessageId, &var));

  // And fething it should return the value we just set.
  VOID_TO_NPVARIANT(var);
  EXPECT_TRUE(mock_api.GetProperty(kOnPrivateMessageId, &var));
  EXPECT_TRUE(NPVARIANT_IS_OBJECT(var));
  EXPECT_EQ(kMockNPObject, NPVARIANT_TO_OBJECT(var));

  mock_api.Uninitialize();
}

// TODO(siggi): test invoking postPrivateMessage.

