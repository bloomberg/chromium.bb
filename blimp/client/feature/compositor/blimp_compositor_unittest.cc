// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor.h"

#include "base/lazy_instance.h"
#include "base/thread_task_runner_handle.h"
#include "cc/proto/compositor_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {

class MockRenderWidgetFeature : public RenderWidgetFeature {
 public:
  MOCK_METHOD2(SendCompositorMessage,
               void(const int,
                    const cc::proto::CompositorMessage&));
  MOCK_METHOD2(SendInputEvent, void(const int,
                                    const blink::WebInputEvent&));
  MOCK_METHOD2(SetDelegate, void(int, RenderWidgetFeatureDelegate*));
  MOCK_METHOD1(RemoveDelegate, void(const int));
};

class BlimpCompositorForTesting : public BlimpCompositor {
 public:
  BlimpCompositorForTesting(float dp_to_px,
                            RenderWidgetFeature* render_widget_feature)
  : BlimpCompositor(dp_to_px, render_widget_feature) {}

  cc::LayerTreeHost* host() const { return host_.get(); }
};

class BlimpCompositorTest : public testing::Test {
 public:
  BlimpCompositorTest():
    loop_(new base::MessageLoop),
    window_(42u) {}

  void SetUp() override {
    EXPECT_CALL(render_widget_feature_, SetDelegate(_, _)).Times(1);
    EXPECT_CALL(render_widget_feature_, RemoveDelegate(_)).Times(1);

    compositor_.reset(new BlimpCompositorForTesting(
        1.0f, &render_widget_feature_));
  }

  void TearDown() override {
    compositor_.reset();
  }

  ~BlimpCompositorTest() override {}

  RenderWidgetFeature::RenderWidgetFeatureDelegate* delegate() const {
    DCHECK(compositor_);
    return static_cast<RenderWidgetFeature::RenderWidgetFeatureDelegate*>
        (compositor_.get());
  }

  void SendInitializeMessage() {
    scoped_ptr<cc::proto::CompositorMessage> message;
    message.reset(new cc::proto::CompositorMessage);
    cc::proto::CompositorMessageToImpl* to_impl =
        message->mutable_to_impl();
    to_impl->set_message_type(
        cc::proto::CompositorMessageToImpl::INITIALIZE_IMPL);
    cc::proto::InitializeImpl* initialize_message =
        to_impl->mutable_initialize_impl_message();
    cc::LayerTreeSettings settings;
    settings.ToProtobuf(initialize_message->mutable_layer_tree_settings());
    delegate()->OnCompositorMessageReceived(std::move(message));
  }

  void SendShutdownMessage() {
    scoped_ptr<cc::proto::CompositorMessage> message;
    message.reset(new cc::proto::CompositorMessage);
    cc::proto::CompositorMessageToImpl* to_impl =
        message->mutable_to_impl();
    to_impl->set_message_type(cc::proto::CompositorMessageToImpl::CLOSE_IMPL);
    delegate()->OnCompositorMessageReceived(std::move(message));
  }

  scoped_ptr<base::MessageLoop> loop_;
  MockRenderWidgetFeature render_widget_feature_;
  scoped_ptr<BlimpCompositorForTesting> compositor_;
  gfx::AcceleratedWidget window_;
};

TEST_F(BlimpCompositorTest, ToggleVisibilityAndWidgetWithHost) {
  // Make the compositor visible and give it a widget when we don't have a host.
  compositor_->SetVisible(true);
  compositor_->SetAcceleratedWidget(window_);
  SendInitializeMessage();

  // Check that the visibility is set correctly on the host.
  EXPECT_NE(compositor_->host(), nullptr);
  EXPECT_TRUE(compositor_->host()->visible());

  // Make the compositor invisible. This should drop the output surface and
  // make the |host_| invisible.
  compositor_->SetVisible(false);
  EXPECT_FALSE(compositor_->host()->visible());

  // Make the compositor visible and release the widget. This should make the
  // |host_| invisible.
  compositor_->SetVisible(true);
  EXPECT_TRUE(compositor_->host()->visible());
  compositor_->ReleaseAcceleratedWidget();
  EXPECT_FALSE(compositor_->host()->visible());

  SendShutdownMessage();
  EXPECT_EQ(compositor_->host(), nullptr);
}

TEST_F(BlimpCompositorTest, DestroyAndRecreateHost) {
  // Create the host and make it visible.
  SendInitializeMessage();
  compositor_->SetVisible(true);
  compositor_->SetAcceleratedWidget(window_);

  // Destroy this host and recreate a new one. Make sure that the visibility is
  // set correctly on this host.
  SendShutdownMessage();
  SendInitializeMessage();
  EXPECT_NE(compositor_->host(), nullptr);
  EXPECT_TRUE(compositor_->host()->visible());
}

TEST_F(BlimpCompositorTest, DestroyHostOnRenderWidgetInitialized) {
  // Create the host.
  compositor_->SetVisible(true);
  compositor_->SetAcceleratedWidget(window_);
  SendInitializeMessage();
  EXPECT_NE(compositor_->host(), nullptr);

  delegate()->OnRenderWidgetInitialized();
  EXPECT_EQ(compositor_->host(), nullptr);

  // Recreate the host.
  SendInitializeMessage();
  EXPECT_NE(compositor_->host(), nullptr);
  EXPECT_TRUE(compositor_->host()->visible());
}

}  // namespace client
}  // namespace blimp
