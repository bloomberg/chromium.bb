// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/setup.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "components/html_viewer/blink_platform_impl.h"
#include "components/html_viewer/web_media_player_factory.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "gin/v8_initializer.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_ANDROID)
#include "components/html_viewer/ui_setup_android.h"
#else
#include "components/html_viewer/ui_setup.h"
#endif

namespace html_viewer {

namespace {

// Enable MediaRenderer in media pipeline instead of using the internal
// media::Renderer implementation.
const char kEnableMojoMediaRenderer[] = "enable-mojo-media-renderer";

// Disables support for (unprefixed) Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Prevents creation of any output surface.
const char kIsHeadless[] = "is-headless";

size_t kDesiredMaxMemory = 20 * 1024 * 1024;

// Paths resources are loaded from.
const char kResourceIcudtl[] = "icudtl.dat";
const char kResourceResourcesPak[] = "html_viewer_resources.pak";
const char kResourceUIPak[] = "ui_test.pak";
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
const char kResourceNativesBlob[] = "natives_blob.bin";
const char kResourceSnapshotBlob[] = "snapshot_blob.bin";
#endif

std::set<std::string> GetResourcePaths() {
  std::set<std::string> paths;
  paths.insert(kResourceResourcesPak);
  paths.insert(kResourceIcudtl);
  paths.insert(kResourceUIPak);
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  paths.insert(kResourceNativesBlob);
  paths.insert(kResourceSnapshotBlob);
#endif
  return paths;
}

}  // namespace

Setup::Setup(mojo::ApplicationImpl* app)
    : app_(app),
      resource_loader_(app->shell(), GetResourcePaths()),
      is_headless_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(kIsHeadless)),
      did_init_(false),
      device_pixel_ratio_(1.f),
      discardable_memory_allocator_(kDesiredMaxMemory),
      compositor_thread_("compositor thread") {
  if (is_headless_)
    InitHeadless();
}

Setup::~Setup() {
}

void Setup::InitHeadless() {
  DCHECK(!did_init_);
  is_headless_ = true;
  InitIfNecessary(gfx::Size(1024, 1024), 1.f);
}

void Setup::InitIfNecessary(const gfx::Size& screen_size_in_pixels,
                            float device_pixel_ratio) {
  if (did_init_)
    return;

  DCHECK_NE(0.f, device_pixel_ratio);

  did_init_ = true;
  device_pixel_ratio_ = device_pixel_ratio;
  screen_size_in_pixels_ = screen_size_in_pixels;

  if (!resource_loader_.BlockUntilLoaded()) {
    // Assume on error we're being shut down.
    mojo::ApplicationImpl::Terminate();
    return;
  }

  ui_setup_.reset(new UISetup(screen_size_in_pixels, device_pixel_ratio));
  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);

  renderer_scheduler_ = scheduler::RendererScheduler::Create();
  blink_platform_.reset(new BlinkPlatformImpl(app_, renderer_scheduler_.get()));
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  CHECK(gin::V8Initializer::LoadV8SnapshotFromFD(
      resource_loader_.ReleaseFile(kResourceNativesBlob).TakePlatformFile(), 0u,
      0u,
      resource_loader_.ReleaseFile(kResourceSnapshotBlob).TakePlatformFile(),
      0u, 0u));
#endif
  blink::initialize(blink_platform_.get());
  base::i18n::InitializeICUWithFileDescriptor(
      resource_loader_.ReleaseFile(kResourceIcudtl).TakePlatformFile(),
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
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        resource_loader_.ReleaseFile(kResourceResourcesPak),
        base::MemoryMappedFile::Region::kWholeFile);
    // TODO(sky): why is this always using 100?
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
        resource_loader_.ReleaseFile(kResourceUIPak), ui::SCALE_FACTOR_100P);
  }

  compositor_thread_.Start();
#if defined(OS_ANDROID)
  // TODO(sky): Get WebMediaPlayerFactory working on android.
  NOTIMPLEMENTED();
#else
  bool enable_mojo_media_renderer =
      command_line->HasSwitch(kEnableMojoMediaRenderer);

  web_media_player_factory_.reset(new WebMediaPlayerFactory(
      compositor_thread_.message_loop_proxy(), enable_mojo_media_renderer));
#endif
}

}  // namespace html_viewer
