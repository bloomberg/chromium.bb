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
    : public NON_EXPORTED_BASE(
          mojo::InterfaceImpl<presentation::PresentationService>),
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
  using ScreenAvailabilityMojoCallback =
      mojo::Callback<void(mojo::String, bool)>;
  using NewSessionMojoCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr,
          presentation::PresentationErrorPtr)>;
  using DefaultSessionMojoCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr)>;

  // A helper data class used by PresentationServiceImpl to do bookkeeping
  // of currently registered screen availability listeners.
  // An instance of this class is a simple state machine that waits for both
  // the available bit and the Mojo callback to become available.
  // Once this happens, the Mojo callback will be invoked with the available
  // bit, and the state machine will reset.
  // The available bit is obtained from the embedder's media router.
  // The callback is obtained from the renderer via PresentationServiceImpl's
  // GetScreenAvailability().
  class CONTENT_EXPORT ScreenAvailabilityContext
      : public PresentationScreenAvailabilityListener {
   public:
    explicit ScreenAvailabilityContext(
        const std::string& presentation_url);
    ~ScreenAvailabilityContext() override;

    // If available bit exists, |callback| will be invoked with the bit and
    // this state machine will reset.
    // Otherwise |callback| is saved for later use.
    // |callback|: Callback to the client of PresentationService
    // (i.e. the renderer) that screen availability has changed, via Mojo.
    void CallbackReceived(const ScreenAvailabilityMojoCallback& callback);

    // Clears both callback and available bit.
    void Reset();

    // PresentationScreenAvailabilityListener implementation.
    std::string GetPresentationUrl() const override;

    // If callback exists, it will be invoked with |available| and
    // this state machine will reset.
    // Otherwise |available| is saved for later use.
    // |available|: New screen availability for the presentation URL.
    void OnScreenAvailabilityChanged(bool available) override;

    // Gets the callback as a raw pointer.
    ScreenAvailabilityMojoCallback* GetCallback() const;

   private:
    std::string presentation_url_;
    scoped_ptr<ScreenAvailabilityMojoCallback> callback_ptr_;
    scoped_ptr<bool> available_ptr_;
  };

  // Context for a StartSession request.
  struct CONTENT_EXPORT StartSessionRequest {
    StartSessionRequest(const std::string& presentation_url,
                        const std::string& presentation_id,
                        const NewSessionMojoCallback& callback);
    ~StartSessionRequest();

    const std::string presentation_url;
    const std::string presentation_id;
    const NewSessionMojoCallback callback;
  };

  friend class PresentationServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, Reset);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DidNavigateThisFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DidNavigateNotThisFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ThisRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      NotThisRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      SetDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      SetSameDefaultPresentationUrl);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ClearDefaultPresentationUrl);

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
  void GetScreenAvailability(
      const mojo::String& presentation_url,
      const ScreenAvailabilityMojoCallback& callback) override;
  void OnScreenAvailabilityListenerRemoved(
      const mojo::String& presentation_url) override;
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
  void CloseSession(
      const mojo::String& presentation_url,
      const mojo::String& presentation_id) override;

  // mojo::InterfaceImpl override.
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

  // Sets |default_presentation_url_| to |presentation_url| and informs the
  // delegate of new default presentation URL and ID.
  void DoSetDefaultPresentationUrl(
      const std::string& presentation_url,
      const std::string& presentation_id);

  // Removes all listeners and resets default presentation URL on this instance
  // and informs the PresentationServiceDelegate of such.
  void Reset();

  // These two functions are bound as base::Callbacks and passed to
  // embedder's implementation of PresentationServiceDelegate for later
  // invocation.
  void OnStartOrJoinSessionSucceeded(
      bool is_start_session,
      const NewSessionMojoCallback& callback,
      const PresentationSessionInfo& session_info);
  void OnStartOrJoinSessionError(
      bool is_start_session,
      const NewSessionMojoCallback& callback,
      const PresentationError& error);

  // Requests delegate to start a session.
  void DoStartSession(
      const std::string& presentation_url,
      const std::string& presentation_id,
      const NewSessionMojoCallback& callback);

  // Removes the head of the queue (which represents the request that has just
  // been processed).
  // Checks if there are any queued StartSession requests and if so, executes
  // the first one in the queue.
  void HandleQueuedStartSessionRequests();

  // Gets the ScreenAvailabilityContext for |presentation_url|, or creates one
  // if it does not exist.
  ScreenAvailabilityContext* GetOrCreateAvailabilityContext(
      const std::string& presentation_url);

  RenderFrameHost* render_frame_host_;
  PresentationServiceDelegate* delegate_;

  // Map from presentation URL to its ScreenAvailabilityContext state machine.
  base::hash_map<std::string, linked_ptr<ScreenAvailabilityContext>>
      availability_contexts_;

  std::string default_presentation_url_;
  std::string default_presentation_id_;

  // We only allow one StartSession request to be processed at a time.
  // StartSession requests are queued here. When a request has been processed,
  // it is removed from head of the queue.
  std::deque<linked_ptr<StartSessionRequest>> queued_start_session_requests_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<PresentationServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PresentationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
