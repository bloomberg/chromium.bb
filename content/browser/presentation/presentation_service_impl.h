// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/common/presentation/presentation_session.mojom.h"
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
  using ScreenAvailabilityMojoCallback = mojo::Callback<void(bool)>;
  using NewSessionMojoCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr,
          presentation::PresentationErrorPtr)>;
  using DefaultSessionMojoCallback =
      mojo::Callback<void(presentation::PresentationSessionInfoPtr)>;

  friend class PresentationServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest, RemoveAllListeners);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DidNavigateThisFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      DidNavigateNotThisFrame);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      ThisRenderFrameDeleted);
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceImplTest,
      NotThisRenderFrameDeleted);

  // |render_frame_host|: The RFH this instance is associated with.
  // |web_contents|: The WebContents to observe.
  // |delegate|: Where Presentation API requests are delegated to. Not owned
  // by this class.
  PresentationServiceImpl(
      RenderFrameHost* render_frame_host,
      WebContents* web_contents,
      PresentationServiceDelegate* delegate);

  // PresentationService implementation.
  void GetScreenAvailability(
      const mojo::String& presentation_url,
      const ScreenAvailabilityMojoCallback& callback) override;
  void OnScreenAvailabilityListenerRemoved() override;
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

  // Removes all listeners on this instance and informs the
  // PresentationServiceDelegate of such.
  void RemoveAllListeners();

  RenderFrameHost* render_frame_host_;
  PresentationServiceDelegate* delegate_;

  // A helper data class used by PresentationServiceImpl to do bookkeeping
  // of currently registered screen availability listeners.
  // An instance of this class is a simple state machine that waits for both
  // the available bit and the Mojo callback to become available.
  // Once this happens, the Mojo callback will be invoked with the available
  // bit, and the state machine will reset.
  // The available bit is obtained from the embedder's media router.
  // The callback is obtained from the renderer via PresentationServiceImpl's
  // GetScreenAvailability().
  class ScreenAvailabilityContext
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

   private:
    std::string presentation_url_;
    scoped_ptr<ScreenAvailabilityMojoCallback> callback_ptr_;
    scoped_ptr<bool> available_ptr_;
  };

  // Map from presentation URL to its ScreenAvailabilityContext state machine.
  base::hash_map<std::string, linked_ptr<ScreenAvailabilityContext>>
      availability_contexts_;

  std::string default_presentation_url_;

  DISALLOW_COPY_AND_ASSIGN(PresentationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_SERVICE_IMPL_H_
