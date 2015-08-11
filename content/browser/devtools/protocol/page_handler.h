// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/readback_types.h"

class SkBitmap;

namespace content {

class RenderFrameHostImpl;
class WebContentsImpl;

namespace devtools {
namespace page {

class ColorPicker;

class PageHandler : public NotificationObserver {
 public:
  typedef DevToolsProtocolClient::Response Response;

  class ScreencastListener {
   public:
    virtual ~ScreencastListener() { }
    virtual void ScreencastEnabledChanged() = 0;
  };

  PageHandler();
  ~PageHandler() override;

  void SetRenderFrameHost(RenderFrameHostImpl* host);
  void SetClient(scoped_ptr<Client> client);
  void Detached();
  void OnSwapCompositorFrame(const cc::CompositorFrameMetadata& frame_metadata);
  void OnSynchronousSwapCompositorFrame(const cc::CompositorFrameMetadata&
      frame_metadata);
  void DidAttachInterstitialPage();
  void DidDetachInterstitialPage();
  void SetScreencastListener(ScreencastListener* listener);
  bool screencast_enabled() const { return enabled_ && screencast_enabled_; }

  Response Enable();
  Response Disable();

  Response Reload(const bool* ignoreCache,
                  const std::string* script_to_evaluate_on_load,
                  const std::string* script_preprocessor = NULL);

  Response Navigate(const std::string& url, FrameId* frame_id);

  using NavigationEntries = std::vector<scoped_refptr<NavigationEntry>>;
  Response GetNavigationHistory(int* current_index,
                                NavigationEntries* entries);

  Response NavigateToHistoryEntry(int entry_id);

  Response CaptureScreenshot(DevToolsCommandId command_id);

  Response CanScreencast(bool* result);
  Response StartScreencast(const std::string* format,
                           const int* quality,
                           const int* max_width,
                           const int* max_height);
  Response StopScreencast();
  Response ScreencastFrameAck(int frame_number);

  Response HandleJavaScriptDialog(bool accept, const std::string* prompt_text);

  Response QueryUsageAndQuota(DevToolsCommandId command_id,
                              const std::string& security_origin);

  Response SetColorPickerEnabled(bool enabled);

 private:
  WebContentsImpl* GetWebContents();
  void NotifyScreencastVisibility(bool visible);
  void InnerSwapCompositorFrame();
  void ScreencastFrameCaptured(const cc::CompositorFrameMetadata& metadata,
                               const SkBitmap& bitmap,
                               ReadbackResponse response);
  void ScreencastFrameEncoded(const cc::CompositorFrameMetadata& metadata,
                              const base::Time& timestamp,
                              const std::string& data);

  void ScreenshotCaptured(
      DevToolsCommandId command_id,
      const unsigned char* png_data,
      size_t png_size);

  void OnColorPicked(int r, int g, int b, int a);

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
  int capture_retry_count_;
  bool has_compositor_frame_metadata_;
  cc::CompositorFrameMetadata next_compositor_frame_metadata_;
  cc::CompositorFrameMetadata last_compositor_frame_metadata_;
  int screencast_frame_sent_;
  int screencast_frame_acked_;
  bool processing_screencast_frame_;

  scoped_ptr<ColorPicker> color_picker_;

  RenderFrameHostImpl* host_;
  scoped_ptr<Client> client_;
  ScreencastListener* screencast_listener_;
  NotificationRegistrar registrar_;
  base::WeakPtrFactory<PageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
