// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_OFFSCREEN_PRESENTATION_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_OFFSCREEN_PRESENTATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/router/render_frame_host_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/presentation_service_delegate.h"

class GURL;

namespace media_router {
// Manages all offscreen presentations started in the associated Profile and
// facilitates communication between the controllers and the receiver of an
// offscreen presentation.
//
// Design doc:
// https://docs.google.com/document/d/1XM3jhMJTQyhEC5PDAAJFNIaKh6UUEihqZDz_ztEe4Co/edit#heading=h.hadpx5oi0gml
//
// Example usage:
//
// Receiver is created to host the offscreen presentation and registers itself
// so that controller frames can connect to it:
//
//   OffscreenPresentationManager* manager =
//       OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext(
//           web_contents_->GetBrowserContext());
//   manager->OnOffscreenPresentationReceiverCreated(presentation_id,
//       base::Bind(&PresentationServiceImpl::OnReceiverConnectionAvailable));
//
// Controlling frame establishes connection with the receiver side, resulting
// in a connection with the two endpoints being the controller
// PresentationConnectionPtr and receiver PresentationConnectionPtr.
// Note calling this will trigger receiver frame's
// PresentationServiceImpl::OnReceiverConnectionAvailable.
//
//   manager->RegisterOffscreenPresentationController(
//       presentation_id, controller_frame_id, controller_connection_ptr,
//       receiver_connection_request);
//
// Invoked on receiver's PresentationServiceImpl when controller connection is
// established.
//
//   |presentation_receiver_client_|: blink::mojom::PresentationServiceClienPtr
//   for the presentation receiver.
//   |controller_connection_ptr|: blink::mojom::PresentationConnectionPtr for
//   blink::PresentationConnection object in controlling frame's render process.
//   |receiver_connection_request|: Mojo InterfaceRequest to be bind to
//   blink::PresentationConnection object in receiver frame's render process.
//   void PresentationServiceImpl::OnReceiverConnectionAvailable(
//       const content::PresentationSessionInfo& session,
//       PresentationConnectionPtr controller_connection_ptr,
//       PresentationConnectionRequest receiver_connection_request) {
//     presentation_receiver_client_->OnReceiverConnectionAvailable(
//         blink::mojom::PresentationSessionInfo::From(session_info),
//         std::move(controller_connection_ptr),
//         std::move(receiver_connection_request));
//   }
//
// Send message from controlling/receiver frame to receiver/controlling frame:
//
//   |target_connection_|: member variable of
//                         blink::mojom::PresentationConnectionPtr type,
//                         refering to remote PresentationConnectionProxy
//                         object on receiver/controlling frame.
//   |message|: Text message to be sent.
//   PresentationConnctionPtr::SendString(
//       const blink::WebString& message) {
//     target_connection_->OnSessionMessageReceived(std::move(session_message));
//   }
//
// A controller or receiver leaves the offscreen presentation (e.g., due to
// navigation) by unregistering themselves from OffscreenPresentation object.
//
// When the receiver is no longer associated with an offscreen presentation, it
// shall unregister itself with OffscreenPresentationManager. Unregistration
// will prevent additional controllers from establishing a connection with the
// receiver:
//
//   In receiver's PSImpl::Reset() {
//     offscreen_presentation_manager->
//         OnOffscreenPresentationReceiverTerminated(presentation_id);
//   }
//
// This class is not thread safe. All functions must be invoked on the UI
// thread. All callbacks passed into this class will also be invoked on UI
// thread.
class OffscreenPresentationManager : public KeyedService {
 public:
  ~OffscreenPresentationManager() override;

  // Registers controller PresentationConnectionPtr to presentation with
  // |presentation_id| and |render_frame_id|.
  // Creates a new presentation if no presentation with |presentation_id|
  // exists.
  // |controller_connection_ptr|, |receiver_connection_request|: Not owned by
  // this class. Ownership is transferred to presentation receiver via
  // |receiver_callback| passed below.
  virtual void RegisterOffscreenPresentationController(
      const std::string& presentation_id,
      const GURL& presentation_url,
      const RenderFrameHostId& render_frame_id,
      content::PresentationConnectionPtr controller_connection_ptr,
      content::PresentationConnectionRequest receiver_connection_request);

  // Unregisters controller PresentationConnectionPtr to presentation with
  // |presentation_id|, |render_frame_id|. It does nothing if there is no
  // controller that matches the provided arguments. It removes presentation
  // that matches the arguments if the presentation has no |receiver_callback|
  // and any other pending controller.
  virtual void UnregisterOffscreenPresentationController(
      const std::string& presentation_id,
      const RenderFrameHostId& render_frame_id);

  // Registers |receiver_callback| to presentation with |presentation_id| and
  // |presentation_url|.
  virtual void OnOffscreenPresentationReceiverCreated(
      const std::string& presentation_id,
      const GURL& presentation_url,
      const content::ReceiverConnectionAvailableCallback& receiver_callback);

  // Unregisters ReceiverConnectionAvailableCallback associated with
  // |presentation_id|.
  virtual void OnOffscreenPresentationReceiverTerminated(
      const std::string& presentation_id);

 private:
  // Represents an offscreen presentation registered with
  // OffscreenPresentationManager. Contains callback to the receiver to inform
  // it of new connections established from a controller. Contains set of
  // controllers registered to OffscreenPresentationManager before corresponding
  // receiver.
  class OffscreenPresentation {
   public:
    OffscreenPresentation(const std::string& presentation_id,
                          const GURL& presentation_url);
    ~OffscreenPresentation();

    // Register controller with |render_frame_id|. If |receiver_callback_| has
    // been set, invoke |receiver_callback_| with |controller_connection_ptr|
    // and |receiver_connection_request| as parameter, else creates a
    // ControllerConnection object with |controller_connection_ptr| and
    // |receiver_connection_request|, and store it in |pending_controllers_|
    // map.
    void RegisterController(
        const RenderFrameHostId& render_frame_id,
        content::PresentationConnectionPtr controller_connection_ptr,
        content::PresentationConnectionRequest receiver_connection_request);

    // Unregister controller with |render_frame_id|. Do nothing if there is no
    // pending controller with |render_frame_id|.
    void UnregisterController(const RenderFrameHostId& render_frame_id);

    // Register |receiver_callback| to current offscreen_presentation object.
    // For each controller in |pending_controllers_| map, invoke
    // |receiver_callback| with controller as parameter. Clear
    // |pending_controllers_| map afterwards.
    void RegisterReceiver(
        const content::ReceiverConnectionAvailableCallback& receiver_callback);

   private:
    friend class OffscreenPresentationManagerTest;
    friend class OffscreenPresentationManager;

    // Returns false if receiver_callback_ is null and there are no pending
    // controllers.
    bool IsValid() const;

    const std::string presentation_id_;
    const GURL presentation_url_;

    // Callback to invoke whenever a receiver connection is available.
    content::ReceiverConnectionAvailableCallback receiver_callback_;

    // Stores controller information.
    // |controller_connection_ptr|: Mojo::InterfacePtr to
    // blink::PresentationConnection object in controlling frame;
    // |receiver_connection_request|: Mojo::InterfaceRequest to be bind to
    // blink::PresentationConnection object in receiver frame.
    struct ControllerConnection {
     public:
      ControllerConnection(
          content::PresentationConnectionPtr controller_connection_ptr,
          content::PresentationConnectionRequest receiver_connection_request);
      ~ControllerConnection();

      content::PresentationConnectionPtr controller_connection_ptr;
      content::PresentationConnectionRequest receiver_connection_request;
    };

    // Contains ControllerConnection objects registered via
    // |RegisterController()| before |receiver_callback_| is set.
    std::unordered_map<RenderFrameHostId,
                       std::unique_ptr<ControllerConnection>,
                       RenderFrameHostIdHasher>
        pending_controllers_;

    DISALLOW_COPY_AND_ASSIGN(OffscreenPresentation);
  };

 private:
  friend class OffscreenPresentationManagerFactory;
  friend class OffscreenPresentationManagerTest;
  friend class MockOffscreenPresentationManager;
  FRIEND_TEST_ALL_PREFIXES(PresentationServiceDelegateImplTest,
                           ConnectToOffscreenPresentation);

  // Used by OffscreenPresentationManagerFactory::GetOrCreateForBrowserContext.
  OffscreenPresentationManager();

  using OffscreenPresentationMap =
      std::map<std::string, std::unique_ptr<OffscreenPresentation>>;

  // Creates an offscreen presentation with |presentation_id| and
  // |presentation_url|.
  OffscreenPresentation* GetOrCreateOffscreenPresentation(
      const std::string& presentation_id,
      const GURL& presentation_url);

  // Maps from presentation ID to OffscreenPresentation.
  OffscreenPresentationMap offscreen_presentations_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(OffscreenPresentationManager);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_OFFSCREEN_PRESENTATION_MANAGER_H_
