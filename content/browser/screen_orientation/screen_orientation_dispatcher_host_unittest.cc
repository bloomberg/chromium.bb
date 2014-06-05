// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockScreenOrientationProvider : public ScreenOrientationProvider {
 public:
  MockScreenOrientationProvider()
      : orientation_(blink::WebScreenOrientationLockPortraitPrimary),
        unlock_called_(false) {}

  virtual void LockOrientation(blink::WebScreenOrientationLockType orientation)
      OVERRIDE {
    orientation_ = orientation;
  }

  virtual void UnlockOrientation() OVERRIDE {
    unlock_called_ = true;
  }

  blink::WebScreenOrientationLockType orientation() const {
    return orientation_;
  }

  bool unlock_called() const {
    return unlock_called_;
  }

  virtual ~MockScreenOrientationProvider() {}

 private:
  blink::WebScreenOrientationLockType orientation_;
  bool unlock_called_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationProvider);
};

class ScreenOrientationDispatcherHostWithSink FINAL :
    public ScreenOrientationDispatcherHost {
 public:
  explicit ScreenOrientationDispatcherHostWithSink(IPC::TestSink* sink)
      : ScreenOrientationDispatcherHost() , sink_(sink) {}

  virtual bool Send(IPC::Message* message) OVERRIDE {
    return sink_->Send(message);
  }

 private:
  virtual ~ScreenOrientationDispatcherHostWithSink() { }

  IPC::TestSink* sink_;
};

class ScreenOrientationDispatcherHostTest : public testing::Test {
 protected:
  virtual ScreenOrientationDispatcherHost* CreateDispatcher() {
    return new ScreenOrientationDispatcherHost();
  }

  virtual void SetUp() OVERRIDE {
    provider_ = new MockScreenOrientationProvider();

    dispatcher_ = CreateDispatcher();
    dispatcher_->SetProviderForTests(provider_);
  }

  // The dispatcher_ owns the provider_ but we still want to access it.
  MockScreenOrientationProvider* provider_;
  scoped_refptr<ScreenOrientationDispatcherHost> dispatcher_;
};

class ScreenOrientationDispatcherHostWithSinkTest :
    public ScreenOrientationDispatcherHostTest {
 protected:
  virtual ScreenOrientationDispatcherHost* CreateDispatcher() OVERRIDE {
    return new ScreenOrientationDispatcherHostWithSink(&sink_);
  }

  const IPC::TestSink& sink() const {
    return sink_;
  }

  IPC::TestSink sink_;
};

// Test that a NULL provider is correctly handled.
TEST_F(ScreenOrientationDispatcherHostTest, NullProvider) {
  dispatcher_->SetProviderForTests(NULL);

  bool message_was_handled = dispatcher_->OnMessageReceived(
      ScreenOrientationHostMsg_LockRequest(
          blink::WebScreenOrientationLockPortraitPrimary, 0));
  EXPECT_TRUE(message_was_handled);
}

// Test that when receiving a lock message, it is correctly dispatched to the
// ScreenOrientationProvider.
TEST_F(ScreenOrientationDispatcherHostTest, ProviderLock) {
  // If we change this array, update |orientationsToTestCount| below.
  blink::WebScreenOrientationLockType orientationsToTest[] = {
    blink::WebScreenOrientationLockPortraitPrimary,
    blink::WebScreenOrientationLockPortraitSecondary,
    blink::WebScreenOrientationLockLandscapePrimary,
    blink::WebScreenOrientationLockLandscapeSecondary,
    blink::WebScreenOrientationLockPortrait,
    blink::WebScreenOrientationLockLandscapePrimary,
    blink::WebScreenOrientationLockAny
  };

  // Unfortunately, initializer list constructor for std::list is not yet
  // something we can use.
  // Keep this in sync with |orientationsToTest|.
  int orientationsToTestCount = 7;

  for (int i = 0; i < orientationsToTestCount; ++i) {
    bool message_was_handled = false;
    blink::WebScreenOrientationLockType orientation = orientationsToTest[i];

    message_was_handled = dispatcher_->OnMessageReceived(
        ScreenOrientationHostMsg_LockRequest(orientation, 0));

    EXPECT_TRUE(message_was_handled);
    EXPECT_EQ(orientation, provider_->orientation());
  }
}

// Test that when receiving an unlock message, it is correctly dispatched to the
// ScreenOrientationProvider.
TEST_F(ScreenOrientationDispatcherHostTest, ProviderUnlock) {
    bool message_was_handled = dispatcher_->OnMessageReceived(
        ScreenOrientationHostMsg_Unlock());

    EXPECT_TRUE(message_was_handled);
    EXPECT_TRUE(provider_->unlock_called());
}

// Test that when there is no provider, a LockRequest fails with the appropriate
// ErrorType.
TEST_F(ScreenOrientationDispatcherHostWithSinkTest, NoProvider_LockError) {
  dispatcher_->SetProviderForTests(NULL);

  const int request_id = 3;
  dispatcher_->OnMessageReceived(ScreenOrientationHostMsg_LockRequest(
      blink::WebScreenOrientationLockPortraitPrimary, request_id));

  EXPECT_EQ(1u, sink().message_count());

  const IPC::Message* msg = sink().GetFirstMessageMatching(
      ScreenOrientationMsg_LockError::ID);
  EXPECT_TRUE(msg != NULL);

  Tuple2<int, blink::WebLockOrientationCallback::ErrorType> params;
  ScreenOrientationMsg_LockError::Read(msg, &params);
  EXPECT_EQ(request_id, params.a);
  EXPECT_EQ(blink::WebLockOrientationCallback::ErrorTypeNotAvailable, params.b);
}

// Test that when there is a provider, we always send a success response back to
// the renderer.
// TODO(mlamouri): we currently do not test the content of the message because
// it currently contains dummy values.
TEST_F(ScreenOrientationDispatcherHostWithSinkTest, WithProvider_LockSuccess) {
  const int request_id = 42;
  dispatcher_->OnMessageReceived(ScreenOrientationHostMsg_LockRequest(
      blink::WebScreenOrientationLockPortraitPrimary, request_id));

  EXPECT_EQ(1u, sink().message_count());

  const IPC::Message* msg = sink().GetFirstMessageMatching(
      ScreenOrientationMsg_LockSuccess::ID);
  EXPECT_TRUE(msg != NULL);

  Tuple3<int, unsigned, blink::WebScreenOrientationType> params;
  ScreenOrientationMsg_LockSuccess::Read(msg, &params);
  EXPECT_EQ(request_id, params.a);
}

} // namespace content
