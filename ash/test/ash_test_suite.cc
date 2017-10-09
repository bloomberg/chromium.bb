// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_suite.h"

#include <memory>
#include <set>

#include "ash/public/cpp/config.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_helper.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/test_context_provider.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/fake_context_factory.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ash {
namespace {

class FrameSinkClient : public viz::TestLayerTreeFrameSinkClient {
 public:
  explicit FrameSinkClient(
      scoped_refptr<viz::ContextProvider> display_context_provider)
      : display_context_provider_(std::move(display_context_provider)) {}

  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return cc::FakeOutputSurface::Create3d(
        std::move(display_context_provider_));
  }

  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {}
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const viz::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

 private:
  scoped_refptr<viz::ContextProvider> display_context_provider_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkClient);
};

class AshTestContextFactory : public ui::FakeContextFactory {
 public:
  AshTestContextFactory() = default;
  ~AshTestContextFactory() override = default;

  // ui::FakeContextFactory
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override {
    scoped_refptr<cc::TestContextProvider> context_provider =
        cc::TestContextProvider::Create();
    std::unique_ptr<FrameSinkClient> frame_sink_client =
        std::make_unique<FrameSinkClient>(context_provider);
    constexpr bool synchronous_composite = false;
    constexpr bool disable_display_vsync = false;
    const double refresh_rate = GetRefreshRate();
    auto frame_sink = std::make_unique<viz::TestLayerTreeFrameSink>(
        context_provider, cc::TestContextProvider::CreateWorker(), nullptr,
        GetGpuMemoryBufferManager(), renderer_settings(),
        base::ThreadTaskRunnerHandle::Get().get(), synchronous_composite,
        disable_display_vsync, refresh_rate);
    frame_sink->SetClient(frame_sink_client.get());
    compositor->SetLayerTreeFrameSink(std::move(frame_sink));
    frame_sink_clients_.insert(std::move(frame_sink_client));
  }

 private:
  std::set<std::unique_ptr<viz::TestLayerTreeFrameSinkClient>>
      frame_sink_clients_;

  DISALLOW_COPY_AND_ASSIGN(AshTestContextFactory);
};

}  // namespace

AshTestSuite::AshTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

AshTestSuite::~AshTestSuite() {}

void AshTestSuite::Initialize() {
  base::TestSuite::Initialize();
  // Ensure histograms hit during tests are registered properly.
  base::StatisticsRecorder::Initialize();
  gl::GLSurfaceTestSupport::InitializeOneOff();

  gfx::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Force unittests to run using en-US so if we test against string output,
  // it'll pass regardless of the system language.
  base::i18n::SetICUDefaultLocale("en_US");

  // Load ash test resources and en-US strings; not 'common' (Chrome) resources.
  base::FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  base::FilePath ash_test_strings =
      path.Append(FILE_PATH_LITERAL("ash_test_strings.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ash_test_strings);

  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_100P)) {
    base::FilePath ash_test_resources_100 =
        path.AppendASCII(AshTestEnvironment::Get100PercentResourceFileName());
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        ash_test_resources_100, ui::SCALE_FACTOR_100P);
  }
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
    base::FilePath ash_test_resources_200 =
        path.Append(FILE_PATH_LITERAL("ash_test_resources_200_percent.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        ash_test_resources_200, ui::SCALE_FACTOR_200P);
  }

  const bool is_mus = base::CommandLine::ForCurrentProcess()->HasSwitch("mus");
  const bool is_mash =
      base::CommandLine::ForCurrentProcess()->HasSwitch("mash");
  AshTestHelper::config_ =
      is_mus ? Config::MUS : is_mash ? Config::MASH : Config::CLASSIC;

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  env_ = aura::Env::CreateInstance(is_mus || is_mash ? aura::Env::Mode::MUS
                                                     : aura::Env::Mode::LOCAL);

  if (is_mus || is_mash) {
    context_factory_ = std::make_unique<AshTestContextFactory>();
    env_->set_context_factory(context_factory_.get());
    env_->set_context_factory_private(nullptr);
  }
}

void AshTestSuite::Shutdown() {
  env_.reset();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace ash
