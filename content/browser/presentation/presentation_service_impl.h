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
#include "base/memory/scoped_vector.h"
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

  using NewSessionCallback =
      base::Callback<void(const base::Optional<PresentationSessionInfo>&,
                          const base::Optional<PresentationError>&)>;

  // Static factory method to create an instance of PresentationServiceImpl.
  // |render_frame_host|: The RFH the instance is associated with.
  // |request|: The instance will be bound to this request. Used for Mojo setup.
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<blink::mojom::PresentationService> request);

 private:
  friend class PresentationServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, Reset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, DidNavigateThisFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DidNavigateOtherFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, ThisRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      OtherRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, DelegateFails);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           SetDefaultPresentationUrls);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           SetSameDefaultPresentationUrls);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ClearDefaultPresentationUrls);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ListenForDefaultSessionStart);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ListenForDefaultSessionStartAfterSet);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DefaultSessionStartReset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiveConnectionMessagesAfterReset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingStartSessionRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           MaxPendingJoinSessionRequests);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionStateChange);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ListenForConnectionClose);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           SetPresentationConnection);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiverPresentationServiceDelegate);

  // Maximum number of pending JoinSession requests at any given time.
  static const int kMaxNumQueuedSessionRequests = 10;

  using ConnectionMessagesCallback =
      base::Callback<void(std::vector<blink::mojom::ConnectionMessagePtr>)>;

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
    void OnScreenAvailabilityChanged(bool available) override;
    void OnScreenAvailabilityNotSupported() override;

   private:
    const GURL availability_url_;
    PresentationServiceImpl* const service_;
  };

  // Ensures the provided NewSessionCallback is invoked exactly once
  // before it goes out of scope.
  class NewSessionCallbackWrapper {
   public:
    explicit NewSessionCallbackWrapper(
        const NewSessionCallback& callback);
    ~NewSessionCallbackWrapper();

    void Run(const base::Optional<PresentationSessionInfo>& session_info,
             const base::Optional<PresentationError>& error);

   private:
    NewSessionCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(NewSessionCallbackWrapper);
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

  // PresentationService implementation.
  void SetDefaultPresentationUrls(
      const std::vector<GURL>& presentation_urls) override;
  void SetClient(blink::mojom::PresentationServiceClientPtr client) override;
  void ListenForScreenAvailability(const GURL& url) override;
  void StopListeningForScreenAvailability(const GURL& url) override;
  void StartSession(const std::vector<GURL>& presentation_urls,
                    const NewSessionCallback& callback) override;
  void JoinSession(const std::vector<GURL>& presentation_urls,
                   const base::Optional<std::string>& presentation_id,
                   const NewSessionCallback& callback) override;
  void CloseConnection(const GURL& presentation_url,
                       const std::string& presentation_id) override;
  void Terminate(const GURL& presentation_url,
                 const std::string& presentation_id) override;
  void ListenForConnectionMessages(
      const PresentationSessionInfo& session_info) override;
  void SetPresentationConnection(
      const PresentationSessionInfo& session_info,
      blink::mojom::PresentationConnectionPtr controller_connection_ptr,
      blink::mojom::PresentationConnectionRequest receiver_connection_request)
      override;

  // Creates a binding between this object and |request|.
  void Bind(mojo::InterfaceRequest<blink::mojom::PresentationService> request);

  // WebContentsObserver override.
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // PresentationServiceDelegate::Observer
  void OnDelegateDestroyed() override;

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when default presentation has started.
  void OnDefaultPresentationStarted(
      const PresentationSessionInfo& session_info);

  // Finds the callback from |pending_join_session_cbs_| using
  // |request_session_id|.
  // If it exists, invoke it with |session_info| and |error|, then erase it from
  // |pending_join_session_cbs_|.
  // Returns true if the callback was found.
  bool RunAndEraseJoinSessionMojoCallback(
      int request_session_id,
      const base::Optional<PresentationSessionInfo>& session_info,
      const base::Optional<PresentationError>& error);

  // Removes all listeners and resets default presentation URL on this instance
  // and informs the PresentationServiceDelegate of such.
  void Reset();

  // These functions are bound as base::Callbacks and passed to
  // embedder's implementation of PresentationServiceDelegate for later
  // invocation.
  void OnStartSessionSucceeded(
      int request_session_id,
      const PresentationSessionInfo& session_info);
  void OnStartSessionError(
      int request_session_id,
      const PresentationError& error);
  void OnJoinSessionSucceeded(
      int request_session_id,
      const PresentationSessionInfo& session_info);
  void OnJoinSessionError(
      int request_session_id,
      const PresentationError& error);

  // Calls to |delegate_| to start listening for state changes for |connection|.
  // State changes will be returned via |OnConnectionStateChanged|.
  void ListenForConnectionStateChange(
      const PresentationSessionInfo& connection);

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when session messages arrive.
  void OnConnectionMessages(
      const content::PresentationSessionInfo& session_info,
      const std::vector<std::unique_ptr<PresentationConnectionMessage>>&
          messages,
      bool pass_ownership);

  // A callback registered to OffscreenPresentationManager when
  // the PresentationServiceImpl for the presentation receiver is initialized.
  // Calls |client_| to create a new PresentationConnection on receiver page.
  void OnReceiverConnectionAvailable(
      const content::PresentationSessionInfo& session_info,
      PresentationConnectionPtr controller_connection_ptr,
      PresentationConnectionRequest receiver_connection_request);

  // Associates a JoinSession |callback| with a unique request ID and
  // stores it in a map.
  // Returns a positive value on success.
  int RegisterJoinSessionCallback(const NewSessionCallback& callback);

  // Invoked by the embedder's PresentationServiceDelegate when a
  // PresentationConnection's state has changed.
  void OnConnectionStateChanged(
      const PresentationSessionInfo& connection,
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

  // For StartSession requests.
  // Set to a positive value when a StartSession request is being processed.
  int start_session_request_id_;
  std::unique_ptr<NewSessionCallbackWrapper> pending_start_session_cb_;

  // For JoinSession requests.
  base::hash_map<int, linked_ptr<NewSessionCallbackWrapper>>
      pending_join_session_cbs_;

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
