// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <sys/sysinfo.h>

#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "chromecast/common/chromecast_switches.h"
#include "chromecast/renderer/cast_media_load_deferrer.h"
#include "chromecast/renderer/cast_render_process_observer.h"
#include "chromecast/renderer/key_systems_cast.h"
#include "chromecast/renderer/media/cma_media_renderer_factory.h"
#include "components/dns_prefetch/renderer/prescient_networking_dispatcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "crypto/nss_util.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace chromecast {
namespace shell {

namespace {

#if defined(ARCH_CPU_ARM_FAMILY) && !defined(OS_ANDROID)
// These memory thresholds are set for Chromecast. See the UMA histogram
// Platform.MeminfoMemFree when tuning.
// TODO(gunsch): These should be platform/product-dependent. Look into a way
// to move these to platform-specific repositories.
const int kCriticalMinFreeMemMB = 24;
const int kModerateMinFreeMemMB = 48;
const int kPollingIntervalMS = 5000;

void PlatformPollFreemem(void) {
  struct sysinfo sys;

  if (sysinfo(&sys) == -1) {
    LOG(ERROR) << "platform_poll_freemem(): sysinfo failed";
  } else {
    int free_mem_mb = static_cast<int64_t>(sys.freeram) *
        sys.mem_unit / (1024 * 1024);

    if (free_mem_mb <= kModerateMinFreeMemMB) {
      if (free_mem_mb <= kCriticalMinFreeMemMB) {
        // Memory is getting really low, we need to do whatever we can to
        // prevent deadlocks and interfering with other processes.
        base::MemoryPressureListener::NotifyMemoryPressure(
            base::MemoryPressureListener::MEMORY_PRESSURE_CRITICAL);
      } else {
        // There is enough memory, but it is starting to get low.
        base::MemoryPressureListener::NotifyMemoryPressure(
            base::MemoryPressureListener::MEMORY_PRESSURE_MODERATE);
      }
    }
  }

  // Setup next poll.
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PlatformPollFreemem),
      base::TimeDelta::FromMilliseconds(kPollingIntervalMS));
}
#endif

// Default background color to set for WebViews. WebColor is in ARGB format
// though the comment of WebColor says it is in RGBA.
const blink::WebColor kColorBlack = 0xFF000000;

}  // namespace

CastContentRendererClient::CastContentRendererClient() {
}

CastContentRendererClient::~CastContentRendererClient() {
}

void CastContentRendererClient::RenderThreadStarted() {
#if defined(USE_NSS)
  // Note: Copied from chrome_render_process_observer.cc to fix b/8676652.
  //
  // On platforms where the system NSS shared libraries are used,
  // initialize NSS now because it won't be able to load the .so's
  // after entering the sandbox.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kSingleProcess))
    crypto::InitNSSSafely();
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && !defined(OS_ANDROID)
  PlatformPollFreemem();
#endif

  cast_observer_.reset(new CastRenderProcessObserver());

  prescient_networking_dispatcher_.reset(
      new dns_prefetch::PrescientNetworkingDispatcher());
}

void CastContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  blink::WebView* webview = render_view->GetWebView();
  if (webview) {
    webview->setBaseBackgroundColor(kColorBlack);

    // The following settings express consistent behaviors across Cast
    // embedders, though Android has enabled by default for mobile browsers.
    webview->settings()->setShrinksViewportContentToFit(false);
    webview->settings()->setMediaControlsOverlayPlayButtonEnabled(false);
  }
}

void CastContentRendererClient::AddKeySystems(
    std::vector< ::media::KeySystemInfo>* key_systems) {
  AddChromecastKeySystems(key_systems);
  AddChromecastPlatformKeySystems(key_systems);
}

#if !defined(OS_ANDROID)
scoped_ptr<::media::RendererFactory>
CastContentRendererClient::CreateMediaRendererFactory(
    ::content::RenderFrame* render_frame) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kEnableCmaMediaPipeline))
    return nullptr;

  return scoped_ptr<::media::RendererFactory>(
      new chromecast::media::CmaMediaRendererFactory(
          render_frame->GetRoutingID()));
}
#endif

blink::WebPrescientNetworking*
CastContentRendererClient::GetPrescientNetworking() {
  return prescient_networking_dispatcher_.get();
}

void CastContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    const base::Closure& closure) {
  if (!render_frame->IsHidden()) {
    closure.Run();
    return;
  }

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new CastMediaLoadDeferrer(render_frame, closure);
}

}  // namespace shell
}  // namespace chromecast
