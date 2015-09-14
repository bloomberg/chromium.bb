// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/public/common/top_controls_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class ContentSettingsObserver;
class SkBitmap;

namespace blink {
class WebView;
struct WebWindowFeatures;
}

namespace content {
class RenderFrame;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace translate {
class TranslateHelper;
}

namespace web_cache {
class WebCacheRenderProcessObserver;
}

// Receives page information that was captured by PageInfo instance.
class PageInfoReceiver {
 public:
  enum CaptureType { PRELIMINARY_CAPTURE, FINAL_CAPTURE };
  virtual void PageCaptured(base::string16* content,
                            CaptureType capture_type) = 0;
};

// Captures page information using the top (main) frame of a frame tree.
// Currently, this page information is just the text content of the all frames,
// collected and concatenated until a certain limit (kMaxIndexChars) is reached.
class PageInfo {
 public:
  using CaptureType = PageInfoReceiver::CaptureType;

  explicit PageInfo(PageInfoReceiver* context);
  ~PageInfo() {}

  void CapturePageInfoLater(CaptureType capture_type,
                            content::RenderFrame* render_frame,
                            base::TimeDelta delay);

 private:
  // Checks if the current frame is an error page.
  bool IsErrorPage(blink::WebLocalFrame* frame);

  // Captures the thumbnail and text contents for indexing for the given load
  // ID.  Kicks off analysis of the captured text.
  void CapturePageInfo(content::RenderFrame* render_frame,
                       CaptureType capture_type);

  // Retrieves the text from the given frame contents, the page text up to the
  // maximum amount kMaxIndexChars will be placed into the given buffer.
  void CaptureText(blink::WebLocalFrame* frame, base::string16* contents);

  PageInfoReceiver* context_;

  // Used to delay calling CapturePageInfo.
  base::Timer capture_timer_;

  DISALLOW_COPY_AND_ASSIGN(PageInfo);
};

// This class holds the Chrome specific parts of RenderView, and has the same
// lifetime.
class ChromeRenderViewObserver : public content::RenderViewObserver,
                                 public PageInfoReceiver {
 public:
  // translate_helper can be NULL.
  ChromeRenderViewObserver(
      content::RenderView* render_view,
      web_cache::WebCacheRenderProcessObserver*
          web_cache_render_process_observer);
  ~ChromeRenderViewObserver() override;

 private:
  // RenderViewObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidStartLoading() override;
  void DidStartProvisionalLoad(blink::WebLocalFrame* frame) override;
  void DidFinishLoad(blink::WebLocalFrame* frame) override;
  void DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                bool is_new_navigation) override;
  void Navigate(const GURL& url) override;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  void OnWebUIJavaScript(const base::string16& javascript);
#endif
#if defined(ENABLE_EXTENSIONS)
  void OnSetVisuallyDeemphasized(bool deemphasized);
#endif
#if defined(OS_ANDROID)
  void OnUpdateTopControlsState(content::TopControlsState constraints,
                                content::TopControlsState current,
                                bool animate);
#endif
  void OnGetWebApplicationInfo();
  void OnSetClientSidePhishingDetection(bool enable_phishing_detection);
  void OnSetWindowFeatures(const blink::WebWindowFeatures& window_features);

  // Determines if a host is in the strict security host set.
  bool IsStrictSecurityHost(const std::string& host);

  // Checks if a page contains <meta http-equiv="refresh" ...> tag.
  bool HasRefreshMetaTag(blink::WebFrame* frame);

  // PageInfoReceiver implementation.
  void PageCaptured(base::string16* content, CaptureType capture_type) override;

  // Save the JavaScript to preload if a ViewMsg_WebUIJavaScript is received.
  std::vector<base::string16> webui_javascript_;

  // Owned by ChromeContentRendererClient and outlive us.
  web_cache::WebCacheRenderProcessObserver* web_cache_render_process_observer_;

  // Have the same lifetime as us.
  translate::TranslateHelper* translate_helper_;
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_;

  // true if webview is overlayed with grey color.
  bool webview_visually_deemphasized_;

  PageInfo page_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderViewObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
