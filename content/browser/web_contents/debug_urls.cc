// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/debug_urls.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "webkit/plugins/plugin_constants.h"

namespace content {

namespace {

void HandlePpapiFlashDebugURL(const GURL& url) {
#if defined(ENABLE_PLUGINS)
  bool crash = url == GURL(kChromeUIPpapiFlashCrashURL);

  std::vector<PpapiPluginProcessHost*> hosts;
  PpapiPluginProcessHost::FindByName(UTF8ToUTF16(kFlashPluginName), &hosts);
  for (std::vector<PpapiPluginProcessHost*>::iterator iter = hosts.begin();
       iter != hosts.end(); ++iter) {
    if (crash)
      (*iter)->Send(new PpapiMsg_Crash());
    else
      (*iter)->Send(new PpapiMsg_Hang());
  }
#endif
}

}  // namespace

bool HandleDebugURL(const GURL& url, PageTransition transition) {
  // Ensure that the user explicitly navigated to this URL.
  if (!(transition & PAGE_TRANSITION_FROM_ADDRESS_BAR))
    return false;

  if (url.host() == chrome::kChromeUIBrowserCrashHost) {
    // Induce an intentional crash in the browser process.
    CHECK(false);
    return true;
  }

  if (url == GURL(kChromeUIGpuCleanURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateRemoveAllContext();
    return true;
  }

  if (url == GURL(kChromeUIGpuCrashURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateCrash();
    return true;
  }

  if (url == GURL(kChromeUIGpuHangURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateHang();
    return true;
  }

  if (url == GURL(kChromeUIPpapiFlashCrashURL) ||
      url == GURL(kChromeUIPpapiFlashHangURL)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&HandlePpapiFlashDebugURL, url));
    return true;
  }

  return false;
}

}  // namespace content
