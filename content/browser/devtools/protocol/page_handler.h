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
#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"

class SkBitmap;

namespace content {

class RenderViewHostImpl;

namespace devtools {
namespace page {

class ColorPicker;

class PageHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  PageHandler();
  virtual ~PageHandler();

  void SetRenderViewHost(RenderViewHostImpl* host);
  void SetClient(scoped_ptr<Client> client);
  void Detached();
  void OnSwapCompositorFrame(const cc::CompositorFrameMetadata& frame_metadata);
  void OnVisibilityChanged(bool visible);
  void DidAttachInterstitialPage();
  void DidDetachInterstitialPage();

  Response Enable();
  Response Disable();

  Response Reload(const bool* ignoreCache,
                  const std::string* script_to_evaluate_on_load,
                  const std::string* script_preprocessor);

  Response Navigate(const std::string& url, FrameId* frame_id);

  Response GetNavigationHistory(int* current_index,
                                std::vector<NavigationEntry>* entries);

  Response NavigateToHistoryEntry(int entry_id);

  Response SetGeolocationOverride(double* latitude,
                                  double* longitude,
                                  double* accuracy);

  Response ClearGeolocationOverride();

  Response SetTouchEmulationEnabled(bool enabled);
  Response SetTouchEmulationEnabled(bool enabled,
                                    const std::string* configuration);

  scoped_refptr<DevToolsProtocol::Response> CaptureScreenshot(
      scoped_refptr<DevToolsProtocol::Command> command);

  Response CanScreencast(bool* result);
  Response CanEmulate(bool* result);

  Response StartScreencast(const std::string* format,
                           const int* quality,
                           const int* max_width,
                           const int* max_height);

  Response StopScreencast();
  Response HandleJavaScriptDialog(bool accept, const std::string* prompt_text);

  scoped_refptr<DevToolsProtocol::Response> QueryUsageAndQuota(
      const std::string& security_origin,
      scoped_refptr<DevToolsProtocol::Command> command);

  Response SetColorPickerEnabled(bool enabled);

 private:
  void UpdateTouchEventEmulationState();

  void NotifyScreencastVisibility(bool visible);
  void InnerSwapCompositorFrame();
  void ScreencastFrameCaptured(
      const std::string& format,
      int quality,
      const cc::CompositorFrameMetadata& metadata,
      bool success,
      const SkBitmap& bitmap);

  void ScreenshotCaptured(
      scoped_refptr<DevToolsProtocol::Command> command,
      const unsigned char* png_data,
      size_t png_size);

  void OnColorPicked(int r, int g, int b, int a);

  void QueryUsageAndQuotaCompleted(
      scoped_refptr<DevToolsProtocol::Command> command,
      scoped_ptr<QueryUsageAndQuotaResponse> response);

  bool enabled_;
  bool touch_emulation_enabled_;
  std::string touch_emulation_configuration_;

  bool screencast_enabled_;
  std::string screencast_format_;
  int screencast_quality_;
  int screencast_max_width_;
  int screencast_max_height_;
  int capture_retry_count_;
  bool has_last_compositor_frame_metadata_;
  cc::CompositorFrameMetadata last_compositor_frame_metadata_;
  base::TimeTicks last_frame_time_;

  scoped_ptr<ColorPicker> color_picker_;

  RenderViewHostImpl* host_;
  scoped_ptr<Client> client_;
  base::WeakPtrFactory<PageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
