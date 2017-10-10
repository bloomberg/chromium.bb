// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/presentation_info.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationClient.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebPresentationAvailabilityObserver;
class WebPresentationReceiver;
class WebString;
class WebURL;
template <typename T>
class WebVector;
}  // namespace blink

namespace content {

// PresentationDispatcher is a delegate for Presentation API used by
// Blink. It forwards the calls to the Mojo PresentationService.
class CONTENT_EXPORT PresentationDispatcher
    : public RenderFrameObserver,
      public blink::WebPresentationClient,
      public blink::mojom::PresentationServiceClient {
 public:
  explicit PresentationDispatcher(RenderFrame* render_frame);
  ~PresentationDispatcher() override;

 private:
  friend class TestPresentationDispatcher;
  friend class PresentationDispatcherTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestStartPresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestStartPresentationError);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestReconnectPresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestReconnectPresentationError);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestReconnectPresentationNoConnection);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestOnReceiverConnectionAvailable);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest, TestCloseConnection);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestTerminatePresentation);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestListenForScreenAvailability);
  FRIEND_TEST_ALL_PREFIXES(PresentationDispatcherTest,
                           TestSetDefaultPresentationUrls);

  // WebPresentationClient implementation.
  void SetController(blink::WebPresentationController* controller) override;
  void SetReceiver(blink::WebPresentationReceiver*) override;
  void StartPresentation(
      const blink::WebVector<blink::WebURL>& presentationUrls,
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback)
      override;
  void ReconnectPresentation(
      const blink::WebVector<blink::WebURL>& presentationUrls,
      const blink::WebString& presentationId,
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback)
      override;
  void GetAvailability(
      const blink::WebVector<blink::WebURL>& availabilityUrls,
      std::unique_ptr<blink::WebPresentationAvailabilityCallbacks> callbacks)
      override;
  void StartListening(blink::WebPresentationAvailabilityObserver*) override;
  void StopListening(blink::WebPresentationAvailabilityObserver*) override;
  void SetDefaultPresentationUrls(
      const blink::WebVector<blink::WebURL>& presentationUrls) override;

  // RenderFrameObserver implementation.
  void DidFinishDocumentLoad() override;
  void OnDestruct() override;
  void WidgetWillClose() override;

  // blink::mojom::PresentationServiceClient
  void OnScreenAvailabilityUpdated(
      const GURL& url,
      blink::mojom::ScreenAvailability availability) override;
  void OnConnectionStateChanged(const PresentationInfo& presentation_info,
                                PresentationConnectionState state) override;
  void OnConnectionClosed(const PresentationInfo& presentation_info,
                          PresentationConnectionCloseReason reason,
                          const std::string& message) override;
  void OnDefaultPresentationStarted(
      const PresentationInfo& presentation_info) override;

  void OnConnectionCreated(
      std::unique_ptr<blink::WebPresentationConnectionCallbacks> callback,
      const base::Optional<PresentationInfo>& presentation_info,
      const base::Optional<PresentationError>& error);

  virtual void ConnectToPresentationServiceIfNeeded();

  void UpdateListeningState();

  // Used as a weak reference. Can be null since lifetime is bound to the frame.
  blink::WebPresentationController* controller_;
  blink::WebPresentationReceiver* receiver_;
  blink::mojom::PresentationServicePtr presentation_service_;
  mojo::Binding<blink::mojom::PresentationServiceClient> binding_;

  enum class ListeningState {
    INACTIVE,
    WAITING,
    ACTIVE,
  };

  using AvailabilityCallbacksMap =
      base::IDMap<std::unique_ptr<blink::WebPresentationAvailabilityCallbacks>>;
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
    blink::mojom::ScreenAvailability last_known_availability;
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
  // otherwise returns DISABLED if at least one url in |urls| has screen
  // availability DISABLED;
  // otherwise, returns SOURCE_NOT_SUPPORTED if any url in |urls| has screen
  // availability SOURCE_NOT_SUPPORTED;
  // otherwise, returns UNAVAILABLE if any url in |urls| has screen
  // availability UNAVAILABLE;
  // otherwise returns UNKNOWN.
  blink::mojom::ScreenAvailability GetScreenAvailability(
      const std::vector<GURL>& urls) const;

  DISALLOW_COPY_AND_ASSIGN(PresentationDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_DISPATCHER_H_
