// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/delegated_output_surface.h"

#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_context_support.h"
#include "cc/test/test_gles2_interface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

class FakeBlimpOutputSurfaceClient : public BlimpOutputSurfaceClient {
 public:
  FakeBlimpOutputSurfaceClient(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : compositor_task_runner_(compositor_task_runner),
        output_surface_(nullptr),
        swap_count_(0),
        weak_factory_(this) {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  ~FakeBlimpOutputSurfaceClient() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  base::WeakPtr<FakeBlimpOutputSurfaceClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void BindToOutputSurface(
      base::WeakPtr<BlimpOutputSurface> output_surface) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    EXPECT_EQ(nullptr, output_surface_);
    output_surface_ = output_surface;
    bound_ = true;
  }

  void SwapCompositorFrame(cc::CompositorFrame frame) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    swap_count_++;
  }

  void UnbindOutputSurface() override {
    DCHECK(bound_);
    bound_ = true;
    DCHECK(thread_checker_.CalledOnValidThread());
    output_surface_ = nullptr;
  }

  int swap_count() const { return swap_count_; }
  bool bound() const { return bound_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  bool bound_ = false;
  base::WeakPtr<BlimpOutputSurface> output_surface_;
  int swap_count_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<FakeBlimpOutputSurfaceClient> weak_factory_;
};

class TestContextProvider : public cc::TestContextProvider {
 public:
  static scoped_refptr<TestContextProvider> Create(bool bind_should_fail) {
    return new TestContextProvider(bind_should_fail);
  }

 protected:
  explicit TestContextProvider(bool bind_should_fail)
      : cc::TestContextProvider(base::MakeUnique<cc::TestContextSupport>(),
                                base::MakeUnique<cc::TestGLES2Interface>(),
                                cc::TestWebGraphicsContext3D::Create()),
        bind_should_fail_(bind_should_fail) {}

  ~TestContextProvider() override {}

  bool BindToCurrentThread() override {
    if (bind_should_fail_)
      return false;
    return cc::TestContextProvider::BindToCurrentThread();
  }

 private:
  const bool bind_should_fail_;
};

class DelegatedOutputSurfaceTest : public testing::Test {
 public:
  DelegatedOutputSurfaceTest() {}

  void SetUpTest(bool bind_should_fail) {
    main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    compositor_thread_ = base::MakeUnique<base::Thread>("Compositor");
    ASSERT_TRUE(compositor_thread_->Start());
    compositor_task_runner_ = compositor_thread_->task_runner();
    blimp_output_surface_client_ =
        base::MakeUnique<FakeBlimpOutputSurfaceClient>(compositor_task_runner_);
    output_surface_ = base::MakeUnique<DelegatedOutputSurface>(
        TestContextProvider::Create(bind_should_fail), nullptr,
        main_task_runner_, blimp_output_surface_client_->GetWeakPtr());

    base::WaitableEvent init_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DelegatedOutputSurfaceTest::InitOnCompositorThread,
                   base::Unretained(this), &init_event));
    init_event.Wait();

    // Run all tasks so the registration of the BlimpOutputSurface on the main
    // thread completes.
    base::MessageLoop::current()->RunUntilIdle();
  }

  void DoSwapBuffers() {
    base::WaitableEvent swap_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DelegatedOutputSurfaceTest::DoSwapBuffersOnCompositorThread,
                   base::Unretained(this), &swap_event));
    swap_event.Wait();
  }

  void TearDown() override {
    EXPECT_EQ(blimp_output_surface_client_->swap_count(),
              output_surface_client_.swap_count());
  }

  void EndTest() {
    base::WaitableEvent shutdown_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DelegatedOutputSurfaceTest::ShutdownOnCompositorThread,
                   base::Unretained(this), &shutdown_event));
    shutdown_event.Wait();
    compositor_thread_->Stop();

    // Run all tasks so the unregistration of the BlimpOutputSurface on the main
    // thread completes.
    base::MessageLoop::current()->RunUntilIdle();
  }

  void InitOnCompositorThread(base::WaitableEvent* event) {
    bound_ = output_surface_->BindToClient(&output_surface_client_);
    event->Signal();
  }

  void DoSwapBuffersOnCompositorThread(base::WaitableEvent* event) {
    output_surface_->SwapBuffers(cc::CompositorFrame());
    event->Signal();
  }

  void ShutdownOnCompositorThread(base::WaitableEvent* event) {
    base::MessageLoop::current()->RunUntilIdle();
    if (bound_) {
      output_surface_->DetachFromClient();
      bound_ = false;
    }
    output_surface_.reset();
    event->Signal();
  }

  base::MessageLoop loop_;
  std::unique_ptr<base::Thread> compositor_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  std::unique_ptr<DelegatedOutputSurface> output_surface_;
  std::unique_ptr<FakeBlimpOutputSurfaceClient> blimp_output_surface_client_;

  bool bound_ = false;
  cc::FakeOutputSurfaceClient output_surface_client_;
};

TEST_F(DelegatedOutputSurfaceTest, BindFails) {
  SetUpTest(true);
  EXPECT_FALSE(blimp_output_surface_client_->bound());
  EndTest();
}

TEST_F(DelegatedOutputSurfaceTest, BindSucceedsSwapBuffers) {
  SetUpTest(false);
  EXPECT_TRUE(blimp_output_surface_client_->bound());

  DoSwapBuffers();
  DoSwapBuffers();
  DoSwapBuffers();

  // Run all tasks so the swap buffer calls to the BlimpOutputSurface on the
  // main thread complete.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(3, blimp_output_surface_client_->swap_count());

  EndTest();
}

}  // namespace
}  // namespace client
}  // namespace blimp
