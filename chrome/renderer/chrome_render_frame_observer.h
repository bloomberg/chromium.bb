// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include "base/basictypes.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_frame_observer.h"

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace gfx {
class Size;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace translate {
class TranslateHelper;
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
// TODO(dglazkov): This is incompatible with OOPIF and needs to be updated.
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

// This class holds the Chrome specific parts of RenderFrame, and has the same
// lifetime.
class ChromeRenderFrameObserver : public content::RenderFrameObserver,
                                  public PageInfoReceiver {
 public:
  explicit ChromeRenderFrameObserver(content::RenderFrame* render_frame);
  ~ChromeRenderFrameObserver() override;

 private:
  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidFinishDocumentLoad() override;
  void DidStartProvisionalLoad() override;
  void DidFinishLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;

  // IPC handlers
  void OnSetIsPrerendering(bool is_prerendering);
  void OnRequestReloadImageForContextNode();
  void OnRequestThumbnailForContextNode(
      int thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      int callback_id);
  void OnPrintNodeUnderContextMenu();
  void OnSetClientSidePhishingDetection(bool enable_phishing_detection);
  void OnAppBannerPromptRequest(int request_id,
                                const std::string& platform);

  // PageInfoReceiver implementation.
  void PageCaptured(base::string16* content, CaptureType capture_type) override;

  // Have the same lifetime as us.
  translate::TranslateHelper* translate_helper_;
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_;

  PageInfo page_info_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderFrameObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
