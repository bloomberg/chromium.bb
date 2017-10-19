// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/image_context_menu_renderer.mojom.h"
#include "chrome/common/prerender_types.h"
#include "chrome/common/thumbnail_capturer.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(SAFE_BROWSING_CSD)
#include "chrome/common/safe_browsing/phishing_detector.mojom.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/common/web_ui_tester.mojom.h"
#endif

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
#if defined(SAFE_BROWSING_CSD)
      public chrome::mojom::PhishingDetector,
#endif
#if !defined(OS_ANDROID)
      public chrome::mojom::WebUITester,
#endif
      public chrome::mojom::ThumbnailCapturer,
      public chrome::mojom::ChromeRenderFrame {
 public:
  explicit ChromeRenderFrameObserver(content::RenderFrame* render_frame);
  ~ChromeRenderFrameObserver() override;

  service_manager::BinderRegistry* registry() { return &registry_; }

 private:
  enum TextCaptureType { PRELIMINARY_CAPTURE, FINAL_CAPTURE };

  // RenderFrameObserver implementation.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidStartProvisionalLoad(blink::WebDocumentLoader* loader) override;
  void DidFinishLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_document_navigation) override;
  void DidClearWindowObject() override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

  // chrome::mojom::ImageContextMenuRenderer:
  void RequestReloadImageForContextNode() override;

#if defined(SAFE_BROWSING_CSD)
  // chrome::mojom::PhishingDetector:
  void SetClientSidePhishingDetection(bool enable_phishing_detection) override;
#endif

#if !defined(OS_ANDROID)
  // chrome::mojom::WebUITester:
  void ExecuteWebUIJavaScript(const base::string16& javascript) override;
#endif

  // chrome::mojom::ThumbnailCapturer:
  void RequestThumbnailForContextNode(
      int32_t thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      chrome::mojom::ImageFormat image_format,
      const RequestThumbnailForContextNodeCallback& callback) override;

  // Mojo handlers.
  void OnImageContextMenuRendererRequest(
      chrome::mojom::ImageContextMenuRendererRequest request);
#if defined(SAFE_BROWSING_CSD)
  void OnPhishingDetectorRequest(
      chrome::mojom::PhishingDetectorRequest request);
#endif
#if !defined(OS_ANDROID)
  void OnWebUITesterRequest(
      chrome::mojom::WebUITesterAssociatedRequest request);
#endif
  void OnThumbnailCapturerRequest(
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

  // chrome::mojom::ChromeRenderFrame:
  void SetWindowFeatures(
      blink::mojom::WindowFeaturesPtr window_features) override;
  void OnRenderFrameObserverRequest(
      chrome::mojom::ChromeRenderFrameAssociatedRequest request);

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

#if defined(SAFE_BROWSING_CSD)
  mojo::BindingSet<chrome::mojom::PhishingDetector> phishing_detector_bindings_;
#endif

#if !defined(OS_ANDROID)
  // Save the JavaScript to preload if ExecuteWebUIJavaScript is invoked.
  std::vector<base::string16> webui_javascript_;
  mojo::AssociatedBindingSet<chrome::mojom::WebUITester>
      web_ui_tester_bindings_;
#endif

  mojo::BindingSet<chrome::mojom::ThumbnailCapturer>
      thumbnail_capturer_bindings_;

  mojo::AssociatedBindingSet<chrome::mojom::ChromeRenderFrame>
      window_features_client_bindings_;

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderFrameObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
