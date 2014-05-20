// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/permission/aw_permission_request.h"
#include "android_webview/native/permission/aw_permission_request_delegate.h"
#include "android_webview/native/permission/permission_request_handler.h"
#include "android_webview/native/permission/permission_request_handler_client.h"
#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class TestAwPermissionRequestDelegate : public AwPermissionRequestDelegate {
 public:
  TestAwPermissionRequestDelegate(
      const GURL& origin, int64 resources, base::Callback<void(bool)> callback)
      : origin_(origin),
        resources_(resources),
        callback_(callback) {}

  // Get the origin which initiated the permission request.
  virtual const GURL& GetOrigin() OVERRIDE {
    return origin_;
  }

  // Get the resources the origin wanted to access.
  virtual int64 GetResources() OVERRIDE {
    return resources_;
  }

  // Notify the permission request is allowed or not.
  virtual void NotifyRequestResult(bool allowed) OVERRIDE {
    callback_.Run(allowed);
  }

 private:
  GURL origin_;
  int64 resources_;
  base::Callback<void(bool)> callback_;
};

class TestPermissionRequestHandlerClient :
      public PermissionRequestHandlerClient {
 public:
  struct Permission {
    Permission()
        :resources(0) {}
    Permission(const GURL& origin, int64 resources)
        : origin(origin),
          resources(resources) {}
    GURL origin;
    int64 resources;
  };

  TestPermissionRequestHandlerClient()
      : request_(NULL) {}

  virtual void OnPermissionRequest(AwPermissionRequest* request) OVERRIDE {
    request_ = request;
    requested_permission_ =
        Permission(request->GetOrigin(), request->GetResources());
  }

  virtual void OnPermissionRequestCanceled(
      AwPermissionRequest* request) OVERRIDE{
    canceled_permission_ =
        Permission(request->GetOrigin(), request->GetResources());
  }

  AwPermissionRequest* request() {
    return request_;
  }

  const Permission& requested_permission() {
    return requested_permission_;
  }

  const Permission& canceled_permission() {
    return canceled_permission_;
  }

  void Grant() {
    request_->OnAccept(NULL, NULL, true);
    request_ = NULL;
  }

  void Deny() {
    request_->OnAccept(NULL, NULL, false);
    request_ = NULL;
  }

  void Reset() {
    request_ = NULL;
    requested_permission_ = Permission();
    canceled_permission_ = Permission();
  }

 private:
  AwPermissionRequest* request_;
  Permission requested_permission_;
  Permission canceled_permission_;
};

class TestPermissionRequestHandler : public PermissionRequestHandler {
 public:
  TestPermissionRequestHandler(PermissionRequestHandlerClient* client)
      : PermissionRequestHandler(client, NULL) {
  }

  const std::vector<base::WeakPtr<AwPermissionRequest> > requests() {
    return requests_;
  }

  void PruneRequests() {
    return PermissionRequestHandler::PruneRequests();
  }
};

class PermissionRequestHandlerTest : public testing::Test {
 public:
  PermissionRequestHandlerTest()
      : handler_(&client_),
        allowed_(false) {}

  void NotifyRequestResult(bool allowed) {
    allowed_ = allowed;
  }

 protected:
  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    origin_ = GURL("http://www.google.com");
    resources_ =
        AwPermissionRequest::VideoCapture | AwPermissionRequest::AudioCapture;
    delegate_.reset(
        new TestAwPermissionRequestDelegate(
            origin_, resources_, base::Bind(
                &PermissionRequestHandlerTest::NotifyRequestResult,
                base::Unretained(this))));
  }

  const GURL& origin() {
    return origin_;
  }

  int64 resources() {
    return resources_;
  }

  scoped_ptr<AwPermissionRequestDelegate> delegate() {
    return delegate_.Pass();
  }

  TestPermissionRequestHandler* handler() {
    return  &handler_;
  }

  TestPermissionRequestHandlerClient* client() {
    return &client_;
  }

  bool allowed() {
    return allowed_;
  }

 private:
  GURL origin_;
  int64 resources_;
  scoped_ptr<AwPermissionRequestDelegate> delegate_;
  TestPermissionRequestHandlerClient client_;
  TestPermissionRequestHandler handler_;
  bool allowed_;
};

TEST_F(PermissionRequestHandlerTest, TestPermissionGranted) {
  handler()->SendRequest(delegate().Pass());
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
  handler()->SendRequest(delegate().Pass());
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
  int64 resources1 = AwPermissionRequest::Geolocation;

  scoped_ptr<AwPermissionRequestDelegate> delegate1;
  delegate1.reset(new TestAwPermissionRequestDelegate(
      origin1, resources1,
      base::Bind(&PermissionRequestHandlerTest::NotifyRequestResult,
                 base::Unretained(this))));

  // Send 1st request
  handler()->SendRequest(delegate().Pass());
  // Verify Handler store the request correctly.
  ASSERT_EQ(1u, handler()->requests().size());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(resources(), handler()->requests()[0]->GetResources());
  // Verify client's onPermissionRequest was called
  EXPECT_EQ(origin(), client()->request()->GetOrigin());
  EXPECT_EQ(resources(), client()->request()->GetResources());

  // Send 2nd request
  handler()->SendRequest(delegate1.Pass());
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
  delegate1.reset(new TestAwPermissionRequestDelegate(origin(), resources(),
      base::Bind(&PermissionRequestHandlerTest::NotifyRequestResult,
                 base::Unretained(this))));
  handler()->SendRequest(delegate1.Pass());
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
  handler()->SendRequest(delegate().Pass());
  EXPECT_TRUE(allowed());
  EXPECT_EQ(NULL, client()->request());

  // Only ask one preauthorized resource, permission should granted
  // without asking PermissionRequestHandlerClient.
  scoped_ptr<AwPermissionRequestDelegate> delegate;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin(), AwPermissionRequest::AudioCapture,
      base::Bind(&PermissionRequestHandlerTest::NotifyRequestResult,
                 base::Unretained(this))));
  client()->Reset();
  handler()->SendRequest(delegate.Pass());
  EXPECT_TRUE(allowed());
  EXPECT_EQ(NULL, client()->request());
}

TEST_F(PermissionRequestHandlerTest, TestOriginNotPreauthorized) {
  handler()->PreauthorizePermission(origin(), resources());

  // Ask the origin which wasn't preauthorized.
  GURL origin ("http://a.google.com/a/b");
  scoped_ptr<AwPermissionRequestDelegate> delegate;
  int64 requested_resources = AwPermissionRequest::AudioCapture;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin, requested_resources,
      base::Bind(&PermissionRequestHandlerTest::NotifyRequestResult,
                 base::Unretained(this))));
  handler()->SendRequest(delegate.Pass());
  EXPECT_EQ(origin, handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(requested_resources, handler()->requests()[0]->GetResources());
  client()->Grant();
  EXPECT_TRUE(allowed());
}

TEST_F(PermissionRequestHandlerTest, TestResourcesNotPreauthorized) {
  handler()->PreauthorizePermission(origin(), resources());

  // Ask the resources which weren't preauthorized.
  scoped_ptr<AwPermissionRequestDelegate> delegate;
  int64 requested_resources = AwPermissionRequest::AudioCapture
    | AwPermissionRequest::Geolocation;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin(), requested_resources,
      base::Bind(&PermissionRequestHandlerTest::NotifyRequestResult,
                 base::Unretained(this))));

  handler()->SendRequest(delegate.Pass());
  EXPECT_EQ(origin(), handler()->requests()[0]->GetOrigin());
  EXPECT_EQ(requested_resources, handler()->requests()[0]->GetResources());
  client()->Deny();
  EXPECT_FALSE(allowed());
}

TEST_F(PermissionRequestHandlerTest, TestPreauthorizeMultiplePermission) {
  handler()->PreauthorizePermission(origin(), resources());
  // Preauthorize another permission.
  GURL origin ("http://a.google.com/a/b");
  handler()->PreauthorizePermission(origin, AwPermissionRequest::Geolocation);
  GURL origin_hostname ("http://a.google.com/");
  scoped_ptr<AwPermissionRequestDelegate> delegate;
  delegate.reset(new TestAwPermissionRequestDelegate(
      origin_hostname, AwPermissionRequest::Geolocation,
      base::Bind(&PermissionRequestHandlerTest::NotifyRequestResult,
                 base::Unretained(this))));
  handler()->SendRequest(delegate.Pass());
  EXPECT_TRUE(allowed());
  EXPECT_EQ(NULL, client()->request());
}

}  // android_webview
