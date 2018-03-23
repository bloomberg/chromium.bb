// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/permission/permission_request_handler.h"

#include <utility>

#include "android_webview/browser/permission/aw_permission_request.h"
#include "android_webview/browser/permission/aw_permission_request_delegate.h"
#include "android_webview/browser/permission/permission_request_handler_client.h"
#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class TestAwPermissionRequestDelegate : public AwPermissionRequestDelegate {
 public:
  TestAwPermissionRequestDelegate(const GURL& origin,
                                  int64_t resources,
                                  base::RepeatingCallback<void(bool)> callback)
      : origin_(origin),
        resources_(resources),
        callback_(std::move(callback)) {}

  // Get the origin which initiated the permission request.
  const GURL& GetOrigin() override { return origin_; }

  // Get the resources the origin wanted to access.
  int64_t GetResources() override { return resources_; }

  // Notify the permission request is allowed or not.
  void NotifyRequestResult(bool allowed) override { callback_.Run(allowed); }

 private:
  GURL origin_;
  int64_t resources_;
  base::RepeatingCallback<void(bool)> callback_;
};

class TestPermissionRequestHandlerClient
    : public PermissionRequestHandlerClient {
 public:
  struct Permission {
    Permission() : resources(0) {}
    Permission(const GURL& origin, int64_t resources)
        : origin(origin), resources(resources) {}
    GURL origin;
    int64_t resources;
  };

  TestPermissionRequestHandlerClient() : request_(NULL) {}

  void OnPermissionRequest(base::android::ScopedJavaLocalRef<jobject> j_request,
                           AwPermissionRequest* request) override {
    DCHECK(request);
    request_ = request;
    java_request_ = j_request;
    requested_permission_ =
        Permission(request->GetOrigin(), request->GetResources());
  }

  void OnPermissionRequestCanceled(AwPermissionRequest* request) override {
    canceled_permission_ =
        Permission(request->GetOrigin(), request->GetResources());
  }

  AwPermissionRequest* request() { return request_; }

  const Permission& requested_permission() { return requested_permission_; }

  const Permission& canceled_permission() { return canceled_permission_; }

  void Grant() {
    request_->OnAccept(NULL, NULL, true);
    request_->DeleteThis();
    request_ = NULL;
  }

  void Deny() {
    request_->OnAccept(NULL, NULL, false);
    request_->DeleteThis();
    request_ = NULL;
  }

  void Reset() {
    request_ = NULL;
    requested_permission_ = Permission();
    canceled_permission_ = Permission();
  }

 private:
  base::android::ScopedJavaLocalRef<jobject> java_request_;
  AwPermissionRequest* request_;
  Permission requested_permission_;
  Permission canceled_permission_;
};

class TestPermissionRequestHandler : public PermissionRequestHandler {
 public:
  TestPermissionRequestHandler(PermissionRequestHandlerClient* client)
      : PermissionRequestHandler(client, NULL) {}

  const std::vector<base::WeakPtr<AwPermissionRequest>> requests() {
    return requests_;
  }

  void PruneRequests() { return PermissionRequestHandler::PruneRequests(); }
};

class PermissionRequestHandlerTest : public testing::Test {
 public:
  PermissionRequestHandlerTest() : handler_(&client_), allowed_(false) {}

  void NotifyRequestResult(bool allowed) { allowed_ = allowed; }

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    origin_ = GURL("http://www.google.com");
    resources_ =
        AwPermissionRequest::VideoCapture | AwPermissionRequest::AudioCapture;
    delegate_.reset(new TestAwPermissionRequestDelegate(
        origin_, resources_,
        base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                            base::Unretained(this))));
  }

  const GURL& origin() { return origin_; }

  int64_t resources() { return resources_; }

  std::unique_ptr<AwPermissionRequestDelegate> delegate() {
    return std::move(delegate_);
  }

  TestPermissionRequestHandler* handler() { return &handler_; }

  TestPermissionRequestHandlerClient* client() { return &client_; }

  bool allowed() { return allowed_; }

 private:
  GURL origin_;
  int64_t resources_;
  std::unique_ptr<AwPermissionRequestDelegate> delegate_;
  TestPermissionRequestHandlerClient client_;
  TestPermissionRequestHandler handler_;
  bool allowed_;
};

TEST_F(PermissionRequestHandlerTest, TestPermissionGranted) {
  handler()->SendRequest(delegate());
  // Verify Handler store the request correctly.
  ASSERT_EQ(1u, handler()->requests().size());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[0]->GetResources());

  // Verify client's onPermissionRequest was called
  EXPECT_EQ(origin(), client()->request()->GetOrigin());
  EXPECT_EQ(resources(), client()->request()->GetResources());

  // Simulate the grant request.
  client()->Grant();
  // Verify the request is notified as granted
  EXPECT_TRUE(allowed());
  handler()->PruneRequests();
  // Verify the weak reference in handler was removed.
  EXPECT_TRUE(handler()->requests().empty());
}

TEST_F(PermissionRequestHandlerTest, TestPermissionDenied) {
  handler()->SendRequest(delegate());
  // Verify Handler store the request correctly.
  ASSERT_EQ(1u, handler()->requests().size());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[0]->GetResources());

  // Verify client's onPermissionRequest was called
  EXPECT_EQ(origin(), client()->request()->GetOrigin());
  EXPECT_EQ(resources(), client()->request()->GetResources());

  // Simulate the deny request.
  client()->Deny();
  // Verify the request is notified as granted
  EXPECT_FALSE(allowed());
  handler()->PruneRequests();
  // Verify the weak reference in handler was removed.
  EXPECT_TRUE(handler()->requests().empty());
}

TEST_F(PermissionRequestHandlerTest, TestMultiplePermissionRequest) {
  GURL origin1 = GURL("http://a.google.com");
  int64_t resources1 = AwPermissionRequest::Geolocation;

  std::unique_ptr<AwPermissionRequestDelegate> delegate1;
  delegate1.reset(new TestAwPermissionRequestDelegate(
      origin1, resources1,
      base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                          base::Unretained(this))));

  // Send 1st request
  handler()->SendRequest(delegate());
  // Verify Handler store the request correctly.
  ASSERT_EQ(1u, handler()->requests().size());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[0]->GetResources());
  // Verify client's onPermissionRequest was called
  EXPECT_EQ(origin(), client()->request()->GetOrigin());
  EXPECT_EQ(resources(), client()->request()->GetResources());

  // Send 2nd request
  handler()->SendRequest(std::move(delegate1));
  // Verify Handler store the request correctly.
  ASSERT_EQ(2u, handler()->requests().size());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[0]->GetResources());
  EXPECT_EQ(origin1, handler()->requests()[1]->GetOrigin());
  EXPECT_EQ(resources1, handler()->requests()[1]->GetResources());
  // Verify client's onPermissionRequest was called
  EXPECT_EQ(origin1, client()->request()->GetOrigin());
  EXPECT_EQ(resources1, client()->request()->GetResources());

  // Send 3rd request which has same origin and resources as first one.
  delegate1.reset(new TestAwPermissionRequestDelegate(
      origin(), resources(),
      base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                          base::Unretained(this))));
  handler()->SendRequest(std::move(delegate1));
  // Verify Handler store the request correctly.
  ASSERT_EQ(3u, handler()->requests().size());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[0]->GetResources());
  EXPECT_EQ(origin1, handler()->requests()[1]->GetOrigin());
  EXPECT_EQ(resources1, handler()->requests()[1]->GetResources());
  EXPECT_EQ(origin(), handler()->requests()[2]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[2]->GetResources());
  // Verify client's onPermissionRequest was called
  EXPECT_EQ(origin(), client()->request()->GetOrigin());
  EXPECT_EQ(resources(), client()->request()->GetResources());

  // Cancel the request.
  handler()->CancelRequest(origin(), resources());
  // Verify client's OnPermissionRequestCancled() was called.
  EXPECT_EQ(origin(), client()->canceled_permission().origin);
  EXPECT_EQ(resources(), client()->canceled_permission().resources);
  // Verify Handler store the request correctly, the 1st and 3rd were removed.
  handler()->PruneRequests();
  ASSERT_EQ(1u, handler()->requests().size());
  EXPECT_EQ(origin1, handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources1, handler()->requests()[0]->GetResources());
}

TEST_F(PermissionRequestHandlerTest, TestPreauthorizePermission) {
  handler()->PreauthorizePermission(origin(), resources());

  // Permission should granted without asking PermissionRequestHandlerClient.
  handler()->SendRequest(delegate());
  EXPECT_TRUE(allowed());
  EXPECT_EQ(NULL, client()->request());

  // Only ask one preauthorized resource, permission should granted
  // without asking PermissionRequestHandlerClient.
  std::unique_ptr<AwPermissionRequestDelegate> delegate;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin(), AwPermissionRequest::AudioCapture,
      base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                          base::Unretained(this))));
  client()->Reset();
  handler()->SendRequest(std::move(delegate));
  EXPECT_TRUE(allowed());
  EXPECT_EQ(NULL, client()->request());
}

TEST_F(PermissionRequestHandlerTest, TestOriginNotPreauthorized) {
  handler()->PreauthorizePermission(origin(), resources());

  // Ask the origin which wasn't preauthorized.
  GURL origin("http://a.google.com/a/b");
  std::unique_ptr<AwPermissionRequestDelegate> delegate;
  int64_t requested_resources = AwPermissionRequest::AudioCapture;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin, requested_resources,
      base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                          base::Unretained(this))));
  handler()->SendRequest(std::move(delegate));
  EXPECT_EQ(origin, handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(requested_resources, handler()->requests()[0]->GetResources());
  client()->Grant();
  EXPECT_TRUE(allowed());
}

TEST_F(PermissionRequestHandlerTest, TestResourcesNotPreauthorized) {
  handler()->PreauthorizePermission(origin(), resources());

  // Ask the resources which weren't preauthorized.
  std::unique_ptr<AwPermissionRequestDelegate> delegate;
  int64_t requested_resources =
      AwPermissionRequest::AudioCapture | AwPermissionRequest::Geolocation;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin(), requested_resources,
      base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                          base::Unretained(this))));

  handler()->SendRequest(std::move(delegate));
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(requested_resources, handler()->requests()[0]->GetResources());
  client()->Deny();
  EXPECT_FALSE(allowed());
}

TEST_F(PermissionRequestHandlerTest, TestPreauthorizeMultiplePermission) {
  handler()->PreauthorizePermission(origin(), resources());
  // Preauthorize another permission.
  GURL origin("http://a.google.com/a/b");
  handler()->PreauthorizePermission(origin, AwPermissionRequest::Geolocation);
  GURL origin_hostname("http://a.google.com/");
  std::unique_ptr<AwPermissionRequestDelegate> delegate;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin_hostname, AwPermissionRequest::Geolocation,
      base::BindRepeating(&PermissionRequestHandlerTest::NotifyRequestResult,
                          base::Unretained(this))));
  handler()->SendRequest(std::move(delegate));
  EXPECT_TRUE(allowed());
  EXPECT_EQ(NULL, client()->request());
}

}  // namespace android_webview
