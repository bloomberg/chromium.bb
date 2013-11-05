// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/context_provider.h"
#include "cc/resources/returned_resource.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class MockLayerTreeHost : public LayerTreeHost {
 public:
  explicit MockLayerTreeHost(LayerTreeHostClient* client)
      : LayerTreeHost(client, NULL, LayerTreeSettings()) {
    Initialize(NULL);
  }

  MOCK_METHOD0(AcquireLayerTextures, void());
  MOCK_METHOD0(SetNeedsCommit, void());
  MOCK_METHOD0(SetNeedsUpdateLayers, void());
  MOCK_METHOD0(StartRateLimiter, void());
  MOCK_METHOD0(StopRateLimiter, void());
};

class TextureLayerTest : public testing::Test {
 public:
  TextureLayerTest()
      : fake_client_(
          FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)),
        host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());

    layer_tree_host_->SetRootLayer(NULL);
    layer_tree_host_.reset();
  }

  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostClient fake_client_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(TextureLayerTest, SyncImplWhenChangingTextureId) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(1);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(2);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(0);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, SyncImplWhenDrawing) {
  gfx::RectF dirty_rect(0.f, 0.f, 1.f, 1.f);

  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer.get());
  scoped_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  test_layer->SetTextureId(1);
  test_layer->SetIsDrawable(true);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(1);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times(1);
  test_layer->SetNeedsDisplayRect(dirty_rect);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  test_layer->PushPropertiesTo(impl_layer.get());  // fake commit
  test_layer->SetIsDrawable(false);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Verify that non-drawable layers don't signal the compositor,
  // except for the first draw after last commit, which must acquire
  // the texture.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(1);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  test_layer->SetNeedsDisplayRect(dirty_rect);
  test_layer->PushPropertiesTo(impl_layer.get());  // fake commit
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Second draw with layer in non-drawable state: no texture
  // acquisition.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  test_layer->SetNeedsDisplayRect(dirty_rect);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, SyncImplWhenRemovingFromTree) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  ASSERT_TRUE(root_layer.get());
  scoped_refptr<Layer> child_layer = Layer::Create();
  ASSERT_TRUE(child_layer.get());
  root_layer->AddChild(child_layer);
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer.get());
  test_layer->SetTextureId(0);
  child_layer->AddChild(test_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(root_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->RemoveFromParent();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  child_layer->AddChild(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(1);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->RemoveFromParent();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  EXPECT_SET_NEEDS_COMMIT(1, layer_tree_host_->SetRootLayer(test_layer));

  // Test properties that should call SetNeedsCommit.  All properties need to
  // be set to new values in order for SetNeedsCommit to be called.
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetFlipped(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetUV(
      gfx::PointF(0.25f, 0.25f), gfx::PointF(0.75f, 0.75f)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetVertexOpacity(
      0.5f, 0.5f, 0.5f, 0.5f));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetPremultipliedAlpha(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBlendBackgroundColor(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetTextureId(1));

  // Calling SetTextureId can call AcquireLayerTextures.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
}

TEST_F(TextureLayerTest, VisibleContentOpaqueRegion) {
  const gfx::Size layer_bounds(100, 100);
  const gfx::Rect layer_rect(layer_bounds);
  const Region layer_region(layer_rect);

  scoped_refptr<TextureLayer> layer = TextureLayer::Create(NULL);
  layer->SetBounds(layer_bounds);
  layer->draw_properties().visible_content_rect = layer_rect;
  layer->SetBlendBackgroundColor(true);

  // Verify initial conditions.
  EXPECT_FALSE(layer->contents_opaque());
  EXPECT_EQ(0u, layer->background_color());
  EXPECT_EQ(Region().ToString(),
            layer->VisibleContentOpaqueRegion().ToString());

  // Opaque background.
  layer->SetBackgroundColor(SK_ColorWHITE);
  EXPECT_EQ(layer_region.ToString(),
            layer->VisibleContentOpaqueRegion().ToString());

  // Transparent background.
  layer->SetBackgroundColor(SkColorSetARGB(100, 255, 255, 255));
  EXPECT_EQ(Region().ToString(),
            layer->VisibleContentOpaqueRegion().ToString());
}

class FakeTextureLayerClient : public TextureLayerClient {
 public:
  FakeTextureLayerClient() : context_(TestWebGraphicsContext3D::Create()) {}

  virtual unsigned PrepareTexture() OVERRIDE {
    return 0;
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_.get();
  }

  virtual bool PrepareTextureMailbox(
      TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    *mailbox = TextureMailbox();
    *release_callback = scoped_ptr<SingleReleaseCallback>();
    return true;
  }

 private:
  scoped_ptr<TestWebGraphicsContext3D> context_;
  DISALLOW_COPY_AND_ASSIGN(FakeTextureLayerClient);
};

TEST_F(TextureLayerTest, RateLimiter) {
  FakeTextureLayerClient client;
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(
      &client);
  test_layer->SetIsDrawable(true);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);

  // Don't rate limit until we invalidate.
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter()).Times(0);
  test_layer->SetRateLimitContext(true);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Do rate limit after we invalidate.
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter());
  test_layer->SetNeedsDisplay();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Stop rate limiter when we don't want it any more.
  EXPECT_CALL(*layer_tree_host_, StopRateLimiter());
  test_layer->SetRateLimitContext(false);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Or we clear the client.
  test_layer->SetRateLimitContext(true);
  EXPECT_CALL(*layer_tree_host_, StopRateLimiter());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  test_layer->ClearClient();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Reset to a layer with a client, that started the rate limiter.
  test_layer = TextureLayer::CreateForMailbox(
      &client);
  test_layer->SetIsDrawable(true);
  test_layer->SetRateLimitContext(true);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter()).Times(0);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_CALL(*layer_tree_host_, StartRateLimiter());
  test_layer->SetNeedsDisplay();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Stop rate limiter when we're removed from the tree.
  EXPECT_CALL(*layer_tree_host_, StopRateLimiter());
  layer_tree_host_->SetRootLayer(NULL);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

class MockMailboxCallback {
 public:
  MOCK_METHOD3(Release, void(const std::string& mailbox,
                             unsigned sync_point,
                             bool lost_resource));
  MOCK_METHOD3(Release2, void(base::SharedMemory* shared_memory,
                              unsigned sync_point,
                              bool lost_resource));
};

struct CommonMailboxObjects {
  CommonMailboxObjects()
      : mailbox_name1_(64, '1'),
        mailbox_name2_(64, '2'),
        sync_point1_(1),
        sync_point2_(2),
        shared_memory_(new base::SharedMemory) {
    release_mailbox1_ = base::Bind(&MockMailboxCallback::Release,
                                   base::Unretained(&mock_callback_),
                                   mailbox_name1_);
    release_mailbox2_ = base::Bind(&MockMailboxCallback::Release,
                                   base::Unretained(&mock_callback_),
                                   mailbox_name2_);
    gpu::Mailbox m1;
    m1.SetName(reinterpret_cast<const int8*>(mailbox_name1_.data()));
    mailbox1_ = TextureMailbox(m1, sync_point1_);
    gpu::Mailbox m2;
    m2.SetName(reinterpret_cast<const int8*>(mailbox_name2_.data()));
    mailbox2_ = TextureMailbox(m2, sync_point2_);

    gfx::Size size(128, 128);
    EXPECT_TRUE(shared_memory_->CreateAndMapAnonymous(4 * size.GetArea()));
    release_mailbox3_ = base::Bind(&MockMailboxCallback::Release2,
                                   base::Unretained(&mock_callback_),
                                   shared_memory_.get());
    mailbox3_ = TextureMailbox(shared_memory_.get(), size);
  }

  std::string mailbox_name1_;
  std::string mailbox_name2_;
  MockMailboxCallback mock_callback_;
  ReleaseCallback release_mailbox1_;
  ReleaseCallback release_mailbox2_;
  ReleaseCallback release_mailbox3_;
  TextureMailbox mailbox1_;
  TextureMailbox mailbox2_;
  TextureMailbox mailbox3_;
  unsigned sync_point1_;
  unsigned sync_point2_;
  scoped_ptr<base::SharedMemory> shared_memory_;
};

class TestMailboxHolder : public TextureLayer::MailboxHolder {
 public:
  using TextureLayer::MailboxHolder::Create;

 protected:
  virtual ~TestMailboxHolder() {}
};

class TextureLayerWithMailboxTest : public TextureLayerTest {
 protected:
  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
    EXPECT_CALL(test_data_.mock_callback_,
                Release(test_data_.mailbox_name1_,
                        test_data_.sync_point1_,
                        false)).Times(1);
    TextureLayerTest::TearDown();
  }

  CommonMailboxObjects test_data_;
};

TEST_F(TextureLayerWithMailboxTest, ReplaceMailboxOnMainThreadBeforeCommit) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(NULL);
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(
      test_data_.mailbox1_,
      SingleReleaseCallback::Create(test_data_.release_mailbox1_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(1);
  test_layer->SetTextureMailbox(
      test_data_.mailbox2_,
      SingleReleaseCallback::Create(test_data_.release_mailbox2_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name2_,
                      test_data_.sync_point2_,
                      false))
      .Times(1);
  test_layer->SetTextureMailbox(TextureMailbox(),
                                scoped_ptr<SingleReleaseCallback>());
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(
      test_data_.mailbox3_,
      SingleReleaseCallback::Create(test_data_.release_mailbox3_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_memory_.get(),
                       0, false))
      .Times(1);
  test_layer->SetTextureMailbox(TextureMailbox(),
                                scoped_ptr<SingleReleaseCallback>());
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(
      test_data_.mailbox1_,
      SingleReleaseCallback::Create(test_data_.release_mailbox1_));
}

class TextureLayerMailboxHolderTest : public TextureLayerTest {
 public:
  TextureLayerMailboxHolderTest()
      : main_thread_("MAIN") {
    main_thread_.Start();
  }

  void Wait(const base::Thread& thread) {
    bool manual_reset = false;
    bool initially_signaled = false;
    base::WaitableEvent event(manual_reset, initially_signaled);
    thread.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));
    event.Wait();
  }

  void CreateMainRef() {
    main_ref_ = TestMailboxHolder::Create(
        test_data_.mailbox1_,
        SingleReleaseCallback::Create(test_data_.release_mailbox1_)).Pass();
  }

  void ReleaseMainRef() {
    main_ref_.reset();
  }

  void CreateImplRef(scoped_ptr<SingleReleaseCallback>* impl_ref) {
    *impl_ref = main_ref_->holder()->GetCallbackForImplThread();
  }

  void CapturePostTasksAndWait(base::WaitableEvent* begin_capture,
                               base::WaitableEvent* wait_for_capture,
                               base::WaitableEvent* stop_capture) {
    begin_capture->Wait();
    BlockingTaskRunner::CapturePostTasks capture;
    wait_for_capture->Signal();
    stop_capture->Wait();
  }

 protected:
  scoped_ptr<TestMailboxHolder::MainThreadReference>
      main_ref_;
  base::Thread main_thread_;
  CommonMailboxObjects test_data_;
};

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_BothReleaseThenMain) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(NULL);
  ASSERT_TRUE(test_layer.get());

  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateMainRef,
                 base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  scoped_ptr<SingleReleaseCallback> compositor1;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor1));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  scoped_ptr<SingleReleaseCallback> compositor2;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor2));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The compositors both destroy their impl trees before the main thread layer
  // is destroyed.
  compositor1->Run(100, false);
  compositor2->Run(200, false);

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread ref is the last one, so the mailbox is released back to the
  // embedder, with the last sync point provided by the impl trees.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, 200, false)).Times(1);

  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                 base::Unretained(this)));
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_MainReleaseBetween) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(NULL);
  ASSERT_TRUE(test_layer.get());

  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateMainRef,
                 base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  scoped_ptr<SingleReleaseCallback> compositor1;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor1));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  scoped_ptr<SingleReleaseCallback> compositor2;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor2));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // One compositor destroys their impl tree.
  compositor1->Run(100, false);

  // Then the main thread reference is destroyed.
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                 base::Unretained(this)));

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The second impl reference is destroyed last, causing the mailbox to be
  // released back to the embedder with the last sync point from the impl tree.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, 200, true)).Times(1);

  compositor2->Run(200, true);
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_MainReleasedFirst) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(NULL);
  ASSERT_TRUE(test_layer.get());

  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateMainRef,
                 base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  scoped_ptr<SingleReleaseCallback> compositor1;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor1));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  scoped_ptr<SingleReleaseCallback> compositor2;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor2));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread reference is destroyed first.
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                 base::Unretained(this)));

  // One compositor destroys their impl tree.
  compositor2->Run(200, false);

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The second impl reference is destroyed last, causing the mailbox to be
  // released back to the embedder with the last sync point from the impl tree.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, 100, true)).Times(1);

  compositor1->Run(100, true);
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_SecondImplRefShortcut) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox(NULL);
  ASSERT_TRUE(test_layer.get());

  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateMainRef,
                 base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  scoped_ptr<SingleReleaseCallback> compositor1;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor1));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  scoped_ptr<SingleReleaseCallback> compositor2;
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CreateImplRef,
                 base::Unretained(this),
                 &compositor2));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread reference is destroyed first.
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                 base::Unretained(this)));

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, 200, true)).Times(1);

  bool manual_reset = false;
  bool initially_signaled = false;
  base::WaitableEvent begin_capture(manual_reset, initially_signaled);
  base::WaitableEvent wait_for_capture(manual_reset, initially_signaled);
  base::WaitableEvent stop_capture(manual_reset, initially_signaled);

  // Post a task to start capturing tasks on the main thread. This will block
  // the main thread until we signal the |stop_capture| event.
  main_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&TextureLayerMailboxHolderTest::CapturePostTasksAndWait,
                 base::Unretained(this),
                 &begin_capture,
                 &wait_for_capture,
                 &stop_capture));

  // Before the main thread capturing starts, one compositor destroys their
  // impl reference. Since capturing did not start, this gets post-tasked to
  // the main thread.
  compositor1->Run(100, false);

  // Start capturing on the main thread.
  begin_capture.Signal();
  wait_for_capture.Wait();

  // Meanwhile, the second compositor released its impl reference, but this task
  // gets shortcutted directly to the main thread. This means the reference is
  // released before compositor1, whose reference will be released later when
  // the post-task is serviced. But since it was destroyed _on the impl thread_
  // last, its sync point values should be used.
  compositor2->Run(200, true);

  stop_capture.Signal();
  Wait(main_thread_);

  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

class TextureLayerImplWithMailboxThreadedCallback : public LayerTreeTest {
 public:
  TextureLayerImplWithMailboxThreadedCallback()
      : callback_count_(0),
        commit_count_(0) {}

  // Make sure callback is received on main and doesn't block the impl thread.
  void ReleaseCallback(unsigned sync_point, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    TextureMailbox mailbox(std::string(64, mailbox_char));
    scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
        base::Bind(
            &TextureLayerImplWithMailboxThreadedCallback::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox, callback.Pass());
  }

  virtual void BeginTest() OVERRIDE {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    // Case #1: change mailbox before the commit. The old mailbox should be
    // released immediately.
    SetMailbox('2');
    EXPECT_EQ(1, callback_count_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Case #2: change mailbox after the commit (and draw), where the
        // layer draws. The old mailbox should be released during the next
        // commit.
        SetMailbox('3');
        EXPECT_EQ(1, callback_count_);
        break;
      case 2:
        EXPECT_EQ(2, callback_count_);
        // Case #3: change mailbox when the layer doesn't draw. The old
        // mailbox should be released during the next commit.
        layer_->SetBounds(gfx::Size());
        SetMailbox('4');
        break;
      case 3:
        EXPECT_EQ(3, callback_count_);
        // Case #4: release mailbox that was committed but never drawn. The
        // old mailbox should be released during the next commit.
        layer_->SetTextureMailbox(TextureMailbox(),
                                  scoped_ptr<SingleReleaseCallback>());
        break;
      case 4:
        if (layer_tree_host()->settings().impl_side_painting) {
          // With impl painting, the texture mailbox will still be on the impl
          // thread when the commit finishes, because the layer is not drawble
          // when it has no texture mailbox, and thus does not block the commit
          // on activation. So, we wait for activation.
          // TODO(danakj): fix this. crbug.com/277953
          layer_tree_host()->SetNeedsCommit();
          break;
        } else {
          ++commit_count_;
        }
      case 5:
        EXPECT_EQ(4, callback_count_);
        // Restore a mailbox for the next step.
        SetMailbox('5');
        break;
      case 6:
        // Case #5: remove layer from tree. Callback should *not* be called, the
        // mailbox is returned to the main thread.
        EXPECT_EQ(4, callback_count_);
        layer_->RemoveFromParent();
        break;
      case 7:
        if (layer_tree_host()->settings().impl_side_painting) {
          // With impl painting, the texture mailbox will still be on the impl
          // thread when the commit finishes, because the layer is not around to
          // block the commit on activation anymore. So, we wait for activation.
          // TODO(danakj): fix this. crbug.com/277953
          layer_tree_host()->SetNeedsCommit();
          break;
        } else {
          ++commit_count_;
        }
      case 8:
        EXPECT_EQ(4, callback_count_);
        // Resetting the mailbox will call the callback now.
        layer_->SetTextureMailbox(TextureMailbox(),
                                  scoped_ptr<SingleReleaseCallback>());
        EXPECT_EQ(5, callback_count_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  int commit_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerImplWithMailboxThreadedCallback);


class TextureLayerNoMailboxIsActivatedDuringCommit : public LayerTreeTest,
                                                     public TextureLayerClient {
 protected:
  TextureLayerNoMailboxIsActivatedDuringCommit()
      : wait_thread_("WAIT"),
        wait_event_(false, false),
        context_(TestWebGraphicsContext3D::Create()) {
    wait_thread_.Start();
  }

  virtual void BeginTest() OVERRIDE {
    activate_count_ = 0;

    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::Create(this);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);

    PostSetNeedsCommitToMainThread();
  }

  // TextureLayerClient implementation.
  virtual unsigned PrepareTexture() OVERRIDE {
    context_->makeContextCurrent();
    return context_->createTexture();
  }
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_.get();
  }
  virtual bool PrepareTextureMailbox(
      TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    return false;
  }

  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Slow down activation so the main thread DidCommit() will run if
    // not blocked.
    wait_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal,
                   base::Unretained(&wait_event_)),
        base::TimeDelta::FromMilliseconds(10));
    wait_event_.Wait();

    base::AutoLock lock(activate_lock_);
    ++activate_count_;
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // The main thread is awake now, and will run DidCommit() immediately.
    // Run DidActivate() afterwards by posting it now.
    proxy()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&TextureLayerNoMailboxIsActivatedDuringCommit::DidActivate,
                   base::Unretained(this)));
  }

  void DidActivate() {
    base::AutoLock lock(activate_lock_);
    switch (activate_count_) {
      case 1:
        // The first texture has been activated. Invalidate the layer so it
        // grabs a new texture id from the client.
        layer_->SetNeedsDisplay();
        // So this commit number should complete after the second activate.
        EXPECT_EQ(1, layer_tree_host()->source_frame_number());
        break;
      case 2:
        // The second mailbox has been activated. Remove the layer from
        // the tree to cause another commit/activation. The commit should
        // finish *after* the layer is removed from the active tree.
        layer_->RemoveFromParent();
        // So this commit number should complete after the third activate.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
        break;
      case 3:
        EndTest();
        break;
    }
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 2: {
        // The activate for the 2nd texture should have happened before now.
        base::AutoLock lock(activate_lock_);
        EXPECT_EQ(2, activate_count_);
        break;
      }
      case 3: {
        // The activate to remove the layer should have happened before now.
        base::AutoLock lock(activate_lock_);
        EXPECT_EQ(3, activate_count_);
        break;
      }
    }
  }


  virtual void AfterTest() OVERRIDE {}

  base::Thread wait_thread_;
  base::WaitableEvent wait_event_;
  base::Lock activate_lock_;
  int activate_count_;
  int activate_commit_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
  scoped_ptr<TestWebGraphicsContext3D> context_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerNoMailboxIsActivatedDuringCommit);

class TextureLayerMailboxIsActivatedDuringCommit : public LayerTreeTest {
 protected:
  TextureLayerMailboxIsActivatedDuringCommit()
      : wait_thread_("WAIT"),
        wait_event_(false, false) {
    wait_thread_.Start();
  }

  static void ReleaseCallback(unsigned sync_point, bool lost_resource) {}

  void SetMailbox(char mailbox_char) {
    TextureMailbox mailbox(std::string(64, mailbox_char));
    scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
        base::Bind(
            &TextureLayerMailboxIsActivatedDuringCommit::ReleaseCallback));
    layer_->SetTextureMailbox(mailbox, callback.Pass());
  }

  virtual void BeginTest() OVERRIDE {
    activate_count_ = 0;

    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
    SetMailbox('1');

    PostSetNeedsCommitToMainThread();
  }

  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // Slow down activation so the main thread DidCommit() will run if
    // not blocked.
    wait_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal,
                   base::Unretained(&wait_event_)),
        base::TimeDelta::FromMilliseconds(10));
    wait_event_.Wait();

    base::AutoLock lock(activate_lock_);
    ++activate_count_;
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    // The main thread is awake now, and will run DidCommit() immediately.
    // Run DidActivate() afterwards by posting it now.
    proxy()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&TextureLayerMailboxIsActivatedDuringCommit::DidActivate,
                   base::Unretained(this)));
  }

  void DidActivate() {
    base::AutoLock lock(activate_lock_);
    switch (activate_count_) {
      case 1:
        // The first mailbox has been activated. Set a new mailbox, and
        // expect the next commit to finish *after* it is activated.
        SetMailbox('2');
        // So this commit number should complete after the second activate.
        EXPECT_EQ(1, layer_tree_host()->source_frame_number());
        break;
      case 2:
        // The second mailbox has been activated. Remove the layer from
        // the tree to cause another commit/activation. The commit should
        // finish *after* the layer is removed from the active tree.
        layer_->RemoveFromParent();
        // So this commit number should complete after the third activate.
        EXPECT_EQ(2, layer_tree_host()->source_frame_number());
        break;
      case 3:
        EndTest();
        break;
    }
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 2: {
        // The activate for the 2nd mailbox should have happened before now.
        base::AutoLock lock(activate_lock_);
        EXPECT_EQ(2, activate_count_);
        break;
      }
      case 3: {
        // The activate to remove the layer should have happened before now.
        base::AutoLock lock(activate_lock_);
        EXPECT_EQ(3, activate_count_);
        break;
      }
    }
  }


  virtual void AfterTest() OVERRIDE {}

  base::Thread wait_thread_;
  base::WaitableEvent wait_event_;
  base::Lock activate_lock_;
  int activate_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerMailboxIsActivatedDuringCommit);

class TextureLayerImplWithMailboxTest : public TextureLayerTest {
 protected:
  TextureLayerImplWithMailboxTest()
      : fake_client_(
          FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)) {}

  virtual void SetUp() {
    TextureLayerTest::SetUp();
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
    EXPECT_TRUE(host_impl_.InitializeRenderer(CreateFakeOutputSurface()));
  }

  bool WillDraw(TextureLayerImpl* layer, DrawMode mode) {
    bool will_draw = layer->WillDraw(
        mode, host_impl_.active_tree()->resource_provider());
    if (will_draw)
      layer->DidDraw(host_impl_.active_tree()->resource_provider());
    return will_draw;
  }

  CommonMailboxObjects test_data_;
  FakeLayerTreeHostClient fake_client_;
};

// Test conditions for results of TextureLayerImpl::WillDraw under
// different configurations of different mailbox, texture_id, and draw_mode.
TEST_F(TextureLayerImplWithMailboxTest, TestWillDraw) {
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(AnyNumber());
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_memory_.get(), 0, false))
      .Times(AnyNumber());
  // Hardware mode.
  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(
        test_data_.mailbox1_,
        SingleReleaseCallback::Create(test_data_.release_mailbox1_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(TextureMailbox(),
                                  scoped_ptr<SingleReleaseCallback>());
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    // Software resource.
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(
        test_data_.mailbox3_,
        SingleReleaseCallback::Create(test_data_.release_mailbox3_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->SetDrawsContent(true);
    ContextProvider* context_provider =
        host_impl_.output_surface()->context_provider();
    unsigned texture =
        context_provider->Context3d()->createTexture();
    impl_layer->set_texture_id(texture);
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->SetDrawsContent(true);
    impl_layer->set_texture_id(0);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  // Software mode.
  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(
        test_data_.mailbox1_,
        SingleReleaseCallback::Create(test_data_.release_mailbox1_));
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(TextureMailbox(),
                                  scoped_ptr<SingleReleaseCallback>());
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    // Software resource.
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(
        test_data_.mailbox3_,
        SingleReleaseCallback::Create(test_data_.release_mailbox3_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->SetDrawsContent(true);
    ContextProvider* context_provider =
        host_impl_.output_surface()->context_provider();
    unsigned texture =
        context_provider->Context3d()->createTexture();
    impl_layer->set_texture_id(texture);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->SetDrawsContent(true);
    impl_layer->set_texture_id(0);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  // Resourceless software mode.
  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
    impl_layer->SetDrawsContent(true);
    impl_layer->SetTextureMailbox(
        test_data_.mailbox1_,
        SingleReleaseCallback::Create(test_data_.release_mailbox1_));
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_RESOURCELESS_SOFTWARE));
  }

  {
    scoped_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
    impl_layer->SetDrawsContent(true);
    ContextProvider* context_provider =
        host_impl_.output_surface()->context_provider();
    unsigned texture =
        context_provider->Context3d()->createTexture();
    impl_layer->set_texture_id(texture);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_RESOURCELESS_SOFTWARE));
  }
}

TEST_F(TextureLayerImplWithMailboxTest, TestImplLayerCallbacks) {
  host_impl_.CreatePendingTree();
  scoped_ptr<TextureLayerImpl> pending_layer;
  pending_layer = TextureLayerImpl::Create(host_impl_.pending_tree(), 1, true);
  ASSERT_TRUE(pending_layer);

  scoped_ptr<LayerImpl> active_layer(
      pending_layer->CreateLayerImpl(host_impl_.active_tree()));
  ASSERT_TRUE(active_layer);

  pending_layer->SetTextureMailbox(
      test_data_.mailbox1_,
      SingleReleaseCallback::Create(test_data_.release_mailbox1_));

  // Test multiple commits without an activation.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(1);
  pending_layer->SetTextureMailbox(
      test_data_.mailbox2_,
      SingleReleaseCallback::Create(test_data_.release_mailbox2_));
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test callback after activation.
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  pending_layer->SetTextureMailbox(
      test_data_.mailbox1_,
      SingleReleaseCallback::Create(test_data_.release_mailbox1_));
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name2_, _, false))
      .Times(1);
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test resetting the mailbox.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  pending_layer->SetTextureMailbox(TextureMailbox(),
                                   scoped_ptr<SingleReleaseCallback>());
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_,
                      test_data_.sync_point1_,
                      false))
      .Times(1);
  pending_layer->SetTextureMailbox(
      test_data_.mailbox1_,
      SingleReleaseCallback::Create(test_data_.release_mailbox1_));
}

TEST_F(TextureLayerImplWithMailboxTest,
       TestDestructorCallbackOnCreatedResource) {
  scoped_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  impl_layer->SetTextureMailbox(
      test_data_.mailbox1_,
      SingleReleaseCallback::Create(test_data_.release_mailbox1_));
  impl_layer->SetDrawsContent(true);
  impl_layer->DidBecomeActive();
  EXPECT_TRUE(impl_layer->WillDraw(
      DRAW_MODE_HARDWARE, host_impl_.active_tree()->resource_provider()));
  impl_layer->DidDraw(host_impl_.active_tree()->resource_provider());
  impl_layer->SetTextureMailbox(TextureMailbox(),
                                scoped_ptr<SingleReleaseCallback>());
}

TEST_F(TextureLayerImplWithMailboxTest, TestCallbackOnInUseResource) {
  ResourceProvider* provider = host_impl_.active_tree()->resource_provider();
  ResourceProvider::ResourceId id =
      provider->CreateResourceFromTextureMailbox(
          test_data_.mailbox1_,
          SingleReleaseCallback::Create(test_data_.release_mailbox1_));
  provider->AllocateForTesting(id);

  // Transfer some resources to the parent.
  ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id);
  TransferableResourceArray list;
  provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  EXPECT_TRUE(provider->InUseByConsumer(id));
  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  ReturnedResourceArray returned;
  TransferableResource::ReturnResources(list, &returned);
  provider->ReceiveReturnsFromParent(returned);
}

// Check that ClearClient correctly clears the state so that the impl side
// doesn't try to use a texture that could have been destroyed.
class TextureLayerClientTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerClientTest()
      : context_(NULL),
        texture_(0),
        commit_count_(0),
        expected_used_textures_on_draw_(0),
        expected_used_textures_on_commit_(0) {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE {
    scoped_ptr<TestWebGraphicsContext3D> context(
        TestWebGraphicsContext3D::Create());
    context_ = context.get();
    texture_ = context->createTexture();
    return FakeOutputSurface::Create3d(context.Pass()).PassAs<OutputSurface>();
  }

  virtual unsigned PrepareTexture() OVERRIDE {
    return texture_;
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return context_;
  }

  virtual bool PrepareTextureMailbox(
      TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    return false;
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    texture_layer_ = TextureLayer::Create(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetAnchorPoint(gfx::PointF());
    texture_layer_->SetIsDrawable(true);
    root->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
    {
      base::AutoLock lock(lock_);
      expected_used_textures_on_commit_ = 1;
    }
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        texture_layer_->ClearClient();
        texture_layer_->SetNeedsDisplay();
        {
          base::AutoLock lock(lock_);
          expected_used_textures_on_commit_ = 0;
        }
        texture_ = 0;
        break;
      case 2:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    base::AutoLock lock(lock_);
    expected_used_textures_on_draw_ = expected_used_textures_on_commit_;
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    context_->ResetUsedTextures();
    return true;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ASSERT_TRUE(result);
    EXPECT_EQ(expected_used_textures_on_draw_, context_->NumUsedTextures());
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<TextureLayer> texture_layer_;
  TestWebGraphicsContext3D* context_;
  unsigned texture_;
  int commit_count_;

  // Used only on thread.
  unsigned expected_used_textures_on_draw_;

  // Used on either thread, protected by lock_.
  base::Lock lock_;
  unsigned expected_used_textures_on_commit_;
};

// The TextureLayerClient does not use mailboxes, so can't use a delegating
// renderer.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TextureLayerClientTest);


// Checks that changing a texture in the client for a TextureLayer that's
// invisible correctly works without drawing a deleted texture. See
// crbug.com/266628
class TextureLayerChangeInvisibleTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerChangeInvisibleTest()
      : client_context_(TestWebGraphicsContext3D::Create()),
        texture_(client_context_->createTexture()),
        texture_to_delete_on_next_commit_(0),
        prepare_called_(0),
        commit_count_(0),
        expected_texture_on_draw_(0) {}

  // TextureLayerClient implementation.
  virtual unsigned PrepareTexture() OVERRIDE {
    ++prepare_called_;
    return texture_;
  }

  // TextureLayerClient implementation.
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return client_context_.get();
  }

  // TextureLayerClient implementation.
  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    return false;
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    solid_layer_ = SolidColorLayer::Create();
    solid_layer_->SetBounds(gfx::Size(10, 10));
    solid_layer_->SetIsDrawable(true);
    solid_layer_->SetBackgroundColor(SK_ColorWHITE);
    root->AddChild(solid_layer_);

    parent_layer_ = Layer::Create();
    parent_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->SetIsDrawable(true);
    root->AddChild(parent_layer_);

    texture_layer_ = TextureLayer::Create(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetAnchorPoint(gfx::PointF());
    texture_layer_->SetIsDrawable(true);
    parent_layer_->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // We should have updated the layer, committing the texture.
        EXPECT_EQ(1, prepare_called_);
        // Make layer invisible.
        parent_layer_->SetOpacity(0.f);
        break;
      case 2: {
        // Layer shouldn't have been updated.
        EXPECT_EQ(1, prepare_called_);
        // Change the texture.
        texture_to_delete_on_next_commit_ = texture_;
        texture_ = client_context_->createTexture();
        texture_layer_->SetNeedsDisplay();
        // Force a change to make sure we draw a frame.
        solid_layer_->SetBackgroundColor(SK_ColorGRAY);
        break;
      }
      case 3:
        EXPECT_EQ(1, prepare_called_);
        client_context_->deleteTexture(texture_to_delete_on_next_commit_);
        texture_to_delete_on_next_commit_ = 0;
        // Make layer visible again.
        parent_layer_->SetOpacity(1.f);
        break;
      case 4: {
        // Layer should have been updated.
        EXPECT_EQ(2, prepare_called_);
        texture_layer_->ClearClient();
        client_context_->deleteTexture(texture_);
        texture_ = 0;
        break;
      }
      case 5:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    ASSERT_TRUE(proxy()->IsMainThreadBlocked());
    // This is the only texture that can be drawn this frame.
    expected_texture_on_draw_ = texture_;
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    ContextForImplThread(host_impl)->ResetUsedTextures();
    return true;
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ASSERT_TRUE(result);
    TestWebGraphicsContext3D* context = ContextForImplThread(host_impl);
    int used_textures = context->NumUsedTextures();
    switch (host_impl->active_tree()->source_frame_number()) {
      case 0:
        EXPECT_EQ(1, used_textures);
        EXPECT_TRUE(context->UsedTexture(expected_texture_on_draw_));
        break;
      case 1:
      case 2:
        EXPECT_EQ(0, used_textures);
        break;
      case 3:
        EXPECT_EQ(1, used_textures);
        EXPECT_TRUE(context->UsedTexture(expected_texture_on_draw_));
        break;
      default:
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  TestWebGraphicsContext3D* ContextForImplThread(LayerTreeHostImpl* host_impl) {
    return static_cast<TestWebGraphicsContext3D*>(
        host_impl->output_surface()->context_provider()->Context3d());
  }

  scoped_refptr<SolidColorLayer> solid_layer_;
  scoped_refptr<Layer> parent_layer_;
  scoped_refptr<TextureLayer> texture_layer_;
  scoped_ptr<TestWebGraphicsContext3D> client_context_;

  // Used on the main thread, and on the impl thread while the main thread is
  // blocked.
  unsigned texture_;

  // Used on the main thread.
  unsigned texture_to_delete_on_next_commit_;
  int prepare_called_;
  int commit_count_;

  // Used on the compositor thread.
  unsigned expected_texture_on_draw_;
};

// The TextureLayerChangeInvisibleTest does not use mailboxes, so can't use a
// delegating renderer.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TextureLayerChangeInvisibleTest);

// Checks that TextureLayer::Update does not cause an extra commit when setting
// the texture mailbox.
class TextureLayerNoExtraCommitForMailboxTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerNoExtraCommitForMailboxTest()
      : prepare_mailbox_count_(0) {}

  // TextureLayerClient implementation.
  virtual unsigned PrepareTexture() OVERRIDE {
    NOTREACHED();
    return 0;
  }

  // TextureLayerClient implementation.
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    prepare_mailbox_count_++;
    // Alternate between two mailboxes to ensure that the TextureLayer updates
    // and commits.
    if (prepare_mailbox_count_ % 2 == 0)
      *mailbox = MakeMailbox('1');
    else
      *mailbox = MakeMailbox('2');

    // Non-zero mailboxes need a callback.
    *release_callback = SingleReleaseCallback::Create(
        base::Bind(&TextureLayerNoExtraCommitForMailboxTest::MailboxReleased,
                   base::Unretained(this)));
    // If the test fails, this would cause an infinite number of commits.
    return true;
  }

  TextureMailbox MakeMailbox(char name) {
    return TextureMailbox(std::string(64, name));
  }

  void MailboxReleased(unsigned sync_point, bool lost_resource) {
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    solid_layer_ = SolidColorLayer::Create();
    solid_layer_->SetBounds(gfx::Size(10, 10));
    solid_layer_->SetIsDrawable(true);
    solid_layer_->SetBackgroundColor(SK_ColorWHITE);
    root->AddChild(solid_layer_);

    parent_layer_ = Layer::Create();
    parent_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->SetIsDrawable(true);
    root->AddChild(parent_layer_);

    texture_layer_ = TextureLayer::CreateForMailbox(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetAnchorPoint(gfx::PointF());
    texture_layer_->SetIsDrawable(true);
    parent_layer_->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }


  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<SolidColorLayer> solid_layer_;
  scoped_refptr<Layer> parent_layer_;
  scoped_refptr<TextureLayer> texture_layer_;

  int prepare_mailbox_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerNoExtraCommitForMailboxTest);

// Checks that changing a mailbox in the client for a TextureLayer that's
// invisible correctly works and uses the new mailbox as soon as the layer
// becomes visible (and returns the old one).
class TextureLayerChangeInvisibleMailboxTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerChangeInvisibleMailboxTest()
      : mailbox_changed_(true),
        mailbox_returned_(0),
        prepare_called_(0),
        commit_count_(0) {
    mailbox_ = MakeMailbox('1');
  }

  // TextureLayerClient implementation.
  virtual unsigned PrepareTexture() OVERRIDE {
    NOTREACHED();
    return 0;
  }

  // TextureLayerClient implementation.
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  // TextureLayerClient implementation.
  virtual bool PrepareTextureMailbox(
      cc::TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    ++prepare_called_;
    if (!mailbox_changed_)
      return false;
    *mailbox = mailbox_;
    *release_callback = SingleReleaseCallback::Create(
        base::Bind(&TextureLayerChangeInvisibleMailboxTest::MailboxReleased,
                   base::Unretained(this)));
    return true;
  }

  TextureMailbox MakeMailbox(char name) {
    return TextureMailbox(std::string(64, name));
  }

  void MailboxReleased(unsigned sync_point, bool lost_resource) {
    ++mailbox_returned_;
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetAnchorPoint(gfx::PointF());
    root->SetIsDrawable(true);

    solid_layer_ = SolidColorLayer::Create();
    solid_layer_->SetBounds(gfx::Size(10, 10));
    solid_layer_->SetIsDrawable(true);
    solid_layer_->SetBackgroundColor(SK_ColorWHITE);
    root->AddChild(solid_layer_);

    parent_layer_ = Layer::Create();
    parent_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->SetIsDrawable(true);
    root->AddChild(parent_layer_);

    texture_layer_ = TextureLayer::CreateForMailbox(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetAnchorPoint(gfx::PointF());
    texture_layer_->SetIsDrawable(true);
    parent_layer_->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // We should have updated the layer, committing the texture.
        EXPECT_EQ(1, prepare_called_);
        // Make layer invisible.
        parent_layer_->SetOpacity(0.f);
        break;
      case 2:
        // Layer shouldn't have been updated.
        EXPECT_EQ(1, prepare_called_);
        // Change the texture.
        mailbox_ = MakeMailbox('2');
        mailbox_changed_ = true;
        texture_layer_->SetNeedsDisplay();
        // Force a change to make sure we draw a frame.
        solid_layer_->SetBackgroundColor(SK_ColorGRAY);
        break;
      case 3:
        // Layer shouldn't have been updated.
        EXPECT_EQ(1, prepare_called_);
        // So the old mailbox isn't returned yet.
        EXPECT_EQ(0, mailbox_returned_);
        // Make layer visible again.
        parent_layer_->SetOpacity(1.f);
        break;
      case 4:
        // Layer should have been updated.
        EXPECT_EQ(2, prepare_called_);
        // So the old mailbox should have been returned already.
        EXPECT_EQ(1, mailbox_returned_);
        texture_layer_->ClearClient();
        break;
      case 5:
        EXPECT_EQ(2, mailbox_returned_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE {
    ASSERT_TRUE(result);
    DelegatedFrameData* delegated_frame_data =
        output_surface()->last_sent_frame().delegated_frame_data.get();
    if (!delegated_frame_data)
      return;

    // Return all resources immediately.
    TransferableResourceArray resources_to_return =
        output_surface()->resources_held_by_parent();

    CompositorFrameAck ack;
    for (size_t i = 0; i < resources_to_return.size(); ++i)
      output_surface()->ReturnResource(resources_to_return[i].id, &ack);
    host_impl->ReclaimResources(&ack);
    host_impl->OnSwapBuffersComplete();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<SolidColorLayer> solid_layer_;
  scoped_refptr<Layer> parent_layer_;
  scoped_refptr<TextureLayer> texture_layer_;

  // Used on the main thread.
  bool mailbox_changed_;
  TextureMailbox mailbox_;
  int mailbox_returned_;
  int prepare_called_;
  int commit_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerChangeInvisibleMailboxTest);

// Test recovering from a lost context.
class TextureLayerLostContextTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  TextureLayerLostContextTest()
      : texture_(0),
        draw_count_(0) {}

  virtual scoped_ptr<OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE {
    texture_context_ = TestWebGraphicsContext3D::Create();
    texture_ = texture_context_->createTexture();
    return CreateFakeOutputSurface();
  }

  virtual unsigned PrepareTexture() OVERRIDE {
    if (draw_count_ == 0) {
      texture_context_->loseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
          GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    return texture_;
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return texture_context_.get();
  }

  virtual bool PrepareTextureMailbox(
      TextureMailbox* mailbox,
      scoped_ptr<SingleReleaseCallback>* release_callback,
      bool use_shared_memory) OVERRIDE {
    return false;
  }

  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    texture_layer_ = TextureLayer::Create(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetIsDrawable(true);
    root->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual bool PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                     LayerTreeHostImpl::FrameData* frame_data,
                                     bool result) OVERRIDE {
    LayerImpl* root = host_impl->RootLayer();
    TextureLayerImpl* texture_layer =
        static_cast<TextureLayerImpl*>(root->children()[0]);
    if (++draw_count_ == 1)
      EXPECT_EQ(0u, texture_layer->texture_id());
    else
      EXPECT_EQ(texture_, texture_layer->texture_id());
    return true;
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  scoped_refptr<TextureLayer> texture_layer_;
  scoped_ptr<TestWebGraphicsContext3D> texture_context_;
  unsigned texture_;
  int draw_count_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(TextureLayerLostContextTest);

class TextureLayerWithMailboxMainThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(unsigned sync_point, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    TextureMailbox mailbox(std::string(64, mailbox_char));
    scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
        base::Bind(
            &TextureLayerWithMailboxMainThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox, callback.Pass());
  }

  virtual void SetupTree() OVERRIDE {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
  }

  virtual void BeginTest() OVERRIDE {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    callback_count_ = 0;

    // Set the mailbox on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Delete the TextureLayer on the main thread while the mailbox is in
        // the impl tree.
        layer_->RemoveFromParent();
        layer_ = NULL;
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, callback_count_);
  }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerWithMailboxMainThreadDeleted);

class TextureLayerWithMailboxImplThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(unsigned sync_point, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    TextureMailbox mailbox(std::string(64, mailbox_char));
    scoped_ptr<SingleReleaseCallback> callback = SingleReleaseCallback::Create(
        base::Bind(
            &TextureLayerWithMailboxImplThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox, callback.Pass());
  }

  virtual void SetupTree() OVERRIDE {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(NULL);
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
  }

  virtual void BeginTest() OVERRIDE {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    callback_count_ = 0;

    // Set the mailbox on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // Remove the TextureLayer on the main thread while the mailbox is in
        // the impl tree, but don't delete the TextureLayer until after the impl
        // tree side is deleted.
        layer_->RemoveFromParent();
        break;
      case 2:
        layer_ = NULL;
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {
    EXPECT_EQ(1, callback_count_);
  }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    TextureLayerWithMailboxImplThreadDeleted);

}  // namespace
}  // namespace cc
