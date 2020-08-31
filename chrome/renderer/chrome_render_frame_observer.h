// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/prerender_types.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace gfx {
class Size;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace translate {
class TranslateAgent;
}

namespace web_cache {
class WebCacheImpl;
}

// This class holds the Chrome specific parts of RenderFrame, and has the same
// lifetime.
class ChromeRenderFrameObserver : public content::RenderFrameObserver,
                                  public chrome::mojom::ChromeRenderFrame {
 public:
  ChromeRenderFrameObserver(content::RenderFrame* render_frame,
                            web_cache::WebCacheImpl* web_cache_impl);
  ~ChromeRenderFrameObserver() override;

  service_manager::BinderRegistry* registry() { return &registry_; }
  blink::AssociatedInterfaceRegistry* associated_interfaces() {
    return &associated_interfaces_;
  }

#if defined(OS_ANDROID)
  // This is called on the main thread for subresources or worker threads for
  // dedicated workers.
  static std::string GetCCTClientHeader(int render_frame_id);
#endif

 private:
  friend class ChromeRenderFrameObserverTest;

  enum TextCaptureType { PRELIMINARY_CAPTURE, FINAL_CAPTURE };

  // RenderFrameObserver implementation.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidFinishLoad() override;
  void DidCreateNewDocument() override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidClearWindowObject() override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

  // IPC handlers
  void OnSetIsPrerendering(prerender::PrerenderMode mode,
                           const std::string& histogram_prefix);

  // chrome::mojom::ChromeRenderFrame:
  void SetWindowFeatures(
      blink::mojom::WindowFeaturesPtr window_features) override;
  void ExecuteWebUIJavaScript(const base::string16& javascript) override;
  void RequestImageForContextNode(
      int32_t thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      chrome::mojom::ImageFormat image_format,
      RequestImageForContextNodeCallback callback) override;
  void RequestReloadImageForContextNode() override;
  void SetClientSidePhishingDetection(bool enable_phishing_detection) override;
  void GetWebApplicationInfo(GetWebApplicationInfoCallback callback) override;
#if defined(OS_ANDROID)
  void SetCCTClientHeader(const std::string& header) override;
#endif
  void GetMediaFeedURL(GetMediaFeedURLCallback callback) override;

  void OnRenderFrameObserverRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>
          receiver);

  // Captures page information using the top (main) frame of a frame tree.
  // Currently, this page information is just the text content of the all
  // frames, collected and concatenated until a certain limit (kMaxIndexChars)
  // is reached.
  // TODO(dglazkov): This is incompatible with OOPIF and needs to be updated.
  void CapturePageText(TextCaptureType capture_type);

  // Check if the image need to downscale.
  static bool NeedsDownscale(const gfx::Size& original_image_size,
                             int32_t requested_image_min_area_pixels,
                             const gfx::Size& requested_image_max_size);

  // If the source image is null or occupies less area than
  // |requested_image_min_area_pixels|, we return the image unmodified.
  // Otherwise, we scale down the image so that the width and height do not
  // exceed |requested_image_max_size|, preserving the original aspect ratio.
  static SkBitmap Downscale(const SkBitmap& image,
                            int requested_image_min_area_pixels,
                            const gfx::Size& requested_image_max_size);

  // Check if the image need to encode to fit requested image format.
  static bool NeedsEncodeImage(const std::string& image_extension,
                               chrome::mojom::ImageFormat image_format);

  // Have the same lifetime as us.
  translate::TranslateAgent* translate_agent_;
#if BUILDFLAG(SAFE_BROWSING_CSD)
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_ = nullptr;
#endif

  // Owned by ChromeContentRendererClient and outlive us.
  web_cache::WebCacheImpl* web_cache_impl_;

#if !defined(OS_ANDROID)
  // Save the JavaScript to preload if ExecuteWebUIJavaScript is invoked.
  std::vector<base::string16> webui_javascript_;
#endif

  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;

  service_manager::BinderRegistry registry_;
  blink::AssociatedInterfaceRegistry associated_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderFrameObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_