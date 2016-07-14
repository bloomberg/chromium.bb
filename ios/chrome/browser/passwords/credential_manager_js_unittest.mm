// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "ios/chrome/browser/passwords/js_credential_manager.h"
#include "ios/web/public/web_state/credential.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace {

using ::testing::_;

// Matcher to match web::Credential.
MATCHER_P(IsEqualTo, value, "") {
  return arg.type == value.type && arg.id == value.id &&
         arg.name == value.name && arg.avatar_url == value.avatar_url &&
         arg.password == value.password &&
         arg.federation_origin.Serialize() ==
             value.federation_origin.Serialize();
}

// A mock WebStateObserver for testing the Credential Manager API.
class MockWebStateObserver : public web::WebStateObserver {
 public:
  explicit MockWebStateObserver(web::WebState* web_state)
      : web::WebStateObserver(web_state) {}
  ~MockWebStateObserver() override {}

  MOCK_METHOD5(
      CredentialsRequested,
      void(int, const GURL&, bool, const std::vector<std::string>&, bool));
  MOCK_METHOD3(SignedIn, void(int, const GURL&, const web::Credential&));
  MOCK_METHOD2(SignedIn, void(int, const GURL&));
  MOCK_METHOD2(SignedOut, void(int, const GURL&));
  MOCK_METHOD3(SignInFailed, void(int, const GURL&, const web::Credential&));
  MOCK_METHOD2(SignInFailed, void(int, const GURL&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebStateObserver);
};

// Unit tests for the Credential Manager JavaScript and associated plumbing.
class CredentialManagerJsTest : public web::WebTestWithWebState {
 public:
  CredentialManagerJsTest() {}

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    js_credential_manager_.reset(base::mac::ObjCCastStrict<JSCredentialManager>(
        [[web_state()->GetJSInjectionReceiver()
            instanceOfClass:[JSCredentialManager class]] retain]));
    observer_.reset(new MockWebStateObserver(web_state()));
  }

  // Sets up a web page and injects the JSCredentialManager. Must be called
  // before any interaction with the page.
  void Inject() {
    LoadHtml(@"");
    [js_credential_manager_ inject];
  }

  // Returns the mock observer.
  MockWebStateObserver& observer() { return *observer_; }

  // Returns a string that creates a Credential object for JavaScript testing.
  NSString* test_credential_js() {
    return @"new PasswordCredential('bob', 'bobiscool', 'Bob Boblaw',"
           @"'https://bobboblawslawblog.com/bob.jpg')";
  }

  // Returns a Credential to match the one returned by |test_credential_js()|.
  web::Credential test_credential() {
    web::Credential test_credential;
    test_credential.type = web::CredentialType::CREDENTIAL_TYPE_PASSWORD;
    test_credential.id = base::ASCIIToUTF16("bob");
    test_credential.password = base::ASCIIToUTF16("bobiscool");
    test_credential.name = base::ASCIIToUTF16("Bob Boblaw");
    test_credential.avatar_url = GURL("https://bobboblawslawblog.com/bob.jpg");
    return test_credential;
  }

  // Adds handlers for resolving and rejecting the promise returned by
  // executing the code in |promise|.
  void PrepareResolverAndRejecter(NSString* promise) {
    EvaluateJavaScriptAsString(
        [NSString stringWithFormat:@"var resolved = false;             "
                                   @"var rejected = false;             "
                                   @"var resolvedCredential = null;    "
                                   @"var rejectedError = null;         "
                                   @"function resolve(credential) {    "
                                   @"  resolved = true;                "
                                   @"  resolvedCredential = credential;"
                                   @"}                                 "
                                   @"function reject(error) {          "
                                   @"  rejected = true;                "
                                   @"  rejectedError = error;          "
                                   @"}                                 "
                                   @"%@.then(resolve, reject);         ",
                                   promise]);
    // Wait until the promise executor has executed.
    WaitForCondition(^bool {
      return [EvaluateJavaScriptAsString(
          @"Object.keys(__gCrWeb.credentialManager.resolvers_).length > 0")
          isEqualToString:@"true"];
    });
  }

  // Checks that the Credential returned to the resolve handler in JavaScript
  // matches the structure of |test_credential()|.
  void CheckResolvedCredentialMatchesTestCredential() {
    EXPECT_NSEQ(@"true", EvaluateJavaScriptAsString(@"resolved"));
    EXPECT_NSEQ(
        @"PasswordCredential",
        EvaluateJavaScriptAsString(@"resolvedCredential.constructor.name"));
    EXPECT_NSEQ(@"bob", EvaluateJavaScriptAsString(@"resolvedCredential.id"));
    EXPECT_NSEQ(@"bobiscool",
                EvaluateJavaScriptAsString(@"resolvedCredential.password_"));
    EXPECT_NSEQ(@"Bob Boblaw",
                EvaluateJavaScriptAsString(@"resolvedCredential.name"));
    EXPECT_NSEQ(@"https://bobboblawslawblog.com/bob.jpg",
                EvaluateJavaScriptAsString(@"resolvedCredential.avatarURL"));
  }

  // Checks that the promise set up by |PrepareResolverAndRejecter| was resolved
  // without a credential.
  void CheckResolvedWithoutCredential() {
    EXPECT_NSEQ(@"true", EvaluateJavaScriptAsString(@"resolved"));
    EXPECT_NSEQ(@"false", EvaluateJavaScriptAsString(@"!!resolvedCredential"));
  }

  // Checks that the promise set up by |PrepareResolverAndRejecter| was rejected
  // with an error with name |error_name| and |message|.
  void CheckRejected(NSString* error_name, NSString* message) {
    EXPECT_NSEQ(@"true", EvaluateJavaScriptAsString(@"rejected"));
    EXPECT_NSEQ(error_name, EvaluateJavaScriptAsString(@"rejectedError.name"));
    EXPECT_NSEQ(message, EvaluateJavaScriptAsString(@"rejectedError.message"));
  }

  // Waits until the promise set up by |PrepareResolverAndRejecter| has been
  // either resolved or rejected.
  void WaitUntilPromiseResolvedOrRejected() {
    WaitForCondition(^bool {
      return [EvaluateJavaScriptAsString(@"resolved || rejected")
          isEqualToString:@"true"];
    });
  }

  // Resolves the promise set up by |PrepareResolverAndRejecter| and associated
  // with |request_id| with |test_credential()|.
  void ResolvePromiseWithTestCredential(int request_id) {
    __block bool finished = false;
    [js_credential_manager() resolvePromiseWithRequestID:request_id
                                              credential:test_credential()
                                       completionHandler:^(BOOL success) {
                                         EXPECT_TRUE(success);
                                         finished = true;
                                       }];
    WaitForCondition(^bool {
      return finished;
    });
    WaitUntilPromiseResolvedOrRejected();
  }

  // Resolves the promise set up by |PrepareResolverAndRejecter| and associated
  // with |request_id| without a credential.
  void ResolvePromiseWithoutCredential(int request_id) {
    __block bool finished = false;
    [js_credential_manager() resolvePromiseWithRequestID:request_id
                                       completionHandler:^(BOOL success) {
                                         EXPECT_TRUE(success);
                                         finished = true;
                                       }];
    WaitForCondition(^bool {
      return finished;
    });
    WaitUntilPromiseResolvedOrRejected();
  }

  // Rejects the promise set up by |PrepareResolverAndRejecter| and associated
  // with |request_id| with an error of type |error_type| and |message|.
  void RejectPromise(int request_id, NSString* error_type, NSString* message) {
    __block bool finished = false;
    [js_credential_manager() rejectPromiseWithRequestID:request_id
                                              errorType:error_type
                                                message:message
                                      completionHandler:^(BOOL success) {
                                        EXPECT_TRUE(success);
                                        finished = true;
                                      }];
    WaitForCondition(^bool {
      return finished;
    });
    WaitUntilPromiseResolvedOrRejected();
  }

  // Tests that the promise set up by |PrepareResolverAndRejecter| wasn't
  // rejected.
  void CheckNeverRejected() {
    EXPECT_NSEQ(@"false", EvaluateJavaScriptAsString(@"rejected"));
  }

  // Tests that the promise set up by |PrepareResolverAndRejecter| wasn't
  // resolved.
  void CheckNeverResolved() {
    EXPECT_NSEQ(@"false", EvaluateJavaScriptAsString(@"resolved"));
  }

  // Returns the JSCredentialManager for testing.
  JSCredentialManager* js_credential_manager() {
    return js_credential_manager_;
  }

  // Tests that resolving the promise returned by |promise| and associated with
  // |request_id| with |test_credential()| correctly forwards that credential
  // to the client.
  void TestPromiseResolutionWithCredential(int request_id, NSString* promise) {
    PrepareResolverAndRejecter(promise);
    ResolvePromiseWithTestCredential(request_id);
    CheckResolvedCredentialMatchesTestCredential();
    CheckNeverRejected();
  }

  // Tests that resolving the promise returned by |promise| and associated with
  // |request_id| without a credential correctly invokes the client.
  void TestPromiseResolutionWithoutCredential(int request_id,
                                              NSString* promise) {
    PrepareResolverAndRejecter(promise);
    ResolvePromiseWithoutCredential(request_id);
    CheckResolvedWithoutCredential();
    CheckNeverRejected();
  }

  // Tests that rejecting the promise returned by |promise| and associated with
  // |request_id| with an error of type |error| and message |message| correctly
  // forwards that error to the client.
  void TestPromiseRejection(int request_id,
                            NSString* error,
                            NSString* message,
                            NSString* promise) {
    PrepareResolverAndRejecter(promise);
    RejectPromise(request_id, error, message);
    CheckRejected(error, message);
    CheckNeverResolved();
  }

 private:
  // Manager for injected credential manager JavaScript.
  base::scoped_nsobject<JSCredentialManager> js_credential_manager_;

  // Mock observer for testing.
  std::unique_ptr<MockWebStateObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerJsTest);
};

// Tests that navigator.credentials calls use distinct request identifiers.
TEST_F(CredentialManagerJsTest, RequestIdentifiersDiffer) {
  Inject();
  EXPECT_CALL(observer(), CredentialsRequested(0, _, _, _, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.request()");
  EXPECT_CALL(observer(), SignInFailed(1, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifyFailedSignIn()");
  EXPECT_CALL(observer(), SignInFailed(2, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifyFailedSignIn()");
  EXPECT_CALL(observer(), SignedIn(3, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifySignedIn()");
  EXPECT_CALL(observer(), SignedOut(4, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifySignedOut()");
  EXPECT_CALL(observer(), CredentialsRequested(5, _, _, _, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.request()");
}

// Tests that navigator.credentials.request() creates and forwards the right
// arguments to the app side.
// TODO(rohitrao): Fails after merge r376674.  https://crbug.com/588706.
TEST_F(CredentialManagerJsTest, DISABLED_RequestToApp) {
  Inject();
  std::vector<std::string> empty_federations;
  std::vector<std::string> nonempty_federations;
  nonempty_federations.push_back("foo");
  nonempty_federations.push_back("bar");

  EXPECT_CALL(observer(),
              CredentialsRequested(0, _, false, empty_federations, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.request()");

  EXPECT_CALL(observer(),
              CredentialsRequested(1, _, false, empty_federations, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.request({})");

  EXPECT_CALL(observer(),
              CredentialsRequested(2, _, true, empty_federations, _));
  EvaluateJavaScriptAsString(
      @"navigator.credentials.request({suppressUI: true})");

  EXPECT_CALL(observer(),
              CredentialsRequested(3, _, false, nonempty_federations, _));
  EvaluateJavaScriptAsString(
      @"navigator.credentials.request({federations: ['foo', 'bar']})");

  EXPECT_CALL(observer(),
              CredentialsRequested(4, _, true, nonempty_federations, _));
  EvaluateJavaScriptAsString(
      @"navigator.credentials.request("
      @"    { suppressUI: true, federations: ['foo', 'bar'] })");

  EXPECT_CALL(observer(),
              CredentialsRequested(5, _, false, empty_federations, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.request("
                             @"    { suppressUI: false, federations: [] })");
}

// Tests that navigator.credentials.notifySignedIn() creates and forwards the
// right arguments to the app side.
TEST_F(CredentialManagerJsTest, NotifySignedInToApp) {
  Inject();
  EXPECT_CALL(observer(), SignedIn(0, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifySignedIn()");

  EXPECT_CALL(observer(), SignedIn(1, _, IsEqualTo(test_credential())));
  EvaluateJavaScriptAsString(
      [NSString stringWithFormat:@"navigator.credentials.notifySignedIn(%@)",
                                 test_credential_js()]);
}

// Tests that navigator.credentials.notifySignedOut() creates and forwards the
// right arguments to the app side.
TEST_F(CredentialManagerJsTest, NotifySignedOutToApp) {
  Inject();
  EXPECT_CALL(observer(), SignedOut(0, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifySignedOut()");
}

// Tests that navigator.credentials.notifyFailedSignIn() creates and forwards
// the right arguments to the app side.
TEST_F(CredentialManagerJsTest, NotifyFailedSignInToApp) {
  Inject();
  EXPECT_CALL(observer(), SignInFailed(0, _));
  EvaluateJavaScriptAsString(@"navigator.credentials.notifyFailedSignIn()");

  EXPECT_CALL(observer(), SignInFailed(1, _, IsEqualTo(test_credential())));
  EvaluateJavaScriptAsString([NSString
      stringWithFormat:@"navigator.credentials.notifyFailedSignIn(%@)",
                       test_credential_js()]);
}

// Tests that resolving the promise returned by a call to
// navigator.credentials.request() with a credential correctly forwards that
// credential to the client.
TEST_F(CredentialManagerJsTest, ResolveRequestPromiseWithCredential) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), CredentialsRequested(request_id, _, _, _, _));
  TestPromiseResolutionWithCredential(request_id,
                                      @"navigator.credentials.request()");
}

// Tests that resolving the promise returned by a call to
// navigator.credentials.request() without a credential correctly invokes the
// client handler.
TEST_F(CredentialManagerJsTest, ResolveRequestPromiseWithoutCredential) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), CredentialsRequested(request_id, _, _, _, _));
  TestPromiseResolutionWithoutCredential(request_id,
                                         @"navigator.credentials.request()");
}

// Tests that resolving the promise returned by a call to
// navigator.credentials.notifySignedIn() without a credential correctly invokes
// the client handler.
TEST_F(CredentialManagerJsTest, ResolveNotifySignedInPromiseWithoutCredential) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignedIn(request_id, _));
  TestPromiseResolutionWithoutCredential(
      request_id, @"navigator.credentials.notifySignedIn()");
}

// Tests that resolving the promise returned by a call to
// navigator.credentials.notifyFailedSignIn() without a credential correctly
// invokes the client handler.
TEST_F(CredentialManagerJsTest,
       ResolveNotifyFailedSignInPromiseWithoutCredential) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignInFailed(request_id, _));
  TestPromiseResolutionWithoutCredential(
      request_id, @"navigator.credentials.notifyFailedSignIn()");
}

// Tests that resolving the promise returned by a call to
// navigator.credentials.notifyFailedSignIn() without a credential correctly
// invokes the client handler.
TEST_F(CredentialManagerJsTest,
       ResolveNotifySignedOutPromiseWithoutCredential) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignedOut(request_id, _));
  TestPromiseResolutionWithoutCredential(
      request_id, @"navigator.credentials.notifySignedOut()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.request() with a InvalidStateError correctly forwards
// that error to the client.
TEST_F(CredentialManagerJsTest, RejectRequestPromiseWithInvalidStateError) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), CredentialsRequested(request_id, _, _, _, _));
  TestPromiseRejection(request_id, @"InvalidStateError", @"foo",
                       @"navigator.credentials.request()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.notifySignedIn() with a InvalidStateError correctly
// forwards that error to the client.
TEST_F(CredentialManagerJsTest,
       RejectNotifySignedInPromiseWithInvalidStateError) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignedIn(request_id, _));
  TestPromiseRejection(request_id, @"InvalidStateError", @"foo",
                       @"navigator.credentials.notifySignedIn()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.notifyFailedSignIn() with a InvalidStateError correctly
// forwards that error to the client.
TEST_F(CredentialManagerJsTest,
       RejectNotifyFailedSignInPromiseWithInvalidStateError) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignInFailed(request_id, _));
  TestPromiseRejection(request_id, @"InvalidStateError", @"foo",
                       @"navigator.credentials.notifyFailedSignIn()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.notifySignedOut() with a InvalidStateError correctly
// forwards that error to the client.
TEST_F(CredentialManagerJsTest,
       RejectNotifySignedOutPromiseWithInvalidStateError) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignedOut(request_id, _));
  TestPromiseRejection(request_id, @"InvalidStateError", @"foo",
                       @"navigator.credentials.notifySignedOut()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.request() with a SecurityError correctly forwards that
// error to the client.
TEST_F(CredentialManagerJsTest, RejectRequestPromiseWithSecurityError) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), CredentialsRequested(request_id, _, _, _, _));
  TestPromiseRejection(request_id, @"SecurityError", @"foo",
                       @"navigator.credentials.request()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.notifySignedIn() with a SecurityError correctly
// forwards that error to the client.
TEST_F(CredentialManagerJsTest, RejectNotifySignedInPromiseWithSecurityError) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignedIn(request_id, _));
  TestPromiseRejection(request_id, @"SecurityError", @"foo",
                       @"navigator.credentials.notifySignedIn()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.notifyFailedSignIn() with a SecurityError correctly
// forwards that error to the client.
TEST_F(CredentialManagerJsTest,
       RejectPromiseWithSecurityError_notifyFailedSignIn) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignInFailed(request_id, _));
  TestPromiseRejection(request_id, @"SecurityError", @"foo",
                       @"navigator.credentials.notifyFailedSignIn()");
}

// Tests that rejecting the promise returned by a call to
// navigator.credentials.notifySignedOut() with a SecurityError correctly
// forwards that error to the client.
TEST_F(CredentialManagerJsTest,
       RejectPromiseWithSecurityError_notifySignedOut) {
  Inject();
  const int request_id = 0;
  EXPECT_CALL(observer(), SignedOut(request_id, _));
  TestPromiseRejection(request_id, @"SecurityError", @"foo",
                       @"navigator.credentials.notifySignedOut()");
}

}  // namespace
