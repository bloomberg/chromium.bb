// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <dwrite.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_win.h"
#include "content/common/font_warmup_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/dwrite_font_platform_win.h"
#include "content/public/common/injection_test_win.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_thread_impl.h"
#include "sandbox/win/src/sandbox.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/win/direct_write.h"
#include "ui/gfx/win/dpi.h"

namespace content {
namespace {

// Windows-only skia sandbox support
// These are used for GDI-path rendering.
void SkiaPreCacheFont(const LOGFONT& logfont) {
  RenderThread* render_thread = RenderThread::Get();
  if (render_thread) {
    render_thread->PreCacheFont(logfont);
  }
}

// Helper function to cast RenderThread to IPC::Sender so we can Bind()
// it.
IPC::Sender* GetRenderThreadSender() {
  return RenderThread::Get();
}

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
        : parameters_(parameters),
          sandbox_test_module_(NULL) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  const base::CommandLine& command_line = parameters_.command_line;

  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
  bool no_sandbox = command_line.HasSwitch(switches::kNoSandbox);

  bool use_direct_write = gfx::win::ShouldUseDirectWrite();
  if (!no_sandbox) {
    // ICU DateFormat class (used in base/time_format.cc) needs to get the
    // Olson timezone ID by accessing the registry keys under
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones.
    // After TimeZone::createDefault is called once here, the timezone ID is
    // cached and there's no more need to access the registry. If the sandbox
    // is disabled, we don't have to make this dummy call.
    scoped_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

    if (use_direct_write) {
      if (ShouldUseDirectWriteFontProxyFieldTrial())
        InitializeDWriteFontProxy(base::Bind(&GetRenderThreadSender));
      else
        WarmupDirectWrite();
    } else {
      SkTypeface_SetEnsureLOGFONTAccessibleProc(SkiaPreCacheFont);
    }
  }
  blink::WebFontRendering::setUseDirectWrite(use_direct_write);
  blink::WebFontRendering::setDeviceScaleFactor(gfx::GetDPIScale());
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
  if (ShouldUseDirectWriteFontProxyFieldTrial())
    UninitializeDWriteFontProxy();
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_.sandbox_info->target_services;

  if (target_services) {
    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);

    target_services->LowerToken();
    return true;
  }
  return false;
}

}  // namespace content
