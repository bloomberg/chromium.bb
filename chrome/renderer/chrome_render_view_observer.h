// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
#pragma once

#include "base/task.h"
#include "content/renderer/render_view_observer.h"

class SkBitmap;
class TranslateHelper;
struct ThumbnailScore;
struct ViewMsg_Navigate_Params;

namespace WebKit {
class WebView;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

// This class holds the Chrome specific parts of RenderView, and has the same
// lifetime.
class ChromeRenderViewObserver : public RenderViewObserver {
 public:
  // translate_helper and/or phishing_classifier can be NULL.
  ChromeRenderViewObserver(
      RenderView* render_view,
      TranslateHelper* translate_helper,
      safe_browsing::PhishingClassifierDelegate* phishing_classifier);
  virtual ~ChromeRenderViewObserver();

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) OVERRIDE;

  void OnCaptureSnapshot();
  void OnNavigate(const ViewMsg_Navigate_Params& params);

  // Captures the thumbnail and text contents for indexing for the given load
  // ID. If the view's load ID is different than the parameter, this call is
  // a NOP. Typically called on a timer, so the load ID may have changed in the
  // meantime.
  void CapturePageInfo(int load_id, bool preliminary_capture);

  // Retrieves the text from the given frame contents, the page text up to the
  // maximum amount kMaxIndexChars will be placed into the given buffer.
  void CaptureText(WebKit::WebFrame* frame, string16* contents);

  void CaptureThumbnail();

  // Creates a thumbnail of |frame|'s contents resized to (|w|, |h|)
  // and puts that in |thumbnail|. Thumbnail metadata goes in |score|.
  bool CaptureFrameThumbnail(WebKit::WebView* view, int w, int h,
                             SkBitmap* thumbnail,
                             ThumbnailScore* score);

  // Capture a snapshot of a view.  This is used to allow an extension
  // to get a snapshot of a tab using chrome.tabs.captureVisibleTab().
  bool CaptureSnapshot(WebKit::WebView* view, SkBitmap* snapshot);

  // Has the same lifetime as us.
  TranslateHelper* translate_helper_;
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_;

  // Page_id from the last page we indexed. This prevents us from indexing the
  // same page twice in a row.
  int32 last_indexed_page_id_;

  ScopedRunnableMethodFactory<ChromeRenderViewObserver>
      page_info_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderViewObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_VIEW_OBSERVER_H_
