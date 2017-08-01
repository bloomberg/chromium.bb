// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/readback_types.h"

class SkBitmap;

namespace gfx {
class Image;
}  // namespace gfx

namespace blink {
struct WebDeviceEmulationParams;
}

namespace content {

class DevToolsAgentHostImpl;
class RenderFrameHostImpl;
class WebContentsImpl;

namespace protocol {

class EmulationHandler;

class PageHandler : public DevToolsDomainHandler,
                    public Page::Backend,
                    public NotificationObserver {
 public:
  explicit PageHandler(EmulationHandler* handler);
  ~PageHandler() override;

  static std::vector<PageHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;
  void OnSwapCompositorFrame(cc::CompositorFrameMetadata frame_metadata);
  void OnSynchronousSwapCompositorFrame(
      cc::CompositorFrameMetadata frame_metadata);
  void DidAttachInterstitialPage();
  void DidDetachInterstitialPage();
  bool screencast_enabled() const { return enabled_ && screencast_enabled_; }

  Response Enable() override;
  Response Disable() override;

  Response Reload(Maybe<bool> bypassCache,
                  Maybe<std::string> script_to_evaluate_on_load) override;
  Response Navigate(const std::string& url,
                    Maybe<std::string> referrer,
                    Maybe<std::string> transition_type,
                    Page::FrameId* frame_id) override;
  Response StopLoading() override;

  using NavigationEntries = protocol::Array<Page::NavigationEntry>;
  Response GetNavigationHistory(
      int* current_index,
      std::unique_ptr<NavigationEntries>* entries) override;
  Response NavigateToHistoryEntry(int entry_id) override;

  void CaptureScreenshot(
      Maybe<std::string> format,
      Maybe<int> quality,
      Maybe<Page::Viewport> clip,
      Maybe<bool> from_surface,
      std::unique_ptr<CaptureScreenshotCallback> callback) override;
  void PrintToPDF(Maybe<bool> landscape,
                  Maybe<bool> display_header_footer,
                  Maybe<bool> print_background,
                  Maybe<double> scale,
                  Maybe<double> paper_width,
                  Maybe<double> paper_height,
                  Maybe<double> margin_top,
                  Maybe<double> margin_bottom,
                  Maybe<double> margin_left,
                  Maybe<double> margin_right,
                  Maybe<String> page_ranges,
                  Maybe<bool> ignore_invalid_page_ranges,
                  std::unique_ptr<PrintToPDFCallback> callback) override;
  Response StartScreencast(Maybe<std::string> format,
                           Maybe<int> quality,
                           Maybe<int> max_width,
                           Maybe<int> max_height,
                           Maybe<int> every_nth_frame) override;
  Response StopScreencast() override;
  Response ScreencastFrameAck(int session_id) override;

  Response HandleJavaScriptDialog(bool accept,
                                  Maybe<std::string> prompt_text) override;

  Response RequestAppBanner() override;

  Response BringToFront() override;

 private:
  enum EncodingFormat { PNG, JPEG };

  WebContentsImpl* GetWebContents();
  void NotifyScreencastVisibility(bool visible);
  void InnerSwapCompositorFrame();
  void ScreencastFrameCaptured(cc::CompositorFrameMetadata metadata,
                               const SkBitmap& bitmap,
                               ReadbackResponse response);
  void ScreencastFrameEncoded(cc::CompositorFrameMetadata metadata,
                              const base::Time& timestamp,
                              const std::string& data);

  void ScreenshotCaptured(
      std::unique_ptr<CaptureScreenshotCallback> callback,
      const std::string& format,
      int quality,
      const gfx::Size& original_view_size,
      const blink::WebDeviceEmulationParams& original_params,
      const gfx::Image& image);

  // NotificationObserver overrides.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  bool enabled_;

  bool screencast_enabled_;
  std::string screencast_format_;
  int screencast_quality_;
  int screencast_max_width_;
  int screencast_max_height_;
  int capture_every_nth_frame_;
  int capture_retry_count_;
  bool has_compositor_frame_metadata_;
  cc::CompositorFrameMetadata next_compositor_frame_metadata_;
  cc::CompositorFrameMetadata last_compositor_frame_metadata_;
  int session_id_;
  int frame_counter_;
  int frames_in_flight_;

  RenderFrameHostImpl* host_;
  EmulationHandler* emulation_handler_;
  std::unique_ptr<Page::Frontend> frontend_;
  NotificationRegistrar registrar_;
  base::WeakPtrFactory<PageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
