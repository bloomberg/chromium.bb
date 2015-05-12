// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_

#include <deque>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"

namespace content {

struct FrameNavigateParams;
struct LoadCommittedDetails;
struct PresentationSessionMessage;
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
    : public NON_EXPORTED_BASE(presentation::PresentationService),
      public mojo::ErrorHandler,
      public WebContentsObserver,
      public PresentationServiceDelegate::Observer {
 public:
  ~PresentationServiceImpl() override;

  // Static factory method to create an instance of PresentationServiceImpl.
  // |render_frame_host|: The RFH the instance is associated with.
  // |request|: The instance will be bound to this request. Used for Mojo setup.
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<presentation::PresentationService> request);

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
      SetDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      SetSameDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ClearDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ListenForDefaultSessionStart);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ListenForDefaultSessionStartAfterSet);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DefaultSessionStartReset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
                           ReceiveSessionMessagesAfterReset);

  using NewSessionMojoCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr,
          presentation::PresentationErrorPtr)>;
  using DefaultSessionMojoCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr)>;
  using SessionStateCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr,
          presentation::PresentationSessionState)>;
  using SessionMessagesCallback =
      mojo::Callback<void(mojo::Array<presentation::SessionMessagePtr>)>;
  using SendMessageMojoCallback = mojo::Callback<void(bool)>;

  // Listener implementation owned by PresentationServiceImpl. An instance of
  // this is created when an |onavailablechange| handler is added.
  // The instance receives screen availability results from the embedder and
  // propagates results back to PresentationServiceImpl.
  class CONTENT_EXPORT ScreenAvailabilityListenerImpl
      : public PresentationScreenAvailabilityListener {
   public:
    ScreenAvailabilityListenerImpl(
        const std::string& presentation_url,
        PresentationServiceImpl* service);
    ~ScreenAvailabilityListenerImpl() override;

    // PresentationScreenAvailabilityListener implementation.
    std::string GetPresentationUrl() const override;
    void OnScreenAvailabilityChanged(bool available) override;

   private:
    const std::string presentation_url_;
    PresentationServiceImpl* const service_;
  };

  class CONTENT_EXPORT DefaultSessionStartContext {
   public:
    DefaultSessionStartContext();
    ~DefaultSessionStartContext();

    // Adds a callback. May invoke the callback immediately if |session| using
    // default presentation URL was already started.
    void AddCallback(const DefaultSessionMojoCallback& callback);

    // Sets the session info. Maybe invoke callbacks queued with AddCallback().
    void set_session(const PresentationSessionInfo& session);

   private:
    // Flush all queued callbacks by invoking them with null
    // PresentationSessionInfoPtr.
    void Reset();

    ScopedVector<DefaultSessionMojoCallback> callbacks_;
    scoped_ptr<PresentationSessionInfo> session_;
  };

  // Context for a StartSession request.
  class CONTENT_EXPORT StartSessionRequest {
   public:
    StartSessionRequest(const std::string& presentation_url,
                        const std::string& presentation_id,
                        const NewSessionMojoCallback& callback);
    ~StartSessionRequest();

    // Retrieves the pending callback from this request, transferring ownership
    // to the caller.
    NewSessionMojoCallback PassCallback();

    const std::string& presentation_url() const { return presentation_url_; }
    const std::string& presentation_id() const { return presentation_id_; }

   private:
    const std::string presentation_url_;
    const std::string presentation_id_;
    NewSessionMojoCallback callback_;
  };

  // |render_frame_host|: The RFH this instance is associated with.
  // |web_contents|: The WebContents to observe.
  // |delegate|: Where Presentation API requests are delegated to. Not owned
  // by this class.
  PresentationServiceImpl(
      RenderFrameHost* render_frame_host,
      WebContents* web_contents,
      PresentationServiceDelegate* delegate);

  // PresentationService implementation.
  void SetDefaultPresentationURL(
      const mojo::String& presentation_url,
      const mojo::String& presentation_id) override;
  void SetClient(presentation::PresentationServiceClientPtr client) override;
  void ListenForScreenAvailability() override;
  void StopListeningForScreenAvailability() override;
  void ListenForDefaultSessionStart(
      const DefaultSessionMojoCallback& callback) override;
  void StartSession(
      const mojo::String& presentation_url,
      const mojo::String& presentation_id,
      const NewSessionMojoCallback& callback) override;
  void JoinSession(
      const mojo::String& presentation_url,
      const mojo::String& presentation_id,
      const NewSessionMojoCallback& callback) override;
  void SendSessionMessage(
      presentation::SessionMessagePtr session_message,
      const SendMessageMojoCallback& callback) override;
  void CloseSession(
      const mojo::String& presentation_url,
      const mojo::String& presentation_id) override;
  void ListenForSessionStateChange(
      const SessionStateCallback& callback) override;
  void ListenForSessionMessages(
      const SessionMessagesCallback& callback) override;

  // Creates a binding between this object and |request|.
  void Bind(mojo::InterfaceRequest<presentation::PresentationService> request);

  // mojo::ErrorHandler override.
  // Note that this is called when the RenderFrameHost is deleted.
  void OnConnectionError() override;

  // WebContentsObserver override.
  void DidNavigateAnyFrame(
      content::RenderFrameHost* render_frame_host,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // PresentationServiceDelegate::Observer
  void OnDelegateDestroyed() override;
  void OnDefaultPresentationStarted(const PresentationSessionInfo& session)
      override;

  // Finds the callback from |pending_session_cbs_| using |request_session_id|.
  // If it exists, invoke it with |session| and |error|, then erase it from
  // |pending_session_cbs_|.
  void RunAndEraseNewSessionMojoCallback(
      int request_session_id,
      presentation::PresentationSessionInfoPtr session,
      presentation::PresentationErrorPtr error);

  // Creates a new screen availability listener for |presentation_url| and
  // registers it with |delegate_|. Replaces the existing listener if any.
  void ResetScreenAvailabilityListener(const std::string& presentation_url);

  // Removes all listeners and resets default presentation URL on this instance
  // and informs the PresentationServiceDelegate of such.
  void Reset();

  // These functions are bound as base::Callbacks and passed to
  // embedder's implementation of PresentationServiceDelegate for later
  // invocation.
  void OnStartOrJoinSessionSucceeded(
      bool is_start_session,
      int request_session_id,
      const PresentationSessionInfo& session_info);
  void OnStartOrJoinSessionError(
      bool is_start_session,
      int request_session_id,
      const PresentationError& error);
  void OnSendMessageCallback();

  // Requests delegate to start a session.
  void DoStartSession(
      const std::string& presentation_url,
      const std::string& presentation_id,
      const NewSessionMojoCallback& callback);

  // Passed to embedder's implementation of PresentationServiceDelegate for
  // later invocation when session messages arrive.
  // For optimization purposes, this method will empty the messages
  // passed to it.
  void OnSessionMessages(
      scoped_ptr<ScopedVector<PresentationSessionMessage>> messages);

  // Removes the head of the queue (which represents the request that has just
  // been processed).
  // Checks if there are any queued StartSession requests and if so, executes
  // the first one in the queue.
  void HandleQueuedStartSessionRequests();

  // Associates |callback| with a unique request ID and stores it in a map.
  int RegisterNewSessionCallback(
    const NewSessionMojoCallback& callback);

  // Flushes all pending new session callbacks with error responses.
  void FlushNewSessionCallbacks();

  // Invokes |callback| with an error.
  static void InvokeNewSessionMojoCallbackWithError(
      const NewSessionMojoCallback& callback);

  // Returns true if this object is associated with |render_frame_host|.
  bool FrameMatches(content::RenderFrameHost* render_frame_host) const;

  // Embedder-specific delegate to forward Presentation requests to.
  // May be null if embedder does not support Presentation API.
  PresentationServiceDelegate* delegate_;

  // Proxy to the PresentationServiceClient to send results (e.g., screen
  // availability) to.
  presentation::PresentationServiceClientPtr client_;

  std::string default_presentation_url_;
  std::string default_presentation_id_;

  scoped_ptr<ScreenAvailabilityListenerImpl> screen_availability_listener_;

  // We only allow one StartSession request to be processed at a time.
  // StartSession requests are queued here. When a request has been processed,
  // it is removed from head of the queue.
  std::deque<linked_ptr<StartSessionRequest>> queued_start_session_requests_;

  // Indicates that a StartSession request is currently being processed.
  bool is_start_session_pending_;

  int next_request_session_id_;
  base::hash_map<int, linked_ptr<NewSessionMojoCallback>> pending_session_cbs_;

  scoped_ptr<DefaultSessionStartContext> default_session_start_context_;

  // RAII binding of |this| to an Presentation interface request.
  // The binding is removed when binding_ is cleared or goes out of scope.
  scoped_ptr<mojo::Binding<presentation::PresentationService>> binding_;

  // There can be only one send message request at a time.
  scoped_ptr<SendMessageMojoCallback> send_message_callback_;

  scoped_ptr<SessionMessagesCallback> on_session_messages_callback_;

  // ID of the RenderFrameHost this object is associated with.
  int render_process_id_;
  int render_frame_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PresentationServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PresentationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
