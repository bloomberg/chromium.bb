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
#include "components/html_viewer/media_factory.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "gin/v8_initializer.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/mojo/init/ui_init.h"
#include "v8/include/v8.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/cpp/font_loader.h"
#endif

namespace html_viewer {

namespace {

// Disables support for (unprefixed) Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Prevents creation of any output surface.
const char kIsHeadless[] = "is-headless";

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

}  // namespace

GlobalState::GlobalState(mojo::ApplicationImpl* app)
    : app_(app),
      resource_loader_(app->shell(), GetResourcePaths()),
      is_headless_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(kIsHeadless)),
      did_init_(false),
      device_pixel_ratio_(1.f),
      discardable_memory_allocator_(kDesiredMaxMemory),
      compositor_thread_("compositor thread") {
  if (is_headless_)
    InitIfNecessary(gfx::Size(1024, 1024), 1.f);
}

GlobalState::~GlobalState() {
  if (blink_platform_) {
    renderer_scheduler_->Shutdown();
    blink::shutdown();
  }
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
  SkFontConfigInterface::SetGlobal(new font_service::FontLoader(app_));
#endif

  ui_init_.reset(
      new ui::mojo::UIInit(screen_size_in_pixels, device_pixel_ratio));
  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  renderer_scheduler_ = scheduler::RendererScheduler::Create();
  blink_platform_.reset(new BlinkPlatformImpl(app_, renderer_scheduler_.get()));
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

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // Display process ID, thread ID and timestamp in logs.
  logging::SetLogItems(true, true, true, false);

  if (command_line->HasSwitch(kDisableEncryptedMedia))
    blink::WebRuntimeFeatures::enableEncryptedMedia(false);

  if (!is_headless_) {
    base::File pak_file = resource_loader_.ReleaseFile(kResourceResourcesPak);
    base::File pak_file_2 = pak_file.Duplicate();
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        pak_file.Pass(), base::MemoryMappedFile::Region::kWholeFile);
    // TODO(sky): why is this always using 100?
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      pak_file_2.Pass(), ui::SCALE_FACTOR_100P);
  }

  compositor_thread_.Start();

  media_factory_.reset(
      new MediaFactory(compositor_thread_.task_runner(), app_->shell()));

  if (command_line->HasSwitch(kJavaScriptFlags)) {
    std::string flags(command_line->GetSwitchValueASCII(kJavaScriptFlags));
    v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  }
}

}  // namespace html_viewer
