// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;

namespace cc {
namespace {

gpu::Mailbox MailboxFromChar(char value) {
  gpu::Mailbox mailbox;
  memset(mailbox.name, value, sizeof(mailbox.name));
  return mailbox;
}

gpu::SyncToken SyncTokenFromUInt(uint32_t value) {
  return gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                        gpu::CommandBufferId::FromUnsafeValue(0x123), value);
}

class MockLayerTreeHost : public LayerTreeHost {
 public:
  static std::unique_ptr<MockLayerTreeHost> Create(
      FakeLayerTreeHostClient* client,
      TaskGraphRunner* task_graph_runner,
      MutatorHost* mutator_host) {
    LayerTreeHost::InitParams params;
    params.client = client;
    params.task_graph_runner = task_graph_runner;
    params.mutator_host = mutator_host;
    LayerTreeSettings settings;
    params.settings = &settings;
    return base::WrapUnique(new MockLayerTreeHost(&params));
  }

  MOCK_METHOD0(SetNeedsCommit, void());
  MOCK_METHOD0(StartRateLimiter, void());
  MOCK_METHOD0(StopRateLimiter, void());

 private:
  explicit MockLayerTreeHost(LayerTreeHost::InitParams* params)
      : LayerTreeHost(params, CompositorMode::SINGLE_THREADED) {
    InitializeSingleThreaded(&single_thread_client_,
                             base::ThreadTaskRunnerHandle::Get());
  }

  StubLayerTreeHostSingleThreadClient single_thread_client_;
};

class MockReleaseCallback {
 public:
  MOCK_METHOD3(Release,
               void(const gpu::Mailbox& mailbox,
                    const gpu::SyncToken& sync_token,
                    bool lost_resource));
  MOCK_METHOD3(Release2,
               void(viz::SharedBitmap* shared_bitmap,
                    const gpu::SyncToken& sync_token,
                    bool lost_resource));
};

struct CommonResourceObjects {
  explicit CommonResourceObjects(viz::SharedBitmapManager* manager)
      : mailbox_name1_(MailboxFromChar('1')),
        mailbox_name2_(MailboxFromChar('2')),
        sync_token1_(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234),
                     1),
        sync_token2_(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234),
                     2) {
    release_callback1_ =
        base::Bind(&MockReleaseCallback::Release,
                   base::Unretained(&mock_callback_), mailbox_name1_);
    release_callback2_ =
        base::Bind(&MockReleaseCallback::Release,
                   base::Unretained(&mock_callback_), mailbox_name2_);
    const uint32_t arbitrary_target1 = GL_TEXTURE_2D;
    const uint32_t arbitrary_target2 = GL_TEXTURE_EXTERNAL_OES;
    resource1_ = viz::TransferableResource::MakeGL(
        mailbox_name1_, GL_LINEAR, arbitrary_target1, sync_token1_);
    resource2_ = viz::TransferableResource::MakeGL(
        mailbox_name2_, GL_LINEAR, arbitrary_target2, sync_token2_);
    gfx::Size size(128, 128);
    shared_bitmap_ = manager->AllocateSharedBitmap(size);
    DCHECK(shared_bitmap_);
    release_callback3_ =
        base::Bind(&MockReleaseCallback::Release2,
                   base::Unretained(&mock_callback_), shared_bitmap_.get());
    resource3_ = viz::TransferableResource::MakeSoftware(
        shared_bitmap_->id(), shared_bitmap_->sequence_number(), size);
  }

  using RepeatingReleaseCallback =
      base::RepeatingCallback<void(const gpu::SyncToken& sync_token,
                                   bool is_lost)>;

  gpu::Mailbox mailbox_name1_;
  gpu::Mailbox mailbox_name2_;
  MockReleaseCallback mock_callback_;
  RepeatingReleaseCallback release_callback1_;
  RepeatingReleaseCallback release_callback2_;
  RepeatingReleaseCallback release_callback3_;
  gpu::SyncToken sync_token1_;
  gpu::SyncToken sync_token2_;
  std::unique_ptr<viz::SharedBitmap> shared_bitmap_;
  viz::TransferableResource resource1_;
  viz::TransferableResource resource2_;
  viz::TransferableResource resource3_;
};

class TextureLayerTest : public testing::Test {
 public:
  TextureLayerTest()
      : layer_tree_frame_sink_(FakeLayerTreeFrameSink::Create3d()),
        host_impl_(&task_runner_provider_, &task_graph_runner_),
        test_data_(&shared_bitmap_manager_) {}

 protected:
  void SetUp() override {
    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = MockLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
    layer_tree_host_->SetViewportSize(gfx::Size(10, 10));
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());

    animation_host_->SetMutatorHostClient(nullptr);
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
    animation_host_ = nullptr;
  }

  std::unique_ptr<MockLayerTreeHost> layer_tree_host_;
  std::unique_ptr<AnimationHost> animation_host_;
  FakeImplTaskRunnerProvider task_runner_provider_;
  FakeLayerTreeHostClient fake_client_;
  TestSharedBitmapManager shared_bitmap_manager_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
  FakeLayerTreeHostImpl host_impl_;
  CommonResourceObjects test_data_;
};

TEST_F(TextureLayerTest, CheckPropertyChangeCausesCorrectBehavior) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  EXPECT_SET_NEEDS_COMMIT(1, layer_tree_host_->SetRootLayer(test_layer));

  // Test properties that should call SetNeedsCommit.  All properties need to
  // be set to new values in order for SetNeedsCommit to be called.
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetFlipped(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetNearestNeighbor(true));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetUV(
      gfx::PointF(0.25f, 0.25f), gfx::PointF(0.75f, 0.75f)));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetVertexOpacity(
      0.5f, 0.5f, 0.5f, 0.5f));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetPremultipliedAlpha(false));
  EXPECT_SET_NEEDS_COMMIT(1, test_layer->SetBlendBackgroundColor(true));
}

class TestMailboxHolder : public TextureLayer::TransferableResourceHolder {
 public:
  using TextureLayer::TransferableResourceHolder::Create;

 protected:
  ~TestMailboxHolder() override = default;
};

class TextureLayerWithResourceTest : public TextureLayerTest {
 protected:
  void TearDown() override {
    Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
    EXPECT_CALL(
        test_data_.mock_callback_,
        Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
        .Times(1);
    TextureLayerTest::TearDown();
  }
};

TEST_F(TextureLayerWithResourceTest, ReplaceMailboxOnMainThreadBeforeCommit) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name1_, test_data_.sync_token1_, false))
      .Times(1);
  test_layer->SetTransferableResource(
      test_data_.resource2_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback2_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release(test_data_.mailbox_name2_, test_data_.sync_token2_, false))
      .Times(1);
  test_layer->ClearTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTransferableResource(
      test_data_.resource3_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback3_));
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release2(test_data_.shared_bitmap_.get(), _, false))
      .Times(1);
  test_layer->ClearTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
}

class TextureLayerMailboxHolderTest : public TextureLayerTest {
 public:
  TextureLayerMailboxHolderTest()
      : main_thread_("MAIN") {
    main_thread_.Start();
  }

  void Wait(const base::Thread& thread) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&event)));
    event.Wait();
  }

  void CreateMainRef() {
    main_ref_ = TestMailboxHolder::Create(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  }

  void ReleaseMainRef() { main_ref_ = nullptr; }

  void CreateImplRef(
      std::unique_ptr<viz::SingleReleaseCallback>* impl_ref,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
    *impl_ref = main_ref_->holder()->GetCallbackForImplThread(
        std::move(main_thread_task_runner));
  }

 protected:
  std::unique_ptr<TestMailboxHolder::MainThreadReference> main_ref_;
  base::Thread main_thread_;
};

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_BothReleaseThenMain) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor1;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor1,
                                main_thread_.task_runner()));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor2;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor2,
                                main_thread_.task_runner()));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The compositors both destroy their impl trees before the main thread layer
  // is destroyed.
  compositor1->Run(SyncTokenFromUInt(100), false);
  compositor2->Run(SyncTokenFromUInt(200), false);

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread ref is the last one, so the resource is released back to
  // the embedder, with the last sync point provided by the impl trees.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, SyncTokenFromUInt(200), false))
      .Times(1);

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                                base::Unretained(this)));
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_MainReleaseBetween) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor1;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor1,
                                main_thread_.task_runner()));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor2;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor2,
                                main_thread_.task_runner()));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // One compositor destroys their impl tree.
  compositor1->Run(SyncTokenFromUInt(100), false);

  // Then the main thread reference is destroyed.
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The second impl reference is destroyed last, causing the resource to be
  // released back to the embedder with the last sync point from the impl tree.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, SyncTokenFromUInt(200), true))
      .Times(1);

  compositor2->Run(SyncTokenFromUInt(200), true);
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

TEST_F(TextureLayerMailboxHolderTest, TwoCompositors_MainReleasedFirst) {
  scoped_refptr<TextureLayer> test_layer =
      TextureLayer::CreateForMailbox(nullptr);
  ASSERT_TRUE(test_layer.get());

  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateMainRef,
                                base::Unretained(this)));

  Wait(main_thread_);

  // The texture layer is attached to compositor1, and passes a reference to its
  // impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor1;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor1,
                                main_thread_.task_runner()));

  // Then the texture layer is removed and attached to compositor2, and passes a
  // reference to its impl tree.
  std::unique_ptr<viz::SingleReleaseCallback> compositor2;
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::CreateImplRef,
                                base::Unretained(this), &compositor2,
                                main_thread_.task_runner()));

  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The main thread reference is destroyed first.
  main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TextureLayerMailboxHolderTest::ReleaseMainRef,
                                base::Unretained(this)));

  // One compositor destroys their impl tree.
  compositor2->Run(SyncTokenFromUInt(200), false);

  Wait(main_thread_);

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // The second impl reference is destroyed last, causing the resource to be
  // released back to the embedder with the last sync point from the impl tree.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, SyncTokenFromUInt(100), true))
      .Times(1);

  compositor1->Run(SyncTokenFromUInt(100), true);
  Wait(main_thread_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
}

class TextureLayerImplWithMailboxThreadedCallback : public LayerTreeTest {
 public:
  std::unique_ptr<viz::TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    constexpr bool disable_display_vsync = false;
    bool synchronous_composite =
        !HasImplThread() &&
        !layer_tree_host()->GetSettings().single_thread_proxy_scheduler;
    return std::make_unique<viz::TestLayerTreeFrameSink>(
        compositor_context_provider, std::move(worker_context_provider),
        shared_bitmap_manager(), gpu_memory_buffer_manager(), renderer_settings,
        ImplThreadTaskRunner(), synchronous_composite, disable_display_vsync,
        refresh_rate);
  }

  void AdvanceTestCase() {
    ++test_case_;
    switch (test_case_) {
      case 1:
        // Case #1: change resource before the commit. The old resource should
        // be released immediately.
        SetMailbox('2');
        EXPECT_EQ(1, callback_count_);
        PostSetNeedsCommitToMainThread();

        // Case 2 does not rely on callbacks to advance.
        pending_callback_ = false;
        break;
      case 2:
        // Case #2: change resource after the commit (and draw), where the
        // layer draws. The old resource should be released during the next
        // commit.
        SetMailbox('3');
        EXPECT_EQ(1, callback_count_);

        // Cases 3-5 rely on a callback to advance.
        pending_callback_ = true;
        break;
      case 3:
        EXPECT_EQ(2, callback_count_);
        // Case #3: change resource when the layer doesn't draw. The old
        // resource should be released during the next commit.
        layer_->SetBounds(gfx::Size());
        SetMailbox('4');
        break;
      case 4:
        EXPECT_EQ(3, callback_count_);
        // Case #4: release resource that was committed but never drawn. The
        // old resource should be released during the next commit.
        layer_->ClearTexture();
        break;
      case 5:
        EXPECT_EQ(4, callback_count_);
        // Restore a resource for the next step.
        SetMailbox('5');

        // Cases 6 and 7 do not rely on callbacks to advance.
        pending_callback_ = false;
        break;
      case 6:
        // Case #5: remove layer from tree. Callback should *not* be called, the
        // resource is returned to the main thread.
        EXPECT_EQ(4, callback_count_);
        layer_->RemoveFromParent();
        break;
      case 7:
        EXPECT_EQ(4, callback_count_);
        // Resetting the resource will call the callback now, before another
        // commit is needed, as the ReleaseCallback is already in flight from
        // RemoveFromParent().
        layer_->ClearTexture();
        pending_callback_ = true;
        frame_number_ = layer_tree_host()->SourceFrameNumber();
        break;
      case 8:
        // A commit wasn't needed, the ReleaseCallback was already in flight.
        EXPECT_EQ(frame_number_, layer_tree_host()->SourceFrameNumber());
        EXPECT_EQ(5, callback_count_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // Make sure callback is received on main and doesn't block the impl thread.
  void ReleaseCallback(char mailbox_char,
                       const gpu::SyncToken& sync_token,
                       bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;

    // If we are waiting on a callback, advance now.
    if (pending_callback_)
      AdvanceTestCase();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::Bind(
            &TextureLayerImplWithMailboxThreadedCallback::ReleaseCallback,
            base::Unretained(this), mailbox_char));

    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char)));
    layer_->SetTransferableResource(resource, std::move(callback));
    // Damage the layer so we send a new frame with the new resource to the
    // Display compositor.
    layer_->SetNeedsDisplay();
  }

  void BeginTest() override {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    // Setup is complete - advance to test case 1.
    AdvanceTestCase();
  }

  void DidCommit() override {
    // If we are not waiting on a callback, advance now.
    if (!pending_callback_)
      AdvanceTestCase();
  }

  void AfterTest() override {}

 private:
  base::ThreadChecker main_thread_;
  int callback_count_ = 0;
  int test_case_ = 0;
  int frame_number_ = 0;
  // Whether we are waiting on a callback to advance the test case.
  bool pending_callback_ = false;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerImplWithMailboxThreadedCallback);

class TextureLayerMailboxIsActivatedDuringCommit : public LayerTreeTest {
 protected:
  void ReleaseCallback(const gpu::SyncToken& original_sync_token,
                       const gpu::SyncToken& release_sync_token,
                       bool lost_resource) {
    released_count_++;
    switch (released_count_) {
      case 1:
        break;
      case 2:
        EXPECT_EQ(3, layer_tree_host()->SourceFrameNumber());
        EndTest();
        break;
      default:
        NOTREACHED();
    }
  }

  void SetMailbox(char mailbox_char) {
    const gpu::SyncToken sync_token =
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char));
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::Bind(
            &TextureLayerMailboxIsActivatedDuringCommit::ReleaseCallback,
            base::Unretained(this), sync_token));
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D, sync_token);
    layer_->SetTransferableResource(resource, std::move(callback));
  }

  void BeginTest() override {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
    SetMailbox('1');

    PostSetNeedsCommitToMainThread();
  }

  void WillActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    base::AutoLock lock(activate_count_lock_);
    ++activate_count_;
  }

  void DidCommit() override {
    // The first frame doesn't cause anything to be returned so it does not
    // need to wait for activation.
    if (layer_tree_host()->SourceFrameNumber() > 1) {
      base::AutoLock lock(activate_count_lock_);
      // The activate happened before commit is done on the main side.
      EXPECT_EQ(activate_count_, layer_tree_host()->SourceFrameNumber());
    }

    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // The first mailbox has been activated. Set a new mailbox, and
        // expect the next commit to finish *after* it is activated.
        SetMailbox('2');
        break;
      case 2:
        // The second mailbox has been activated. Remove the layer from
        // the tree to cause another commit/activation. The commit should
        // finish *after* the layer is removed from the active tree.
        layer_->RemoveFromParent();
        break;
      case 3:
        // This ensures all texture mailboxes are released before the end of the
        // test.
        layer_->ClearClient();
        break;
      default:
        NOTREACHED();
    }
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    // The activate didn't happen before commit is done on the impl side (but it
    // should happen before the main thread is done).
    EXPECT_EQ(activate_count_, host_impl->sync_tree()->source_frame_number());
  }

  void AfterTest() override {}

  base::Lock activate_count_lock_;
  int activate_count_ = 0;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
  int released_count_ = 0;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerMailboxIsActivatedDuringCommit);

class TextureLayerImplWithResourceTest : public TextureLayerTest {
 protected:
  void SetUp() override {
    TextureLayerTest::SetUp();
    layer_tree_host_ = MockLayerTreeHost::Create(
        &fake_client_, &task_graph_runner_, animation_host_.get());
    host_impl_.SetVisible(true);
    EXPECT_TRUE(host_impl_.InitializeRenderer(layer_tree_frame_sink_.get()));
  }

  bool WillDraw(TextureLayerImpl* layer, DrawMode mode) {
    bool will_draw = layer->WillDraw(
        mode, host_impl_.active_tree()->resource_provider());
    if (will_draw)
      layer->DidDraw(host_impl_.active_tree()->resource_provider());
    return will_draw;
  }

  FakeLayerTreeHostClient fake_client_;
};

// Test conditions for results of TextureLayerImpl::WillDraw under
// different configurations of different mailbox, texture_id, and draw_mode.
TEST_F(TextureLayerImplWithResourceTest, TestWillDraw) {
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, gpu::SyncToken(), false))
      .Times(AnyNumber());
  EXPECT_CALL(
      test_data_.mock_callback_,
      Release2(test_data_.shared_bitmap_.get(), gpu::SyncToken(), false))
      .Times(AnyNumber());
  // Hardware mode.
  {
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  {
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_HARDWARE));
  }

  // Software mode.
  {
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  {
    // Software resource.
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(
        test_data_.resource3_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback3_));
    EXPECT_TRUE(WillDraw(impl_layer.get(), DRAW_MODE_SOFTWARE));
  }

  // Resourceless software mode.
  {
    std::unique_ptr<TextureLayerImpl> impl_layer =
        TextureLayerImpl::Create(host_impl_.active_tree(), 1);
    impl_layer->SetTransferableResource(
        test_data_.resource1_,
        viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
    EXPECT_FALSE(WillDraw(impl_layer.get(), DRAW_MODE_RESOURCELESS_SOFTWARE));
  }
}

TEST_F(TextureLayerImplWithResourceTest, TestImplLayerCallbacks) {
  host_impl_.CreatePendingTree();
  std::unique_ptr<TextureLayerImpl> pending_layer;
  pending_layer = TextureLayerImpl::Create(host_impl_.pending_tree(), 1);
  ASSERT_TRUE(pending_layer);

  std::unique_ptr<LayerImpl> active_layer(
      pending_layer->CreateLayerImpl(host_impl_.active_tree()));
  ASSERT_TRUE(active_layer);

  pending_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));

  // Test multiple commits without an activation. The resource wasn't used so no
  // sync token is returned.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, gpu::SyncToken(), false))
      .Times(1);
  pending_layer->SetTransferableResource(
      test_data_.resource2_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback2_));
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test callback after activation.
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _, _)).Times(0);
  pending_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
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
  pending_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor. The resource wasn't used so no sync token is returned.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, gpu::SyncToken(), false))
      .Times(1);
  pending_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
}

TEST_F(TextureLayerImplWithResourceTest,
       TestDestructorCallbackOnCreatedResource) {
  std::unique_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, _, false))
      .Times(1);
  impl_layer->SetTransferableResource(
      test_data_.resource1_,
      viz::SingleReleaseCallback::Create(test_data_.release_callback1_));
  impl_layer->DidBecomeActive();
  EXPECT_TRUE(impl_layer->WillDraw(
      DRAW_MODE_HARDWARE, host_impl_.active_tree()->resource_provider()));
  impl_layer->DidDraw(host_impl_.active_tree()->resource_provider());
  impl_layer->SetTransferableResource(viz::TransferableResource(), nullptr);
}

// Checks that TextureLayer::Update does not cause an extra commit when setting
// the texture mailbox.
class TextureLayerNoExtraCommitForMailboxTest
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      // Once this has been committed, the resource will be released.
      *resource = viz::TransferableResource();
      return true;
    }

    *resource = viz::TransferableResource::MakeGL(MailboxFromChar('1'),
                                                  GL_LINEAR, GL_TEXTURE_2D,
                                                  SyncTokenFromUInt(0x123));
    *release_callback = viz::SingleReleaseCallback::Create(
        base::Bind(&TextureLayerNoExtraCommitForMailboxTest::ResourceReleased,
                   base::Unretained(this)));
    return true;
  }

  void ResourceReleased(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_TRUE(sync_token.HasData());
    EndTest();
  }

  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
    root->SetIsDrawable(true);

    texture_layer_ = TextureLayer::CreateForMailbox(this);
    texture_layer_->SetBounds(gfx::Size(10, 10));
    texture_layer_->SetIsDrawable(true);
    root->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        EXPECT_FALSE(proxy()->MainFrameWillHappenForTesting());
        // Invalidate the texture layer to clear the mailbox before
        // ending the test.
        texture_layer_->SetNeedsDisplay();
        break;
      case 2:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<TextureLayer> texture_layer_;
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
      : resource_changed_(true),
        resource_(MakeResource('1')),
        resource_returned_(0),
        prepare_called_(0),
        commit_count_(0) {}

  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    ++prepare_called_;
    if (!resource_changed_)
      return false;
    *resource = resource_;
    *release_callback = viz::SingleReleaseCallback::Create(
        base::Bind(&TextureLayerChangeInvisibleMailboxTest::ResourceReleased,
                   base::Unretained(this)));
    return true;
  }

  viz::TransferableResource MakeResource(char name) {
    return viz::TransferableResource::MakeGL(
        MailboxFromChar(name), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(name)));
  }

  void ResourceReleased(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_TRUE(sync_token.HasData());
    ++resource_returned_;
  }

  void SetupTree() override {
    scoped_refptr<Layer> root = Layer::Create();
    root->SetBounds(gfx::Size(10, 10));
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
    texture_layer_->SetIsDrawable(true);
    parent_layer_->AddChild(texture_layer_);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeTest::SetupTree();
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidReceiveCompositorFrameAck() override {
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
        resource_ = MakeResource('2');
        resource_changed_ = true;
        texture_layer_->SetNeedsDisplay();
        // Force a change to make sure we draw a frame.
        solid_layer_->SetBackgroundColor(SK_ColorGRAY);
        break;
      case 3:
        // Layer shouldn't have been updated.
        EXPECT_EQ(1, prepare_called_);
        // So the old resource isn't returned yet.
        EXPECT_EQ(0, resource_returned_);
        // Make layer visible again.
        parent_layer_->SetOpacity(0.9f);
        break;
      case 4:
        // Layer should have been updated.
        EXPECT_EQ(2, prepare_called_);
        // So the old resource should have been returned already.
        EXPECT_EQ(1, resource_returned_);
        texture_layer_->ClearClient();
        break;
      case 5:
        EXPECT_EQ(2, resource_returned_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  void AfterTest() override {}

 private:
  scoped_refptr<SolidColorLayer> solid_layer_;
  scoped_refptr<Layer> parent_layer_;
  scoped_refptr<TextureLayer> texture_layer_;

  // Used on the main thread.
  bool resource_changed_;
  viz::TransferableResource resource_;
  int resource_returned_;
  int prepare_called_;
  int commit_count_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerChangeInvisibleMailboxTest);

// Test that TextureLayerImpl::ReleaseResources can be called which releases
// the resource back to TextureLayerClient.
class TextureLayerReleaseResourcesBase
    : public LayerTreeTest,
      public TextureLayerClient {
 public:
  // TextureLayerClient implementation.
  bool PrepareTransferableResource(
      viz::TransferableResource* resource,
      std::unique_ptr<viz::SingleReleaseCallback>* release_callback) override {
    *resource = viz::TransferableResource::MakeGL(
        MailboxFromChar('1'), GL_LINEAR, GL_TEXTURE_2D, SyncTokenFromUInt(1));
    *release_callback = viz::SingleReleaseCallback::Create(
        base::Bind(&TextureLayerReleaseResourcesBase::ResourceReleased,
                   base::Unretained(this)));
    return true;
  }

  void ResourceReleased(const gpu::SyncToken& sync_token, bool lost_resource) {
    resource_released_ = true;
  }

  void SetupTree() override {
    LayerTreeTest::SetupTree();

    scoped_refptr<TextureLayer> texture_layer =
        TextureLayer::CreateForMailbox(this);
    texture_layer->SetBounds(gfx::Size(10, 10));
    texture_layer->SetIsDrawable(true);

    layer_tree_host()->root_layer()->AddChild(texture_layer);
    texture_layer_id_ = texture_layer->id();
  }

  void BeginTest() override {
    resource_released_ = false;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override { EndTest(); }

  void AfterTest() override { EXPECT_TRUE(resource_released_); }

 protected:
  int texture_layer_id_;

 private:
  bool resource_released_;
};

class TextureLayerReleaseResourcesAfterCommit
    : public TextureLayerReleaseResourcesBase {
 public:
  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    LayerTreeImpl* tree = nullptr;
    tree = host_impl->sync_tree();
    tree->LayerById(texture_layer_id_)->ReleaseResources();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerReleaseResourcesAfterCommit);

class TextureLayerReleaseResourcesAfterActivate
    : public TextureLayerReleaseResourcesBase {
 public:
  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    host_impl->active_tree()->LayerById(texture_layer_id_)->ReleaseResources();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerReleaseResourcesAfterActivate);

class TextureLayerWithResourceMainThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::Bind(
            &TextureLayerWithResourceMainThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char)));
    layer_->SetTransferableResource(resource, std::move(callback));
  }

  void SetupTree() override {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
  }

  void BeginTest() override {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    callback_count_ = 0;

    // Set the resource on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Delete the TextureLayer on the main thread while the resource is in
        // the impl tree.
        layer_->RemoveFromParent();
        layer_ = nullptr;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, callback_count_); }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerWithResourceMainThreadDeleted);

class TextureLayerWithResourceImplThreadDeleted : public LayerTreeTest {
 public:
  void ReleaseCallback(const gpu::SyncToken& sync_token, bool lost_resource) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    EXPECT_FALSE(lost_resource);
    ++callback_count_;
    EndTest();
  }

  void SetMailbox(char mailbox_char) {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(base::Bind(
            &TextureLayerWithResourceImplThreadDeleted::ReleaseCallback,
            base::Unretained(this)));
    auto resource = viz::TransferableResource::MakeGL(
        MailboxFromChar(mailbox_char), GL_LINEAR, GL_TEXTURE_2D,
        SyncTokenFromUInt(static_cast<uint32_t>(mailbox_char)));
    layer_->SetTransferableResource(resource, std::move(callback));
  }

  void SetupTree() override {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox(nullptr);
    layer_->SetIsDrawable(true);
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds);
  }

  void BeginTest() override {
    EXPECT_EQ(true, main_thread_.CalledOnValidThread());

    callback_count_ = 0;

    // Set the resource on the main thread.
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // Remove the TextureLayer on the main thread while the resource is in
        // the impl tree, but don't delete the TextureLayer until after the impl
        // tree side is deleted.
        layer_->RemoveFromParent();
        break;
      case 2:
        layer_ = nullptr;
        break;
    }
  }

  void AfterTest() override { EXPECT_EQ(1, callback_count_); }

 private:
  base::ThreadChecker main_thread_;
  int callback_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerWithResourceImplThreadDeleted);

}  // namespace
}  // namespace cc
