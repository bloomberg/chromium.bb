// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/presentation_session.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebPresentationAvailabilityObserver;
class WebPresentationConnection;
class WebPresentationReceiver;
class WebString;
class WebURL;
template <typename T>
class WebVector;
}  // namespace blink

namespace content {

class TestPresentationDispatcher;

// PresentationDispatcher is a delegate for Presentation API messages used by
// Blink. It forwards the calls to the Mojo PresentationService.
class CONTENT_EXPORT PresentationDispatcher
    : public RenderFrameObserver,
      public NON_EXPORTED_BASE(blink::WebPresentationClient),
      public NON_EXPORTED_BASE(blink::mojom::PresentationServiceClient) {
 public:
  explicit PresentationDispatcher(RenderFrame* render_frame);
  ~PresentationDispatcher() override;

 private:
  // TODO(crbug.com/684116): remove friend class after moving message queue
  // logic from PresentationDispatcher to PresentationConnectionProxy.
  friend class PresentationConnectionProxy;
  friend class TestPresentationDispatcher;
  friend class PresentationDispatcherTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestStartSession);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestStartSessionError);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestJoinSession);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestJoinSessionError);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestSendString);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestSendArrayBuffer);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestSendBlobData);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestOnReceiverConnectionAvailable);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestCloseSession);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestTerminateSession);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestListenForScreenAvailability);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestSetDefaultPresentationUrls);

  struct SendMessageRequest {
    SendMessageRequest(
        const PresentationSessionInfo& session_info,
        blink::mojom::ConnectionMessagePtr message,
        const blink::WebPresentationConnectionProxy* connection_proxy);

    ~SendMessageRequest();

    PresentationSessionInfo session_info;
    blink::mojom::ConnectionMessagePtr message;
    // Proxy of Blink connection object |connection| calling connection.send().
    // It does not take ownership of proxy object. Proxy object is owned by
    // Blink connection. Blink connection is destroyed after
    // PresentationDispatcher so |connection_proxy| should always be valid.
    const blink::WebPresentationConnectionProxy* connection_proxy;
  };

  static SendMessageRequest* CreateSendTextMessageRequest(
      const blink::WebURL& presentationUrl,
      const blink::WebString& presentationId,
      const blink::WebString& message,
      const blink::WebPresentationConnectionProxy* connection_proxy);
  static SendMessageRequest* CreateSendBinaryMessageRequest(
      const blink::WebURL& presentationUrl,
      const blink::WebString& presentationId,
      blink::mojom::PresentationMessageType type,
      const uint8_t* data,
      size_t length,
      const blink::WebPresentationConnectionProxy* connection_proxy);

  // WebPresentationClient implementation.
  void setController(blink::WebPresentationController* controller) override;
  void setReceiver(blink::WebPresentationReceiver*) override;
  void startSession(const blink::WebVector<blink::WebURL>& presentationUrls,
                    std::unique_ptr<blink::WebPresentationConnectionCallbacks>
                        callback) override;
  void joinSession(const blink::WebVector<blink::WebURL>& presentationUrls,
                   const blink::WebString& presentationId,
                   std::unique_ptr<blink::WebPresentationConnectionCallbacks>
                       callback) override;
  void sendString(
      const blink::WebURL& presentationUrl,
      const blink::WebString& presentationId,
      const blink::WebString& message,
      const blink::WebPresentationConnectionProxy* connection_proxy) override;
  void sendArrayBuffer(
      const blink::WebURL& presentationUrl,
      const blink::WebString& presentationId,
      const uint8_t* data,
      size_t length,
      const blink::WebPresentationConnectionProxy* connection_proxy) override;
  void sendBlobData(
      const blink::WebURL& presentationUrl,
      const blink::WebString& presentationId,
      const uint8_t* data,
      size_t length,
      const blink::WebPresentationConnectionProxy* connection_proxy) override;
  void closeSession(
      const blink::WebURL& presentationUrl,
      const blink::WebString& presentationId,
      const blink::WebPresentationConnectionProxy* connection_proxy) override;
  void terminateSession(const blink::WebURL& presentationUrl,
                        const blink::WebString& presentationId) override;
  void getAvailability(
      const blink::WebVector<blink::WebURL>& availabilityUrls,
      std::unique_ptr<blink::WebPresentationAvailabilityCallbacks> callbacks)
      override;
  void startListening(blink::WebPresentationAvailabilityObserver*) override;
  void stopListening(blink::WebPresentationAvailabilityObserver*) override;
  void setDefaultPresentationUrls(
      const blink::WebVector<blink::WebURL>& presentationUrls) override;

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(
      bool is_new_navigation,
      bool is_same_page_navigation) override;
  void OnDestruct() override;

  // blink::mojom::PresentationServiceClient
  void OnScreenAvailabilityNotSupported(const GURL& url) override;
  void OnScreenAvailabilityUpdated(const GURL& url, bool available) override;
  void OnConnectionStateChanged(const PresentationSessionInfo& session_info,
                                PresentationConnectionState state) override;
  void OnConnectionClosed(const PresentationSessionInfo& session_info,
                          PresentationConnectionCloseReason reason,
                          const std::string& message) override;
  void OnConnectionMessagesReceived(
      const PresentationSessionInfo& session_info,
      std::vector<blink::mojom::ConnectionMessagePtr> messages) override;
  void OnDefaultSessionStarted(
      const PresentationSessionInfo& session_info) override;

  void OnSessionCreated(
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback,
      const base::Optional<PresentationSessionInfo>& session_info,
      const base::Optional<PresentationError>& error);
  void OnReceiverConnectionAvailable(
      const PresentationSessionInfo& session_info,
      blink::mojom::PresentationConnectionPtr /*connection*/,
      blink::mojom::PresentationConnectionRequest /*connection_request*/)
      override;

  // Call to PresentationService to send the message in |request|.
  // |session_info| and |message| of |reuqest| will be consumed.
  // |HandleSendMessageRequests| will be invoked after the send is attempted.
  void DoSendMessage(SendMessageRequest* request);
  void HandleSendMessageRequests(bool success);

  // Creates ControllerConnectionProxy object |controller_connection_proxy| with
  // |connection|. Sends mojo interface ptr of |controller_connection_proxy|
  // and mojo interface request of |controller_connection_proxy|'s
  // |target_connection_| to PresentationService.
  // |session_info|: |connection|'s id and url;
  // |connection|: |controller_connection_proxy|'s |source_connection_|. Raw
  // pointer to Blink connection owning proxy object. It does not take object
  // ownership.
  void SetControllerConnection(const PresentationSessionInfo& session_info,
                               blink::WebPresentationConnection* connection);

  virtual void ConnectToPresentationServiceIfNeeded();

  void UpdateListeningState();

  // Used as a weak reference. Can be null since lifetime is bound to the frame.
  blink::WebPresentationController* controller_;
  blink::WebPresentationReceiver* receiver_;
  blink::mojom::PresentationServicePtr presentation_service_;
  mojo::Binding<blink::mojom::PresentationServiceClient> binding_;

  // Message requests are queued here and only one message at a time is sent
  // over mojo channel.
  using MessageRequestQueue = std::deque<std::unique_ptr<SendMessageRequest>>;
  MessageRequestQueue message_request_queue_;

  enum class ListeningState {
    INACTIVE,
    WAITING,
    ACTIVE,
  };

  // Do not change order or add new enum values. |GetScreenAvailability| impl
  // depends on the order of the enum values.
  enum class ScreenAvailability {
    UNKNOWN = 0,
    UNAVAILABLE,
    UNSUPPORTED,
    AVAILABLE
  };

  using AvailabilityCallbacksMap =
      IDMap<std::unique_ptr<blink::WebPresentationAvailabilityCallbacks>>;
  using AvailabilityObserversSet =
      std::set<blink::WebPresentationAvailabilityObserver*>;

  // Tracks listeners of presentation displays availability for
  // |availability_urls|.
  struct AvailabilityListener {
    explicit AvailabilityListener(const std::vector<GURL>& availability_urls);
    ~AvailabilityListener();

    const std::vector<GURL> urls;
    AvailabilityCallbacksMap availability_callbacks;
    AvailabilityObserversSet availability_observers;
  };

  // Tracks listening status of |availability_url|.
  struct ListeningStatus {
    explicit ListeningStatus(const GURL& availability_url);
    ~ListeningStatus();

    const GURL url;
    ScreenAvailability last_known_availability;
    ListeningState listening_state;
  };

  // Map of ListeningStatus for known URLs.
  std::map<GURL, std::unique_ptr<ListeningStatus>> listening_status_;

  // Set of AvailabilityListener for known PresentationRequest.
  std::set<std::unique_ptr<AvailabilityListener>> availability_set_;

  // Starts listening to |url|.
  void StartListeningToURL(const GURL& url);

  // Stops listening to |url| if no PresentationAvailability is observing |url|.
  // StartListening() must have been called first.
  void MaybeStopListeningToURL(const GURL& url);

  // Returns nullptr if there is no status for |url|.
  ListeningStatus* GetListeningStatus(const GURL& url) const;

  // Returns nullptr if there is no availability listener for |urls|.
  AvailabilityListener* GetAvailabilityListener(
      const std::vector<GURL>& urls) const;

  // Removes |listener| from |availability_set_| if |listener| has no callbacks
  // and no observers.
  void TryRemoveAvailabilityListener(AvailabilityListener* listener);

  // Returns AVAILABLE if any url in |urls| has screen availability AVAILABLE;
  // Returns UNSUPPORTED if any url in |urls| have screen availability
  // UNSUPPORTED, and no url has screen availability AVAILABLE;
  // Returns UNAVAILABLE if at least one url in |urls| has screen availability
  // UNAVAILABLE, and no url has screen availability AVAILABLE or UNSUPPORTED;
  // Returns UNKNOWN if all urls in |urls| have screen availability
  // UNKNOWN.
  ScreenAvailability GetScreenAvailability(const std::vector<GURL>& urls) const;

  DISALLOW_COPY_AND_ASSIGN(PresentationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
