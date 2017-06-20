// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/common/image_context_menu_renderer.mojom.h"
#include "chrome/common/prerender_types.h"
#include "chrome/common/thumbnail_capturer.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace gfx {
class Size;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace translate {
class TranslateHelper;
}

// This class holds the Chrome specific parts of RenderFrame, and has the same
// lifetime.
class ChromeRenderFrameObserver
    : public content::RenderFrameObserver,
      public chrome::mojom::ImageContextMenuRenderer,
      public chrome::mojom::ThumbnailCapturer {
 public:
  explicit ChromeRenderFrameObserver(content::RenderFrame* render_frame);
  ~ChromeRenderFrameObserver() override;

 private:
  enum TextCaptureType { PRELIMINARY_CAPTURE, FINAL_CAPTURE };

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidStartProvisionalLoad(blink::WebDataSource* data_source) override;
  void DidFinishLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_document_navigation) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

  // chrome::mojom::ImageContextMenuRenderer:
  void RequestReloadImageForContextNode() override;

  // chrome::mojom::ThumbnailCapturer:
  void RequestThumbnailForContextNode(
      int32_t thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      const RequestThumbnailForContextNodeCallback& callback) override;

  // Mojo handlers.
  void OnImageContextMenuRendererRequest(
      const service_manager::BindSourceInfo& source_info,
      chrome::mojom::ImageContextMenuRendererRequest request);
  void OnThumbnailCapturerRequest(
      const service_manager::BindSourceInfo& source_info,
      chrome::mojom::ThumbnailCapturerRequest request);

  // IPC handlers
  void OnGetWebApplicationInfo();
  void OnSetIsPrerendering(prerender::PrerenderMode mode);
  void OnRequestThumbnailForContextNode(
      int thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      int callback_id);
  void OnPrintNodeUnderContextMenu();
  void OnSetClientSidePhishingDetection(bool enable_phishing_detection);

  // Captures page information using the top (main) frame of a frame tree.
  // Currently, this page information is just the text content of the all
  // frames, collected and concatenated until a certain limit (kMaxIndexChars)
  // is reached.
  // TODO(dglazkov): This is incompatible with OOPIF and needs to be updated.
  void CapturePageText(TextCaptureType capture_type);

  void CapturePageTextLater(TextCaptureType capture_type,
                            base::TimeDelta delay);

  // Have the same lifetime as us.
  translate::TranslateHelper* translate_helper_;
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_;

  mojo::BindingSet<chrome::mojom::ImageContextMenuRenderer>
      image_context_menu_renderer_bindings_;

  mojo::BindingSet<chrome::mojom::ThumbnailCapturer>
      thumbnail_capturer_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderFrameObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
