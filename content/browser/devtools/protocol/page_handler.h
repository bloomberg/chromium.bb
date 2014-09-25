// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"

namespace content {

class RenderViewHostImpl;

namespace devtools {
namespace page {

class PageHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  PageHandler();
  virtual ~PageHandler();

  void SetRenderViewHost(RenderViewHostImpl* host);
  void SetClient(scoped_ptr<Client> client);

  Response Enable();
  Response Disable();

  Response Reload(const bool* ignoreCache,
                  const std::string* script_to_evaluate_on_load,
                  const std::string* script_preprocessor);

  Response Navigate(const std::string& url, FrameId* frame_id);

  Response GetNavigationHistory(int* current_index,
                                std::vector<NavigationEntry>* entries);

  Response NavigateToHistoryEntry(int entry_id);
  Response SetTouchEmulationEnabled(bool enabled);

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
  RenderViewHostImpl* host_;
  scoped_ptr<Client> client_;
  base::WeakPtrFactory<PageHandler> weak_factory_;

  void ScreenshotCaptured(
      scoped_refptr<DevToolsProtocol::Command> command,
      const unsigned char* png_data,
      size_t png_size);

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
