// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <queue>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
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
      const uint8_t* data,
      size_t length);

  // WebPresentationClient implementation.
  void setController(blink::WebPresentationController* controller) override;
  void startSession(
      const blink::WebString& presentationUrl,
      blink::WebPresentationConnectionClientCallbacks* callback) override;
  void joinSession(
      const blink::WebString& presentationUrl,
      const blink::WebString& presentationId,
      blink::WebPresentationConnectionClientCallbacks* callback) override;
  void sendString(const blink::WebString& presentationUrl,
                  const blink::WebString& presentationId,
                  const blink::WebString& message) override;
  void sendArrayBuffer(const blink::WebString& presentationUrl,
                       const blink::WebString& presentationId,
                       const uint8_t* data,
                       size_t length) override;
  void sendBlobData(const blink::WebString& presentationUrl,
                    const blink::WebString& presentationId,
                    const uint8_t* data,
                    size_t length) override;
  void closeSession(const blink::WebString& presentationUrl,
                    const blink::WebString& presentationId) override;
  void terminateSession(const blink::WebString& presentationUrl,
                        const blink::WebString& presentationId) override;
  void getAvailability(
      const blink::WebString& availabilityUrl,
      blink::WebPresentationAvailabilityCallbacks* callbacks) override;
  void startListening(blink::WebPresentationAvailabilityObserver*) override;
  void stopListening(blink::WebPresentationAvailabilityObserver*) override;
  void setDefaultPresentationUrl(const blink::WebString& url) override;

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(
      bool is_new_navigation,
      bool is_same_page_navigation) override;

  // presentation::PresentationServiceClient
  void OnScreenAvailabilityNotSupported(const mojo::String& url) override;
  void OnScreenAvailabilityUpdated(const mojo::String& url,
                                   bool available) override;
  void OnConnectionStateChanged(
      presentation::PresentationSessionInfoPtr connection,
      presentation::PresentationConnectionState state) override;
  void OnSessionMessagesReceived(
      presentation::PresentationSessionInfoPtr session_info,
      mojo::Array<presentation::SessionMessagePtr> messages) override;
  void OnDefaultSessionStarted(
      presentation::PresentationSessionInfoPtr session_info) override;

  void OnSessionCreated(
      blink::WebPresentationConnectionClientCallbacks* callback,
      presentation::PresentationSessionInfoPtr session_info,
      presentation::PresentationErrorPtr error);

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
  using MessageRequestQueue = std::queue<scoped_ptr<SendMessageRequest>>;
  MessageRequestQueue message_request_queue_;

  enum class ListeningState {
    INACTIVE,
    WAITING,
    ACTIVE,
  };

  using AvailabilityCallbacksMap =
      IDMap<blink::WebPresentationAvailabilityCallbacks, IDMapOwnPointer>;
  using AvailabilityObserversSet =
      std::set<blink::WebPresentationAvailabilityObserver*>;

  // Tracks status of presentation displays availability for |availability_url|.
  struct AvailabilityStatus {
    explicit AvailabilityStatus(const std::string& availability_url);
    ~AvailabilityStatus();

    const std::string url;
    bool last_known_availability;
    ListeningState listening_state;
    AvailabilityCallbacksMap availability_callbacks;
    AvailabilityObserversSet availability_observers;
  };

  std::map<std::string, scoped_ptr<AvailabilityStatus>> availability_status_;

  // Updates the listening state of availability for |status| and notifies the
  // client.
  void UpdateListeningState(AvailabilityStatus* status);

  DISALLOW_COPY_AND_ASSIGN(PresentationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
