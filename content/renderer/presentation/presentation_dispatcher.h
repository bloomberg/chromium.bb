// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/memory/linked_ptr.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {
class WebPresentationAvailabilityObserver;
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
  struct SendMessageRequest {
    SendMessageRequest(presentation::PresentationSessionInfoPtr session_info,
                       presentation::SessionMessagePtr message);
    ~SendMessageRequest();

    presentation::PresentationSessionInfoPtr session_info;
    presentation::SessionMessagePtr message;
  };

  static SendMessageRequest* CreateSendTextMessageRequest(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      const blink::WebString& message);
  static SendMessageRequest* CreateSendBinaryMessageRequest(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      presentation::PresentationMessageType type,
      const uint8* data,
      size_t length);

  // WebPresentationClient implementation.
  virtual void setController(
      blink::WebPresentationController* controller);
  virtual void updateAvailableChangeWatched(bool watched);
  virtual void startSession(
      const blink::WebString& presentationUrl,
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
  virtual void sendBlobData(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      const uint8* data,
      size_t length);
  virtual void closeSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId);
  virtual void getAvailability(
      const blink::WebString& presentationUrl,
      blink::WebPresentationAvailabilityCallbacks* callbacks);
  virtual void startListening(blink::WebPresentationAvailabilityObserver*);
  virtual void stopListening(blink::WebPresentationAvailabilityObserver*);
  virtual void setDefaultPresentationUrl(const blink::WebString& url);

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(
      bool is_new_navigation,
      bool is_same_page_navigation) override;

  // presentation::PresentationServiceClient
  void OnScreenAvailabilityUpdated(bool available) override;
  void OnSessionStateChanged(
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationSessionState new_state) override;
  void OnScreenAvailabilityNotSupported() override;
  void OnSessionMessagesReceived(
      presentation::PresentationSessionInfoPtr session_info,
      mojo::Array<presentation::SessionMessagePtr> messages) override;

  void OnSessionCreated(
      blink::WebPresentationSessionClientCallbacks* callback,
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationErrorPtr error);
  void OnDefaultSessionStarted(
      presentation::PresentationSessionInfoPtr session_info);

  // Call to PresentationService to send the message in |request|.
  // |session_info| and |message| of |reuqest| will be consumed.
  // |HandleSendMessageRequests| will be invoked after the send is attempted.
  void DoSendMessage(SendMessageRequest* request);
  void HandleSendMessageRequests(bool success);

  void ConnectToPresentationServiceIfNeeded();

  void UpdateListeningState();

  // Used as a weak reference. Can be null since lifetime is bound to the frame.
  blink::WebPresentationController* controller_;
  presentation::PresentationServicePtr presentation_service_;
  mojo::Binding<presentation::PresentationServiceClient> binding_;

  // Message requests are queued here and only one message at a time is sent
  // over mojo channel.
  using MessageRequestQueue = std::queue<linked_ptr<SendMessageRequest>>;
  MessageRequestQueue message_request_queue_;

  enum class ListeningState {
    Inactive,
    Waiting,
    Active,
  };

  ListeningState listening_state_;
  bool last_known_availability_;

  using AvailabilityCallbacksMap =
      IDMap<blink::WebPresentationAvailabilityCallbacks, IDMapOwnPointer>;
  AvailabilityCallbacksMap availability_callbacks_;

  using AvailabilityObserversSet =
      std::set<blink::WebPresentationAvailabilityObserver*>;
  AvailabilityObserversSet availability_observers_;

  DISALLOW_COPY_AND_ASSIGN(PresentationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
