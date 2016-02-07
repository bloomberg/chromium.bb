// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/global_state.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/layer_settings.h"
#include "components/html_viewer/blink_platform_impl.h"
#include "components/html_viewer/blink_settings_impl.h"
#include "components/html_viewer/media_factory.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "gin/v8_initializer.h"
#include "mojo/logging/init_logging.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/shell.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/display.h"
#include "ui/mojo/init/ui_init.h"
#include "v8/include/v8.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/cpp/font_loader.h"
#endif

namespace html_viewer {

namespace {

// Disables support for (unprefixed) Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Specifies the flags passed to JS engine.
const char kJavaScriptFlags[] = "js-flags";

size_t kDesiredMaxMemory = 20 * 1024 * 1024;

// Paths resources are loaded from.
const char kResourceResourcesPak[] = "html_viewer.pak";
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
const char kResourceNativesBlob[] = "natives_blob.bin";
const char kResourceSnapshotBlob[] = "snapshot_blob.bin";
#endif

std::set<std::string> GetResourcePaths() {
  std::set<std::string> paths;
  paths.insert(kResourceResourcesPak);
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  paths.insert(kResourceNativesBlob);
  paths.insert(kResourceSnapshotBlob);
#endif
  return paths;
}

// TODO(sky): convert to using DisplayService.
std::vector<gfx::Display> DisplaysFromSizeAndScale(
    const gfx::Size& screen_size_in_pixels,
    float device_pixel_ratio) {
  std::vector<gfx::Display> displays(1);
  displays[0].set_id(2000);
  displays[0].SetScaleAndBounds(device_pixel_ratio,
                                gfx::Rect(screen_size_in_pixels));
  return displays;
}

}  // namespace

GlobalState::GlobalState(mojo::Shell* shell, const std::string& url)
    : shell_(shell),
      resource_loader_(shell, GetResourcePaths()),
      did_init_(false),
      device_pixel_ratio_(1.f),
      discardable_memory_allocator_(kDesiredMaxMemory),
      compositor_thread_("compositor thread"),
      blink_settings_(new BlinkSettingsImpl()) {
  tracing_.Initialize(shell, url);
}

GlobalState::~GlobalState() {
  if (blink_platform_) {
    renderer_scheduler_->Shutdown();
    blink::shutdown();
  }
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (font_loader_.get()) {
    SkFontConfigInterface::SetGlobal(nullptr);
    // FontLoader is ref counted. We need to explicitly shutdown the background
    // thread, otherwise the background thread may be shutdown after the app is
    // torn down, when we're in a bad state.
    font_loader_->Shutdown();
  }
#endif
}

void GlobalState::InitIfNecessary(const gfx::Size& screen_size_in_pixels,
                                  float device_pixel_ratio) {
  if (did_init_)
    return;

  DCHECK_NE(0.f, device_pixel_ratio);

  did_init_ = true;
  device_pixel_ratio_ = device_pixel_ratio;
  screen_size_in_pixels_ = screen_size_in_pixels;

  if (!resource_loader_.BlockUntilLoaded()) {
    // Assume on error we're being shut down.
    shell_->Quit();
    return;
  }

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  font_loader_ = skia::AdoptRef(new font_service::FontLoader(shell_));
  SkFontConfigInterface::SetGlobal(font_loader_.get());
#endif

  ui_init_.reset(new ui::mojo::UIInit(
      DisplaysFromSizeAndScale(screen_size_in_pixels_, device_pixel_ratio_)));
  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  shell_->ConnectToService("mojo:mus", &gpu_service_);
  gpu_service_->GetGpuInfo(base::Bind(&GlobalState::GetGpuInfoCallback,
                                      base::Unretained(this)));

  // Use new animation system (cc::AnimationHost).
  cc::LayerSettings layer_settings;
  layer_settings.use_compositor_animation_timelines = true;
  cc_blink::WebLayerImpl::SetLayerSettings(layer_settings);
  blink::WebRuntimeFeatures::enableCompositorAnimationTimelines(true);

  renderer_scheduler_ = scheduler::RendererScheduler::Create();
  blink_platform_.reset(
      new BlinkPlatformImpl(this, shell_, renderer_scheduler_.get()));
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8SnapshotFromFD(
      resource_loader_.ReleaseFile(kResourceSnapshotBlob).TakePlatformFile(),
      0u, 0u);
  gin::V8Initializer::LoadV8NativesFromFD(
      resource_loader_.ReleaseFile(kResourceNativesBlob).TakePlatformFile(), 0u,
      0u);
#endif
  blink::initialize(blink_platform_.get());
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::File pak_file = resource_loader_.ReleaseFile(kResourceResourcesPak);

  bool initialize_ui = true;
#if defined(COMPONENT_BUILD)
  if (command_line->HasSwitch("single-process"))
    initialize_ui = false;
#endif
  if (initialize_ui) {
    ui::RegisterPathProvider();
    base::File pak_file_2 = pak_file.Duplicate();
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        std::move(pak_file_2), base::MemoryMappedFile::Region::kWholeFile);
  }

  mojo::InitLogging();

  if (command_line->HasSwitch(kDisableEncryptedMedia))
    blink::WebRuntimeFeatures::enableEncryptedMedia(false);

  blink_settings_->Init();

  // TODO(sky): why is this always using 100?
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      std::move(pak_file), ui::SCALE_FACTOR_100P);

  compositor_thread_.Start();

  media_factory_.reset(
      new MediaFactory(compositor_thread_.task_runner(), shell_));

  if (command_line->HasSwitch(kJavaScriptFlags)) {
    std::string flags(command_line->GetSwitchValueASCII(kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }
}

// TODO(rjkroege): These two functions probably do not interoperate correctly
// with MUS.
const mus::mojom::GpuInfo* GlobalState::GetGpuInfo() {
  if (gpu_service_)
    CHECK(gpu_service_.WaitForIncomingResponse()) <<"Get GPU info failed!";
  return gpu_info_.get();
}

void GlobalState::GetGpuInfoCallback(mus::mojom::GpuInfoPtr gpu_info) {
  CHECK(gpu_info);
  gpu_info_ = std::move(gpu_info);
  gpu_service_.reset();
}

}  // namespace html_viewer
