// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/presentation/presentation.mojom.h"
#include "url/gurl.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace content {

struct PresentationConnectionMessage;
class RenderFrameHost;

// Implementation of Mojo PresentationService.
// It handles Presentation API requests coming from Blink / renderer process
// and delegates the requests to the embedder's media router via
// PresentationServiceDelegate.
// An instance of this class tied to a RenderFrameHost and listens to events
// related to the RFH via implementing WebContentsObserver.
// This class is instantiated on-demand via Mojo's ConnectToRemoteService
// from the renderer when the first presentation API request is handled.
class CONTENT_EXPORT PresentationServiceImpl
    : public NON_EXPORTED_BASE(blink::mojom::PresentationService),
      public WebContentsObserver,
      public PresentationServiceDelegate::Observer {
 public:
  ~PresentationServiceImpl() override;

  using NewPresentationCallback =
      base::OnceCallback<void(const base::Optional<PresentationInfo>&,
                              const base::Optional<PresentationError>&)>;

  // Static factory method to create an instance of PresentationServiceImpl.
  // |render_frame_host|: The RFH the instance is associated with.
  // |request|: The instance will be bound to this request. Used for Mojo setup.
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      const service_manager::BindSourceInfo& source_info,
      mojo::InterfaceRequest<blink::mojom::PresentationService> request);

  // PresentationService implementation.
  void SetDefaultPresentationUrls(
      const std::vector<GURL>& presentation_urls) override;
  void SetClient(blink::mojom::PresentationServiceClientPtr client) override;
  void ListenForScreenAvailability(const GURL& url) override;
  void StopListeningForScreenAvailability(const GURL& url) override;
  void StartPresentation(const std::vector<GURL>& presentation_urls,
                         NewPresentationCallback callback) override;
  void ReconnectPresentation(const std::vector<GURL>& presentation_urls,
                             const base::Optional<std::string>& presentation_id,
                             NewPresentationCallback callback) override;
  void CloseConnection(const GURL& presentation_url,
                       const std::string& presentation_id) override;
  void Terminate(const GURL& presentation_url,
                 const std::string& presentation_id) override;
  void ListenForConnectionMessages(
      const PresentationInfo& presentation_info) override;
  void SetPresentationConnection(
      const PresentationInfo& presentation_info,
      blink::mojom::PresentationConnectionPtr controller_connection_ptr,
      blink::mojom::PresentationConnectionRequest receiver_connection_request)
      override;

 private:
  friend class PresentationServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, Reset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, ThisRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      OtherRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, DelegateFails);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionStateChange);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionClose);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingStartPresentationRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingReconnectPresentationRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiverPresentationServiceDelegate);

  // Maximum number of pending ReconnectPresentation requests at any given time.
  static const int kMaxQueuedRequests = 10;

  using ConnectionMessagesCallback =
      base::OnceCallback<void(std::vector<PresentationConnectionMessage>)>;

  // Listener implementation owned by PresentationServiceImpl. An instance of
  // this is created when PresentationRequest.getAvailability() is resolved.
  // The instance receives screen availability results from the embedder and
  // propagates results back to PresentationServiceImpl.
  class CONTENT_EXPORT ScreenAvailabilityListenerImpl
      : public PresentationScreenAvailabilityListener {
   public:
    ScreenAvailabilityListenerImpl(const GURL& availability_url,
                                   PresentationServiceImpl* service);
    ~ScreenAvailabilityListenerImpl() override;

    // PresentationScreenAvailabilityListener implementation.
    GURL GetAvailabilityUrl() const override;
    void OnScreenAvailabilityChanged(
        blink::mojom::ScreenAvailability availability) override;

   private:
    const GURL availability_url_;
    PresentationServiceImpl* const service_;
  };

  // Ensures the provided NewPresentationCallback is invoked exactly once
  // before it goes out of scope.
  class NewPresentationCallbackWrapper {
   public:
    explicit NewPresentationCallbackWrapper(NewPresentationCallback callback);
    ~NewPresentationCallbackWrapper();

    void Run(const base::Optional<PresentationInfo>& presentation_info,
             const base::Optional<PresentationError>& error);

   private:
    NewPresentationCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(NewPresentationCallbackWrapper);
  };

  // |render_frame_host|: The RFH this instance is associated with.
  // |web_contents|: The WebContents to observe.
  // |controller_delegate|: Where Presentation API requests are delegated to in
  // controller frame. Set to nullptr if current frame is receiver frame. Not
  // owned by this class.
  // |receiver_delegate|: Where Presentation API requests are delegated to in
  // receiver frame. Set to nullptr if current frame is controller frame. Not
  // owned by this class.
  PresentationServiceImpl(
      RenderFrameHost* render_frame_host,
      WebContents* web_contents,
      ControllerPresentationServiceDelegate* controller_delegate,
      ReceiverPresentationServiceDelegate* receiver_delegate);

  // Creates a binding between this object and |request|.
  void Bind(blink::mojom::PresentationServiceRequest request);

  // WebContentsObserver override.
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // PresentationServiceDelegate::Observer
  void OnDelegateDestroyed() override;

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when default presentation has started.
  void OnDefaultPresentationStarted(const PresentationInfo& presentation_info);

  // Finds the callback from |pending_reconnect_presentation_cbs_| using
  // |request_id|.
  // If it exists, invoke it with |presentation_info| and |error|, then erase it
  // from |pending_reconnect_presentation_cbs_|. Returns true if the callback
  // was found.
  bool RunAndEraseReconnectPresentationMojoCallback(
      int request_id,
      const base::Optional<PresentationInfo>& presentation_info,
      const base::Optional<PresentationError>& error);

  // Removes all listeners and resets default presentation URL on this instance
  // and informs the PresentationServiceDelegate of such.
  void Reset();

  // These functions are bound as base::Callbacks and passed to
  // embedder's implementation of PresentationServiceDelegate for later
  // invocation.
  void OnStartPresentationSucceeded(int request_id,
                                    const PresentationInfo& presentation_info);
  void OnStartPresentationError(int request_id, const PresentationError& error);
  void OnReconnectPresentationSucceeded(
      int request_id,
      const PresentationInfo& presentation_info);
  void OnReconnectPresentationError(int request_id,
                                    const PresentationError& error);

  // Calls to |delegate_| to start listening for state changes for |connection|.
  // State changes will be returned via |OnConnectionStateChanged|.
  void ListenForConnectionStateChange(const PresentationInfo& connection);

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when connection messages arrive.
  void OnConnectionMessages(
      const content::PresentationInfo& presentation_info,
      std::vector<content::PresentationConnectionMessage> messages);

  // A callback registered to OffscreenPresentationManager when
  // the PresentationServiceImpl for the presentation receiver is initialized.
  // Calls |client_| to create a new PresentationConnection on receiver page.
  void OnReceiverConnectionAvailable(
      const content::PresentationInfo& presentation_info,
      PresentationConnectionPtr controller_connection_ptr,
      PresentationConnectionRequest receiver_connection_request);

  // Associates a ReconnectPresentation |callback| with a unique request ID and
  // stores it in a map. Moves out |callback| object if |callback| is registered
  // successfully. If the queue is full, returns a negative value and leaves
  // |callback| as is.
  int RegisterReconnectPresentationCallback(NewPresentationCallback* callback);

  // Invoked by the embedder's PresentationServiceDelegate when a
  // PresentationConnection's state has changed.
  void OnConnectionStateChanged(
      const PresentationInfo& connection,
      const PresentationConnectionStateChangeInfo& info);

  // Returns true if this object is associated with |render_frame_host|.
  bool FrameMatches(content::RenderFrameHost* render_frame_host) const;

  // Returns |controller_delegate| if current frame is controller frame; Returns
  // |receiver_delegate| if current frame is receiver frame.
  PresentationServiceDelegate* GetPresentationServiceDelegate();

  // Embedder-specific delegate for controller to forward Presentation requests
  // to. Must be nullptr if current page is receiver page or
  // embedder does not support Presentation API .
  ControllerPresentationServiceDelegate* controller_delegate_;

  // Embedder-specific delegate for receiver to forward Presentation requests
  // to. Must be nullptr if current page is receiver page or
  // embedder does not support Presentation API.
  ReceiverPresentationServiceDelegate* receiver_delegate_;

  // Proxy to the PresentationServiceClient to send results (e.g., screen
  // availability) to.
  blink::mojom::PresentationServiceClientPtr client_;

  std::vector<GURL> default_presentation_urls_;

  using ScreenAvailabilityListenerMap =
      std::map<GURL, std::unique_ptr<ScreenAvailabilityListenerImpl>>;
  ScreenAvailabilityListenerMap screen_availability_listeners_;

  // For StartPresentation requests.
  // Set to a positive value when a StartPresentation request is being
  // processed.
  int start_presentation_request_id_;
  std::unique_ptr<NewPresentationCallbackWrapper>
      pending_start_presentation_cb_;

  // For ReconnectPresentation requests.
  base::hash_map<int, linked_ptr<NewPresentationCallbackWrapper>>
      pending_reconnect_presentation_cbs_;

  // RAII binding of |this| to an Presentation interface request.
  // The binding is removed when binding_ is cleared or goes out of scope.
  std::unique_ptr<mojo::Binding<blink::mojom::PresentationService>> binding_;

  // ID of the RenderFrameHost this object is associated with.
  int render_process_id_;
  int render_frame_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PresentationServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PresentationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
