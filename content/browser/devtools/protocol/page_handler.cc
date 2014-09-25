// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/page_handler.h"

#include "base/base64.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {
namespace devtools {
namespace page {

typedef DevToolsProtocolClient::Response Response;

PageHandler::PageHandler()
    : weak_factory_(this) {
}

PageHandler::~PageHandler() {
}

void PageHandler::SetRenderViewHost(RenderViewHostImpl* host) {
  host_ = host;
}

void PageHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

Response PageHandler::Enable() {
  return Response::FallThrough();
}

Response PageHandler::Disable() {
  return Response::FallThrough();
}

Response PageHandler::Reload(const bool* ignoreCache,
                             const std::string* script_to_evaluate_on_load,
                             const std::string* script_preprocessor) {
  return Response::FallThrough();
}

Response PageHandler::Navigate(const std::string& url,
                               FrameId* frame_id) {
  return Response::FallThrough();
}

Response PageHandler::GetNavigationHistory(
    int* current_index,
    std::vector<NavigationEntry>* entries) {
  return Response::FallThrough();
}

Response PageHandler::NavigateToHistoryEntry(int entry_id) {
  return Response::FallThrough();
}

Response PageHandler::SetTouchEmulationEnabled(bool enabled) {
  return Response::FallThrough();
}

scoped_refptr<DevToolsProtocol::Response> PageHandler::CaptureScreenshot(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (!host_ || !host_->GetView())
    return command->InternalErrorResponse("Could not connect to view");

  host_->GetSnapshotFromBrowser(
      base::Bind(&PageHandler::ScreenshotCaptured,
          weak_factory_.GetWeakPtr(), command));
  return command->AsyncResponsePromise();
}

Response PageHandler::CanScreencast(bool* result) {
  return Response::FallThrough();
}

Response PageHandler::CanEmulate(bool* result) {
  return Response::FallThrough();
}

Response PageHandler::StartScreencast(const std::string* format,
                                      const int* quality,
                                      const int* max_width,
                                      const int* max_height) {
  return Response::FallThrough();
}

Response PageHandler::StopScreencast() {
  return Response::FallThrough();
}

Response PageHandler::HandleJavaScriptDialog(bool accept,
                                             const std::string* prompt_text) {
  return Response::FallThrough();
}

scoped_refptr<DevToolsProtocol::Response> PageHandler::QueryUsageAndQuota(
    const std::string& security_origin,
    scoped_refptr<DevToolsProtocol::Command> command) {
  return NULL;
}

Response PageHandler::SetColorPickerEnabled(bool enabled) {
  return Response::FallThrough();
}

void PageHandler::ScreenshotCaptured(
    scoped_refptr<DevToolsProtocol::Command> command,
    const unsigned char* png_data,
    size_t png_size) {
  if (!png_data || !png_size) {
    client_->SendInternalErrorResponse(command,
                                       "Unable to capture screenshot");
    return;
  }

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(png_data), png_size),
      &base_64_data);

  CaptureScreenshotResponse response;
  response.set_data(base_64_data);
  client_->SendCaptureScreenshotResponse(command, response);
}

}  // namespace page
}  // namespace devtools
}  // namespace content
