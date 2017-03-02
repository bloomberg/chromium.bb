// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/test/mash_test_suite.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "cc/output/context_provider.h"
#include "cc/surfaces/frame_sink_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace mash {
namespace test {

class TestContextFactory : public ui::ContextFactory,
                           public ui::ContextFactoryPrivate {
 public:
  TestContextFactory() : frame_sink_id_allocator_(0) {}
  ~TestContextFactory() override {}

 private:
  // ui::ContextFactory::
  void CreateCompositorFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override {}
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider()
      override {
    return nullptr;
  }
  void RemoveCompositor(ui::Compositor* compositor) override {}
  bool DoesCreateTestContexts() override { return true; }
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override {
    return GL_TEXTURE_2D;
  }
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &gpu_memory_buffer_manager_;
  }
  cc::TaskGraphRunner* GetTaskGraphRunner() override {
    return &task_graph_runner_;
  }
  void AddObserver(ui::ContextFactoryObserver* observer) override {}
  void RemoveObserver(ui::ContextFactoryObserver* observer) override {}

  // ui::ContextFactoryPrivate:
  std::unique_ptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override {
    return nullptr;
  }
  void RemoveReflector(ui::Reflector* reflector) override {}
  cc::FrameSinkId AllocateFrameSinkId() override {
    return frame_sink_id_allocator_.NextFrameSinkId();
  }
  cc::SurfaceManager* GetSurfaceManager() override { return &surface_manager_; }
  void SetDisplayVisible(ui::Compositor* compositor, bool visible) override {}
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override {}
  void SetDisplayColorSpace(ui::Compositor* compositor,
                            const gfx::ColorSpace& color_space) override {}
  void SetAuthoritativeVSyncInterval(ui::Compositor* compositor,
                                     base::TimeDelta interval) override {}
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override {}
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override {}

  cc::TestTaskGraphRunner task_graph_runner_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  cc::FrameSinkIdAllocator frame_sink_id_allocator_;
  cc::SurfaceManager surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestContextFactory);
};

MashTestSuite::MashTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

MashTestSuite::~MashTestSuite() {}

void MashTestSuite::Initialize() {
  base::TestSuite::Initialize();
  gl::GLSurfaceTestSupport::InitializeOneOff();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kOverrideUseSoftwareGLForTests);

  // Load ash mus strings and resources; not 'common' (Chrome) resources.
  base::FilePath resources;
  PathService::Get(base::DIR_MODULE, &resources);
  resources = resources.Append(FILE_PATH_LITERAL("ash_mus_resources.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(resources);

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);

  compositor_context_factory_ = base::MakeUnique<TestContextFactory>();
  env_->set_context_factory(compositor_context_factory_.get());
  env_->set_context_factory_private(compositor_context_factory_.get());
}

void MashTestSuite::Shutdown() {
  env_.reset();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace mash
