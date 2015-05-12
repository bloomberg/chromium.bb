// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {
class WebString;
}  // namespace blink

namespace content {

// PresentationDispatcher is a delegate for Presentation API messages used by
// Blink. It forwards the calls to the Mojo PresentationService.
class CONTENT_EXPORT PresentationDispatcher
    : public RenderFrameObserver,
      public NON_EXPORTED_BASE(blink::WebPresentationClient),
      public NON_EXPORTED_BASE(presentation::PresentationServiceClient) {
 public:
  explicit PresentationDispatcher(RenderFrame* render_frame);
  ~PresentationDispatcher() override;

 private:
  // WebPresentationClient implementation.
  virtual void setController(
      blink::WebPresentationController* controller);
  virtual void updateAvailableChangeWatched(bool watched);
  virtual void startSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      blink::WebPresentationSessionClientCallbacks* callback);
  virtual void joinSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      blink::WebPresentationSessionClientCallbacks* callback);
  virtual void sendString(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      const blink::WebString& message);
  virtual void sendArrayBuffer(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      const uint8* data,
      size_t length);
  virtual void closeSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId);

  // RenderFrameObserver implementation.
  void DidChangeDefaultPresentation() override;
  void DidCommitProvisionalLoad(
      bool is_new_navigation,
      bool is_same_page_navigation) override;

  // presentation::PresentationServiceClient
  void OnScreenAvailabilityUpdated(bool available) override;

  void OnSessionCreated(
      blink::WebPresentationSessionClientCallbacks* callback,
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationErrorPtr error);
  void OnDefaultSessionStarted(
      presentation::PresentationSessionInfoPtr session_info);
  void OnSessionStateChange(
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationSessionState session_state);
  void OnSessionMessagesReceived(
      mojo::Array<presentation::SessionMessagePtr> messages);
  void DoSendMessage(const presentation::SessionMessage& session_message);
  void HandleSendMessageRequests(bool success);

  void ConnectToPresentationServiceIfNeeded();

  // Used as a weak reference. Can be null since lifetime is bound to the frame.
  blink::WebPresentationController* controller_;
  presentation::PresentationServicePtr presentation_service_;
  mojo::Binding<presentation::PresentationServiceClient> binding_;

  // Message requests are queued here and only one message at a time is sent
  // over mojo channel.
  using MessageRequestQueue =
      std::queue<linked_ptr<presentation::SessionMessage>>;
  MessageRequestQueue message_request_queue_;

  DISALLOW_COPY_AND_ASSIGN(PresentationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
