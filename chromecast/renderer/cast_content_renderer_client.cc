// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <sys/sysinfo.h>

#include "base/command_line.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/common/chromecast_switches.h"
#include "chromecast/crash/cast_crash_keys.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/renderer/cast_media_load_deferrer.h"
#include "chromecast/renderer/cast_render_process_observer.h"
#include "chromecast/renderer/key_systems_cast.h"
#include "chromecast/renderer/media/chromecast_media_renderer_factory.h"
#include "components/network_hints/renderer/prescient_networking_dispatcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "crypto/nss_util.h"
#include "ipc/message_filter.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace chromecast {
namespace shell {

namespace {

#if defined(ARCH_CPU_ARM_FAMILY) && !defined(OS_ANDROID)
// This memory threshold is set for Chromecast. See the UMA histogram
// Platform.MeminfoMemFree when tuning.
// TODO(gunsch): These should be platform/product-dependent. Look into a way
// to move these to platform-specific repositories.
const int kCriticalMinFreeMemMB = 24;
const int kPollingIntervalMS = 5000;

void PlatformPollFreemem(void) {
  struct sysinfo sys;

  if (sysinfo(&sys) == -1) {
    LOG(ERROR) << "platform_poll_freemem(): sysinfo failed";
  } else {
    int free_mem_mb = static_cast<int64_t>(sys.freeram) *
        sys.mem_unit / (1024 * 1024);

    if (free_mem_mb <= kCriticalMinFreeMemMB) {
      // Memory is getting really low, we need to do whatever we can to
      // prevent deadlocks and interfering with other processes.
      base::MemoryPressureListener::NotifyMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
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

class CastRenderViewObserver : content::RenderViewObserver {
 public:
  explicit CastRenderViewObserver(content::RenderView* render_view);
  ~CastRenderViewObserver() override {}

  void DidClearWindowObject(blink::WebLocalFrame* frame) override;
};

CastRenderViewObserver::CastRenderViewObserver(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
}

void CastRenderViewObserver::DidClearWindowObject(blink::WebLocalFrame* frame) {
  PlatformAddRendererNativeBindings(frame);
}

}  // namespace

CastContentRendererClient::CastContentRendererClient() {
}

CastContentRendererClient::~CastContentRendererClient() {
}

void CastContentRendererClient::RenderThreadStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if defined(USE_NSS)
  // Note: Copied from chrome_render_process_observer.cc to fix b/8676652.
  //
  // On platforms where the system NSS shared libraries are used,
  // initialize NSS now because it won't be able to load the .so's
  // after entering the sandbox.
  if (!command_line->HasSwitch(switches::kSingleProcess))
    crypto::InitNSSSafely();
#endif

#if defined(ARCH_CPU_ARM_FAMILY) && !defined(OS_ANDROID)
  PlatformPollFreemem();
#endif

  // Set the initial known codecs mask.
  if (command_line->HasSwitch(switches::kHdmiSinkSupportedCodecs)) {
    int hdmi_codecs_mask;
    if (base::StringToInt(command_line->GetSwitchValueASCII(
                              switches::kHdmiSinkSupportedCodecs),
                          &hdmi_codecs_mask)) {
      ::media::SetHdmiSinkCodecs(hdmi_codecs_mask);
    }
  }

  cast_observer_.reset(
      new CastRenderProcessObserver(PlatformGetRendererMessageFilters()));

  prescient_networking_dispatcher_.reset(
      new network_hints::PrescientNetworkingDispatcher());

  std::string last_launched_app =
      command_line->GetSwitchValueNative(switches::kLastLaunchedApp);
  if (!last_launched_app.empty())
    base::debug::SetCrashKeyValue(crash_keys::kLastApp, last_launched_app);

  std::string previous_app =
      command_line->GetSwitchValueNative(switches::kPreviousApp);
  if (!previous_app.empty())
    base::debug::SetCrashKeyValue(crash_keys::kPreviousApp, previous_app);
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

    // Disable application cache as Chromecast doesn't support off-line
    // application running.
    webview->settings()->setOfflineWebApplicationCacheEnabled(false);
  }

  // Note: RenderView will own the lifetime of its observer.
  new CastRenderViewObserver(render_view);
}

void CastContentRendererClient::AddKeySystems(
    std::vector< ::media::KeySystemInfo>* key_systems) {
  AddChromecastKeySystems(key_systems);
  AddChromecastPlatformKeySystems(key_systems);
}

#if !defined(OS_ANDROID)
scoped_ptr<::media::RendererFactory>
CastContentRendererClient::CreateMediaRendererFactory(
    ::content::RenderFrame* render_frame,
    const scoped_refptr<::media::MediaLog>& media_log) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kEnableCmaMediaPipeline))
    return nullptr;

  return scoped_ptr<::media::RendererFactory>(
      new chromecast::media::ChromecastMediaRendererFactory(
          media_log, render_frame->GetRoutingID()));
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
