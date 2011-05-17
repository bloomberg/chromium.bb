// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_VIEW_HOST_OBSERVER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_VIEW_HOST_OBSERVER_H_

#include "content/browser/renderer_host/render_view_host_observer.h"

#include <string>
#include <vector>

struct FaviconURL;
class GURL;
class RenderViewHost;

namespace IPC {
class Message;
}

namespace prerender {

class PrerenderContents;

// Observer for RenderViewHost messages.
class PrerenderRenderViewHostObserver : public RenderViewHostObserver {
 public:
  PrerenderRenderViewHostObserver(PrerenderContents* prerender_contents,
                                  RenderViewHost* render_view_host);

  virtual void RenderViewHostDestroyed() OVERRIDE;

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual bool Send(IPC::Message* message) OVERRIDE;

  void set_prerender_contents(PrerenderContents* prerender_contents) {
    prerender_contents_ = prerender_contents;
  }

 private:
  // Message handlers.
  void OnJSOutOfMemory();
  void OnRunJavaScriptMessage(const std::wstring& message,
                              const std::wstring& default_prompt,
                              const GURL& frame_url,
                              const int flags,
                              bool* did_suppress_message,
                              std::wstring* prompt_field);
  void OnRenderViewGone(int status, int exit_code);
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         bool is_main_frame,
                                         bool has_opener_set,
                                         const GURL& url);
  void OnUpdateFaviconURL(int32 page_id, const std::vector<FaviconURL>& urls);
  void OnMaybeCancelPrerenderForHTML5Media();
  void OnCancelPrerenderForPrinting();

  // The associated prerender contents.
  PrerenderContents* prerender_contents_;
};

}
#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_VIEW_HOST_OBSERVER_H_
