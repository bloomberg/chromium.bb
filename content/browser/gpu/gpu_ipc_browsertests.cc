// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_switches.h"

namespace {

using content::WebGraphicsContext3DCommandBufferImpl;

const content::CauseForGpuLaunch kInitCause =
    content::
        CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE;

class ContextTestBase : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    if (!content::BrowserGpuChannelHostFactory::CanUseForTesting())
      return;

    if (!content::BrowserGpuChannelHostFactory::instance())
      content::BrowserGpuChannelHostFactory::Initialize(true);

    content::BrowserGpuChannelHostFactory* factory =
        content::BrowserGpuChannelHostFactory::instance();
    CHECK(factory);
    bool lose_context_when_out_of_memory = false;
    base::RunLoop run_loop;
    factory->EstablishGpuChannel(kInitCause, run_loop.QuitClosure());
    run_loop.Run();
    scoped_refptr<content::GpuChannelHost>
        gpu_channel_host(factory->GetGpuChannel());
    DCHECK(gpu_channel_host.get());
    context_.reset(
        WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            gpu_channel_host.get(),
            blink::WebGraphicsContext3D::Attributes(),
            lose_context_when_out_of_memory,
            GURL(),
            WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
            NULL));
    CHECK(context_.get());
    context_->InitializeOnCurrentThread();
    context_support_ = context_->GetContextSupport();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Must delete the context first.
    context_.reset(NULL);
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  scoped_ptr<content::WebGraphicsContext3DCommandBufferImpl> context_;
  gpu::ContextSupport* context_support_;
};

}  // namespace

// Include the shared tests.
#define CONTEXT_TEST_F IN_PROC_BROWSER_TEST_F
#include "content/common/gpu/client/gpu_context_tests.h"

namespace content {

class BrowserGpuChannelHostFactoryTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    if (!BrowserGpuChannelHostFactory::CanUseForTesting())
      return;

    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    if (!BrowserGpuChannelHostFactory::instance())
      BrowserGpuChannelHostFactory::Initialize(false);

    CHECK(GetFactory());

    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    command_line->AppendSwitch(switches::kDisableGpuEarlyInit);
  }

  void OnContextLost(const base::Closure callback, int* counter) {
    (*counter)++;
    callback.Run();
  }

 protected:
  BrowserGpuChannelHostFactory* GetFactory() {
    return BrowserGpuChannelHostFactory::instance();
  }

  bool IsChannelEstablished() {
    return GetFactory()->GetGpuChannel() != NULL;
  }

  void EstablishAndWait() {
    base::RunLoop run_loop;
    GetFactory()->EstablishGpuChannel(kInitCause, run_loop.QuitClosure());
    run_loop.Run();
  }

  GpuChannelHost* GetGpuChannel() {
    return GetFactory()->GetGpuChannel();
  }

  static void Signal(bool *event) {
    CHECK_EQ(*event, false);
    *event = true;
  }

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContext() {
    bool lose_context_when_out_of_memory = false;
    return make_scoped_ptr(
        WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            GetGpuChannel(),
            blink::WebGraphicsContext3D::Attributes(),
            lose_context_when_out_of_memory,
            GURL(),
            WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
            NULL));
  }
};

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, MAYBE_Basic) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(GetGpuChannel() != NULL);
}

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_EstablishAndTerminate EstablishAndTerminate
#else
#define MAYBE_EstablishAndTerminate DISABLED_EstablishAndTerminate
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_EstablishAndTerminate) {
  DCHECK(!IsChannelEstablished());
  base::RunLoop run_loop;
  GetFactory()->EstablishGpuChannel(kInitCause, run_loop.QuitClosure());
  GetFactory()->Terminate();

  // The callback should still trigger.
  run_loop.Run();
}

#if !defined(OS_ANDROID)
// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_AlreadyEstablished AlreadyEstablished
#else
#define MAYBE_AlreadyEstablished DISABLED_AlreadyEstablished
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_AlreadyEstablished) {
  DCHECK(!IsChannelEstablished());
  scoped_refptr<GpuChannelHost> gpu_channel =
      GetFactory()->EstablishGpuChannelSync(kInitCause);

  // Expect established callback immediately.
  bool event = false;
  GetFactory()->EstablishGpuChannel(
      kInitCause,
      base::Bind(&BrowserGpuChannelHostFactoryTest::Signal, &event));
  EXPECT_TRUE(event);
  EXPECT_EQ(gpu_channel.get(), GetGpuChannel());
}
#endif

// Test fails on Windows because GPU Channel set-up fails.
// Mac only fails GPU set-up on MacOS 10.6, but we have no version-specific
// macros for disabling tests.
#if !defined(OS_WIN) && !defined(OS_MACOSX)
#define MAYBE_GrContextKeepsGpuChannelAlive GrContextKeepsGpuChannelAlive
#else
#define MAYBE_GrContextKeepsGpuChannelAlive \
    DISABLED_GrContextKeepsGpuChannelAlive
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_GrContextKeepsGpuChannelAlive) {
  // Test for crbug.com/551143
  // This test verifies that holding a reference to the GrContext created by
  // a ContextProviderCommandBuffer will keep the gpu channel alive after the
  // provider has been destroyed. Without this behavior, user code would have
  // to be careful to destroy objects in the right order to avoid using freed
  // memory as a function pointer in the GrContext's GrGLInterface instance.
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();

  // Step 2: verify that holding onto the provider's GrContext will
  // retain the host after provider is destroyed.
  scoped_refptr<ContextProviderCommandBuffer> provider =
      ContextProviderCommandBuffer::Create(CreateContext(),
                                           OFFSCREEN_CONTEXT_FOR_TESTING);
  EXPECT_TRUE(provider->BindToCurrentThread());

  skia::RefPtr<GrContext> gr_context = skia::SharePtr(provider->GrContext());
  provider = nullptr;

  SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
  skia::RefPtr<SkSurface> surface = skia::AdoptRef(SkSurface::NewRenderTarget(
      gr_context.get(), SkSurface::kNo_Budgeted, info));
  gr_context = nullptr;

  // use the canvas after the provider and grcontext have been locally
  // unref'ed. This should work just fine thanks to SkSurface_Gpu ref'ing
  // the GrContext, which is ref'ing the GrGLInterfaceForWebGraphicsContext3D,
  // which owns the commandbuffer instance.
  SkPaint greenFillPaint;
  greenFillPaint.setColor(SK_ColorGREEN);
  greenFillPaint.setStyle(SkPaint::kFill_Style);
  // Passes by not crashing
  surface->getCanvas()->drawRect(SkRect::MakeWH(100, 100), greenFillPaint);
}

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_CrashAndRecover
#else
#define MAYBE_CrashAndRecover DISABLED_CrashAndRecover
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_CrashAndRecover) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  scoped_refptr<GpuChannelHost> host = GetGpuChannel();

  scoped_refptr<ContextProviderCommandBuffer> provider =
      ContextProviderCommandBuffer::Create(CreateContext(),
                                           OFFSCREEN_CONTEXT_FOR_TESTING);
  base::RunLoop run_loop;
  int counter = 0;
  provider->SetLostContextCallback(
      base::Bind(&BrowserGpuChannelHostFactoryTest::OnContextLost,
                 base::Unretained(this), run_loop.QuitClosure(), &counter));
  EXPECT_TRUE(provider->BindToCurrentThread());
  GpuProcessHostUIShim* shim =
      GpuProcessHostUIShim::FromID(GetFactory()->GpuProcessHostId());
  EXPECT_TRUE(shim != NULL);
  shim->SimulateCrash();
  run_loop.Run();

  EXPECT_EQ(1, counter);
  EXPECT_FALSE(IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(IsChannelEstablished());
}

}  // namespace content
