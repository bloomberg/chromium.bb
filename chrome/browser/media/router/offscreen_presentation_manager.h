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

// TODO(zhaobin): move these back to
// content/public/browser/presentation_service_delegate.h when they are actually
// used in content.
namespace content {
// TODO(zhaobin): A class stub for blink::mojom::PresentationConnectionPtr.
// presentation.mojom is under security review. Change this to
// blink::mojom::PresentationConnectionPtr once that is done.
class PresentationConnectionPtr {};

using ReceiverConnectionAvailableCallback =
    base::Callback<void(const content::PresentationSessionInfo&,
                        PresentationConnectionPtr)>;
}  // namespace content

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
// Controller frame establishes connection with the receiver side, resulting
// in a connection with the two endpoints being the controller
// PresentationConnectionPtr and receiver PresentationConnectionPtr.
// Note calling this will trigger receiver frame's
// PresentationServiceImpl::OnReceiverConnectionAvailable.
//
//   manager->RegisterOffscreenPresentationController(
//       presentation_id, controller_frame_id, controller_ptr);
//
// Invoked on receiver's PresentationServiceImpl when controller connection is
// established.
//
//   |presentation_receiver_client_|: blink::mojom::PresentationServiceClienPtr
//   for the presentation receiver.
//   |PresentationConnectionPtr|: blink::mojom::PresentationConnectionPtr for
//   blink::PresentationConnection object in render process.
//   void PresentationServiceImpl::OnReceiverConnectionAvailable(
//       const content::PresentationSessionInfo& session,
//       PresentationConnectionPtr&& controller) {
//     presentation_receiver_client_->OnReceiverConnectionAvailable(
//         blink::mojom::PresentationSessionInfo::From(session_info),
//         std::move(controller));
//   }
//
// Send message from controller/receiver to receiver/controller:
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
// A controller or receiver leaves the offscreen presentation (e.g.,
// due to navigation) by unregistering themselves from
// OffscreenPresentation object.
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

  // Registers controller PresentationConnectionPtr to presentation
  // with |presentation_id|, |render_frame_id|.
  // Creates a new presentation if no presentation with |presentation_id|
  // exists.
  // |controller|: Not owned by this class. Ownership is transferred to the
  //               presentation receiver via |receiver_callback| passed below.
  void RegisterOffscreenPresentationController(
      const std::string& presentation_id,
      const GURL& presentation_url,
      const RenderFrameHostId& render_frame_id,
      content::PresentationConnectionPtr controller);

  // Unregisters controller PresentationConnectionPtr to presentation with
  // |presentation_id|, |render_frame_id|. It does nothing if there is no
  // controller that matches the provided arguments. It removes presentation
  // that matches the arguments if the presentation has no receiver_callback and
  // any other pending controller.
  void UnregisterOffscreenPresentationController(
      const std::string& presentation_id,
      const RenderFrameHostId& render_frame_id);

  // Registers ReceiverConnectionAvailableCallback to presentation
  // with |presentation_id|.
  void OnOffscreenPresentationReceiverCreated(
      const std::string& presentation_id,
      const GURL& presentation_url,
      const content::ReceiverConnectionAvailableCallback& receiver_callback);

  // Unregisters the ReceiverConnectionAvailableCallback associated with
  // |presentation_id|.
  void OnOffscreenPresentationReceiverTerminated(
      const std::string& presentation_id);

 private:
  // Represents an offscreen presentation registered with
  // OffscreenPresentationManager.
  // Contains callback to the receiver to inform it of new connections
  // established from a controller.
  // Contains set of controllers registered to OffscreenPresentationManager
  // before corresponding receiver.
  class OffscreenPresentation {
   public:
    OffscreenPresentation(const std::string& presentation_id,
                          const GURL& presentation_url);
    ~OffscreenPresentation();

    // Register |controller| with |render_frame_id|. If |receiver_callback_| has
    // been set, invoke |receiver_callback_| with |controller| as parameter,
    // else store |controller| in |pending_controllers_| map.
    void RegisterController(const RenderFrameHostId& render_frame_id,
                            content::PresentationConnectionPtr controller);

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

    // Contains Mojo pointers to controller PresentationConnections registered
    // via |RegisterController()| before |receiver_callback_| is set.
    std::unordered_map<RenderFrameHostId,
                       content::PresentationConnectionPtr,
                       RenderFrameHostIdHasher>
        pending_controllers_;

    DISALLOW_COPY_AND_ASSIGN(OffscreenPresentation);
  };

 private:
  friend class OffscreenPresentationManagerFactory;
  friend class OffscreenPresentationManagerTest;

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
