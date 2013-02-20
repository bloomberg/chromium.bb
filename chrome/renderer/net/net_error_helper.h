// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include "chrome/common/net/net_error_info.h"
#include "chrome/common/net/net_error_tracker.h"
#include "content/public/renderer/render_view_observer.h"

namespace WebKit {
class WebFrame;
}

class NetErrorHelper : public content::RenderViewObserver {
 public:
  explicit NetErrorHelper(content::RenderView* render_view);
  virtual ~NetErrorHelper();

  // RenderViewObserver implementation.
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      WebKit::WebFrame* frame,
      const WebKit::WebURLError& error) OVERRIDE;
  virtual void DidCommitProvisionalLoad(
      WebKit::WebFrame* frame,
      bool is_new_navigation) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnNetErrorInfo(int dns_probe_result);
  void TrackerCallback(NetErrorTracker::DnsErrorPageState state);
  void UpdateErrorPage(chrome_common_net::DnsProbeResult dns_probe_result);

  NetErrorTracker tracker_;
  NetErrorTracker::DnsErrorPageState dns_error_page_state_;
  bool updated_error_page_;
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
