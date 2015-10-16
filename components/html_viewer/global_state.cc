// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/global_state.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "components/html_viewer/blink_platform_impl.h"
#include "components/html_viewer/blink_settings.h"
#include "components/html_viewer/media_factory.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "gin/v8_initializer.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/logging/init_logging.h"
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

GlobalState::GlobalState(mojo::ApplicationImpl* app)
    : app_(app),
      resource_loader_(app, GetResourcePaths()),
      did_init_(false),
      device_pixel_ratio_(1.f),
      discardable_memory_allocator_(kDesiredMaxMemory),
      compositor_thread_("compositor thread") {}

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
    app_->Quit();
    return;
  }

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  font_loader_ = skia::AdoptRef(new font_service::FontLoader(app_));
  SkFontConfigInterface::SetGlobal(font_loader_.get());
#endif

  ui_init_.reset(new ui::mojo::UIInit(
      DisplaysFromSizeAndScale(screen_size_in_pixels_, device_pixel_ratio_)));
  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:mus");
  app_->ConnectToService(request.Pass(), &gpu_service_);
  gpu_service_->GetGpuInfo(base::Bind(&GlobalState::GetGpuInfoCallback,
                                      base::Unretained(this)));

  renderer_scheduler_ = scheduler::RendererScheduler::Create();
  blink_platform_.reset(
      new BlinkPlatformImpl(this, app_, renderer_scheduler_.get()));
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8SnapshotFromFD(
      resource_loader_.ReleaseFile(kResourceSnapshotBlob).TakePlatformFile(),
      0u, 0u);
  gin::V8Initializer::LoadV8NativesFromFD(
      resource_loader_.ReleaseFile(kResourceNativesBlob).TakePlatformFile(), 0u,
      0u);
#endif
  blink::initialize(blink_platform_.get());
  base::i18n::InitializeICUWithFileDescriptor(
      resource_loader_.GetICUFile().TakePlatformFile(),
      base::MemoryMappedFile::Region::kWholeFile);

  ui::RegisterPathProvider();

  mojo::logging::InitLogging();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kDisableEncryptedMedia))
    blink::WebRuntimeFeatures::enableEncryptedMedia(false);

  blink_settings_.Init();

  base::File pak_file = resource_loader_.ReleaseFile(kResourceResourcesPak);
  base::File pak_file_2 = pak_file.Duplicate();
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      pak_file.Pass(), base::MemoryMappedFile::Region::kWholeFile);
  // TODO(sky): why is this always using 100?
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      pak_file_2.Pass(), ui::SCALE_FACTOR_100P);

  compositor_thread_.Start();

  media_factory_.reset(
      new MediaFactory(compositor_thread_.task_runner(), app_->shell()));

  if (command_line->HasSwitch(kJavaScriptFlags)) {
    std::string flags(command_line->GetSwitchValueASCII(kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }
}

// TODO(rjkroege): These two functions probably do not interoperate correctly
// with MUS.
const mojo::GpuInfo* GlobalState::GetGpuInfo() {
  if (gpu_service_)
    CHECK(gpu_service_.WaitForIncomingResponse()) <<"Get GPU info failed!";
  return gpu_info_.get();
}

void GlobalState::GetGpuInfoCallback(mojo::GpuInfoPtr gpu_info) {
  CHECK(gpu_info);
  gpu_info_ = gpu_info.Pass();
  gpu_service_.reset();
}

}  // namespace html_viewer
