// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_service_delegate_impl.h"

#include <string>
#include <utility>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/containers/small_map.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/presentation_media_sinks_observer.h"
#include "chrome/browser/media/router/presentation_session_messages_observer.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/presentation_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::PresentationServiceDelegateImpl);

using content::RenderFrameHost;

namespace media_router {

namespace {

using DelegateObserver = content::PresentationServiceDelegate::Observer;

// Returns the unique identifier for the supplied RenderFrameHost.
RenderFrameHostId GetRenderFrameHostId(RenderFrameHost* render_frame_host) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();
  return RenderFrameHostId(render_process_id, render_frame_id);
}

// Gets the last committed URL for the render frame specified by
// |render_frame_host_id|.
GURL GetLastCommittedURLForFrame(RenderFrameHostId render_frame_host_id) {
  RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
      render_frame_host_id.first, render_frame_host_id.second);
  DCHECK(render_frame_host);
  return render_frame_host->GetLastCommittedURL();
}

}  // namespace

// Used by PresentationServiceDelegateImpl to manage
// listeners and default presentation info in a render frame.
// Its lifetime:
//  * PresentationFrameManager AddDelegateObserver
//  * Reset 0+ times.
//  * PresentationFrameManager.RemoveDelegateObserver.
class PresentationFrame {
 public:
  PresentationFrame(content::WebContents* web_contents, MediaRouter* router);
  ~PresentationFrame();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool RemoveScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool HasScreenAvailabilityListenerForTest(
      const MediaSource::Id& source_id) const;
  std::string GetDefaultPresentationId() const;
  void ListenForConnectionStateChange(
      const content::PresentationSessionInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);
  void ListenForSessionMessages(
      const content::PresentationSessionInfo& session,
      const content::PresentationSessionMessageCallback& message_cb);

  void Reset();
  void RemoveConnection(const std::string& presentation_id,
                        const MediaRoute::Id& route_id);

  const MediaRoute::Id GetRouteId(const std::string& presentation_id) const;
  const std::vector<MediaRoute::Id> GetRouteIds() const;

  void OnPresentationSessionStarted(
      const content::PresentationSessionInfo& session,
      const MediaRoute::Id& route_id);
  void OnPresentationServiceDelegateDestroyed() const;

  void set_delegate_observer(DelegateObserver* observer) {
    delegate_observer_ = observer;
  }

 private:
  MediaSource GetMediaSourceFromListener(
      content::PresentationScreenAvailabilityListener* listener) const;
  base::SmallMap<std::map<std::string, MediaRoute::Id>>
      presentation_id_to_route_id_;
  scoped_ptr<PresentationMediaSinksObserver> sinks_observer_;
  base::ScopedPtrHashMap<MediaRoute::Id,
                         scoped_ptr<PresentationConnectionStateSubscription>>
      connection_state_subscriptions_;
  ScopedVector<PresentationSessionMessagesObserver> session_messages_observers_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  const content::WebContents* web_contents_;
  MediaRouter* router_;

  DelegateObserver* delegate_observer_;
};

PresentationFrame::PresentationFrame(content::WebContents* web_contents,
                                     MediaRouter* router)
    : web_contents_(web_contents),
      router_(router),
      delegate_observer_(nullptr) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrame::~PresentationFrame() {
}

void PresentationFrame::OnPresentationServiceDelegateDestroyed() const {
  if (delegate_observer_)
    delegate_observer_->OnDelegateDestroyed();
}

void PresentationFrame::OnPresentationSessionStarted(
    const content::PresentationSessionInfo& session,
    const MediaRoute::Id& route_id) {
  presentation_id_to_route_id_[session.presentation_id] = route_id;
}

const MediaRoute::Id PresentationFrame::GetRouteId(
    const std::string& presentation_id) const {
  auto it = presentation_id_to_route_id_.find(presentation_id);
  return it != presentation_id_to_route_id_.end() ? it->second : "";
}

const std::vector<MediaRoute::Id> PresentationFrame::GetRouteIds() const {
  std::vector<MediaRoute::Id> route_ids;
  for (const auto& e : presentation_id_to_route_id_)
    route_ids.push_back(e.second);
  return route_ids;
}

bool PresentationFrame::SetScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  if (sinks_observer_ && sinks_observer_->listener() == listener)
    return false;

  MediaSource source(GetMediaSourceFromListener(listener));
  sinks_observer_.reset(
      new PresentationMediaSinksObserver(router_, listener, source));

  if (!sinks_observer_->Init()) {
    sinks_observer_.reset();
    listener->OnScreenAvailabilityNotSupported();
    return false;
  }

  return true;
}

bool PresentationFrame::RemoveScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  if (sinks_observer_ && sinks_observer_->listener() == listener) {
    sinks_observer_.reset();
    return true;
  }
  return false;
}

bool PresentationFrame::HasScreenAvailabilityListenerForTest(
    const MediaSource::Id& source_id) const {
  return sinks_observer_ && sinks_observer_->source().id() == source_id;
}

void PresentationFrame::Reset() {
  for (const auto& pid_route_id : presentation_id_to_route_id_)
    router_->DetachRoute(pid_route_id.second);

  presentation_id_to_route_id_.clear();
  sinks_observer_.reset();
  connection_state_subscriptions_.clear();
  session_messages_observers_.clear();
}

void PresentationFrame::RemoveConnection(const std::string& presentation_id,
                                         const MediaRoute::Id& route_id) {
  // Remove the presentation id mapping so a later call to Reset is a no-op.
  presentation_id_to_route_id_.erase(presentation_id);

  // We no longer need to observe route messages.
  auto observer_iter = std::find_if(
      session_messages_observers_.begin(), session_messages_observers_.end(),
      [&route_id](const PresentationSessionMessagesObserver* observer) {
        return route_id == observer->route_id();
      });
  if (observer_iter != session_messages_observers_.end())
    session_messages_observers_.erase(observer_iter);

  // We keep the PresentationConnectionStateChangedCallback registered with MR
  // so the MRP can tell us when terminate() completed.
}

void PresentationFrame::ListenForConnectionStateChange(
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  auto it = presentation_id_to_route_id_.find(connection.presentation_id);
  if (it == presentation_id_to_route_id_.end()) {
    DLOG(ERROR) << __FUNCTION__ << "route id not found for presentation: "
                << connection.presentation_id;
    return;
  }

  const MediaRoute::Id& route_id = it->second;
  if (connection_state_subscriptions_.contains(route_id)) {
    DLOG(ERROR) << __FUNCTION__ << "Already listening connection state change "
                                   "for route: "
                << route_id;
    return;
  }

  connection_state_subscriptions_.add(
      route_id, router_->AddPresentationConnectionStateChangedCallback(
                    it->second, state_changed_cb));
}

void PresentationFrame::ListenForSessionMessages(
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionMessageCallback& message_cb) {
  auto it = presentation_id_to_route_id_.find(session.presentation_id);
  if (it == presentation_id_to_route_id_.end()) {
    DVLOG(2) << "ListenForSessionMessages: no route for "
             << session.presentation_id;
    return;
  }

  session_messages_observers_.push_back(
      new PresentationSessionMessagesObserver(message_cb, it->second, router_));
}

MediaSource PresentationFrame::GetMediaSourceFromListener(
    content::PresentationScreenAvailabilityListener* listener) const {
  // If the default presentation URL is empty then fall back to tab mirroring.
  std::string availability_url(listener->GetAvailabilityUrl());
  return availability_url.empty()
             ? MediaSourceForTab(SessionTabHelper::IdForTab(web_contents_))
             : MediaSourceForPresentationUrl(availability_url);
}

// Used by PresentationServiceDelegateImpl to manage PresentationFrames.
class PresentationFrameManager {
 public:
  PresentationFrameManager(content::WebContents* web_contents,
                           MediaRouter* router);
  ~PresentationFrameManager();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      const RenderFrameHostId& render_frame_host_id,
      content::PresentationScreenAvailabilityListener* listener);
  bool RemoveScreenAvailabilityListener(
      const RenderFrameHostId& render_frame_host_id,
      content::PresentationScreenAvailabilityListener* listener);
  void ListenForConnectionStateChange(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);
  void ListenForSessionMessages(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      const content::PresentationSessionMessageCallback& message_cb);

  // Sets or clears the default presentation request and callback for the given
  // frame. Also sets / clears the default presentation requests for the owning
  // tab WebContents.
  void SetDefaultPresentationUrl(
      const RenderFrameHostId& render_frame_host_id,
      const std::string& default_presentation_url,
      const content::PresentationSessionStartedCallback& callback);
  void AddDelegateObserver(const RenderFrameHostId& render_frame_host_id,
                           DelegateObserver* observer);
  void RemoveDelegateObserver(const RenderFrameHostId& render_frame_host_id);
  void AddDefaultPresentationRequestObserver(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
          observer);
  void RemoveDefaultPresentationRequestObserver(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
          observer);
  void Reset(const RenderFrameHostId& render_frame_host_id);
  void RemoveConnection(const RenderFrameHostId& render_frame_host_id,
                        const MediaRoute::Id& route_id,
                        const std::string& presentation_id);
  bool HasScreenAvailabilityListenerForTest(
      const RenderFrameHostId& render_frame_host_id,
      const MediaSource::Id& source_id) const;
  void SetMediaRouterForTest(MediaRouter* router);

  void OnPresentationSessionStarted(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      const MediaRoute::Id& route_id);
  void OnDefaultPresentationSessionStarted(
      const PresentationRequest& request,
      const content::PresentationSessionInfo& session,
      const MediaRoute::Id& route_id);

  const MediaRoute::Id GetRouteId(const RenderFrameHostId& render_frame_host_id,
                                  const std::string& presentation_id) const;
  const std::vector<MediaRoute::Id> GetRouteIds(
      const RenderFrameHostId& render_frame_host_id) const;

  const PresentationRequest* default_presentation_request() const {
    return default_presentation_request_.get();
  }

 private:
  PresentationFrame* GetOrAddPresentationFrame(
      const RenderFrameHostId& render_frame_host_id);

  // Sets the default presentation request for the owning WebContents and
  // notifies observers of changes.
  void SetDefaultPresentationRequest(
      const PresentationRequest& default_presentation_request);

  // Clears the default presentation request for the owning WebContents and
  // notifies observers of changes. Also resets
  // |default_presentation_started_callback_|.
  void ClearDefaultPresentationRequest();

  bool IsMainFrame(const RenderFrameHostId& render_frame_host_id) const;

  // Maps a frame identifier to a PresentationFrame object for frames
  // that are using presentation API.
  base::ScopedPtrHashMap<RenderFrameHostId, scoped_ptr<PresentationFrame>>
      presentation_frames_;

  // Default presentation request for the owning tab WebContents.
  scoped_ptr<PresentationRequest> default_presentation_request_;

  // Callback to invoke when default presentation has started.
  content::PresentationSessionStartedCallback
      default_presentation_started_callback_;

  // References to the observers listening for changes to this tab WebContent's
  // default presentation.
  base::ObserverList<
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver>
      default_presentation_request_observers_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  MediaRouter* router_;
  content::WebContents* web_contents_;
};

PresentationFrameManager::PresentationFrameManager(
    content::WebContents* web_contents,
    MediaRouter* router)
    : router_(router), web_contents_(web_contents) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrameManager::~PresentationFrameManager() {
  for (auto& frame : presentation_frames_)
    frame.second->OnPresentationServiceDelegateDestroyed();
}

void PresentationFrameManager::OnPresentationSessionStarted(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    const MediaRoute::Id& route_id) {
  auto presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->OnPresentationSessionStarted(session, route_id);
}

void PresentationFrameManager::OnDefaultPresentationSessionStarted(
    const PresentationRequest& request,
    const content::PresentationSessionInfo& session,
    const MediaRoute::Id& route_id) {
  auto presentation_frame =
      presentation_frames_.get(request.render_frame_host_id());
  if (presentation_frame)
    presentation_frame->OnPresentationSessionStarted(session, route_id);

  if (default_presentation_request_ &&
      default_presentation_request_->Equals(request)) {
    default_presentation_started_callback_.Run(session);
  }
}

const MediaRoute::Id PresentationFrameManager::GetRouteId(
    const RenderFrameHostId& render_frame_host_id,
    const std::string& presentation_id) const {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  return presentation_frame ? presentation_frame->GetRouteId(presentation_id)
                            : "";
}

const std::vector<MediaRoute::Id> PresentationFrameManager::GetRouteIds(
    const RenderFrameHostId& render_frame_host_id) const {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  return presentation_frame ? presentation_frame->GetRouteIds()
                            : std::vector<MediaRoute::Id>();
}

bool PresentationFrameManager::SetScreenAvailabilityListener(
    const RenderFrameHostId& render_frame_host_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  auto presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  return presentation_frame->SetScreenAvailabilityListener(listener);
}

bool PresentationFrameManager::RemoveScreenAvailabilityListener(
    const RenderFrameHostId& render_frame_host_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  return presentation_frame &&
         presentation_frame->RemoveScreenAvailabilityListener(listener);
}

bool PresentationFrameManager::HasScreenAvailabilityListenerForTest(
    const RenderFrameHostId& render_frame_host_id,
    const MediaSource::Id& source_id) const {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  return presentation_frame &&
         presentation_frame->HasScreenAvailabilityListenerForTest(source_id);
}

void PresentationFrameManager::ListenForConnectionStateChange(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  if (presentation_frame) {
    presentation_frame->ListenForConnectionStateChange(connection,
                                                       state_changed_cb);
  }
}

void PresentationFrameManager::ListenForSessionMessages(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionMessageCallback& message_cb) {
  PresentationFrame* presentation_frame =
      presentation_frames_.get(render_frame_host_id);
  if (!presentation_frame) {
    DVLOG(2) << "ListenForSessionMessages: PresentationFrame does not exist "
             << "for: (" << render_frame_host_id.first << ", "
             << render_frame_host_id.second << ")";
    return;
  }
  presentation_frame->ListenForSessionMessages(session, message_cb);
}

void PresentationFrameManager::SetDefaultPresentationUrl(
    const RenderFrameHostId& render_frame_host_id,
    const std::string& default_presentation_url,
    const content::PresentationSessionStartedCallback& callback) {
  if (!IsMainFrame(render_frame_host_id))
    return;

  if (default_presentation_url.empty()) {
    ClearDefaultPresentationRequest();
  } else {
    DCHECK(!callback.is_null());
    GURL frame_url(GetLastCommittedURLForFrame(render_frame_host_id));
    PresentationRequest request(render_frame_host_id, default_presentation_url,
                                frame_url);
    default_presentation_started_callback_ = callback;
    SetDefaultPresentationRequest(request);
  }
}

void PresentationFrameManager::AddDelegateObserver(
    const RenderFrameHostId& render_frame_host_id,
    DelegateObserver* observer) {
  auto presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->set_delegate_observer(observer);
}

void PresentationFrameManager::RemoveDelegateObserver(
    const RenderFrameHostId& render_frame_host_id) {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  if (presentation_frame) {
    presentation_frame->set_delegate_observer(nullptr);
    presentation_frames_.erase(render_frame_host_id);
  }
}

void PresentationFrameManager::AddDefaultPresentationRequestObserver(
    PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
        observer) {
  default_presentation_request_observers_.AddObserver(observer);
}

void PresentationFrameManager::RemoveDefaultPresentationRequestObserver(
    PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
        observer) {
  default_presentation_request_observers_.RemoveObserver(observer);
}

void PresentationFrameManager::Reset(
    const RenderFrameHostId& render_frame_host_id) {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  if (presentation_frame)
    presentation_frame->Reset();

  if (default_presentation_request_ &&
      render_frame_host_id ==
          default_presentation_request_->render_frame_host_id()) {
    ClearDefaultPresentationRequest();
  }
}

void PresentationFrameManager::RemoveConnection(
    const RenderFrameHostId& render_frame_host_id,
    const MediaRoute::Id& route_id,
    const std::string& presentation_id) {
  auto presentation_frame = presentation_frames_.get(render_frame_host_id);
  if (presentation_frame)
    presentation_frame->RemoveConnection(route_id, presentation_id);
}

PresentationFrame* PresentationFrameManager::GetOrAddPresentationFrame(
    const RenderFrameHostId& render_frame_host_id) {
  if (!presentation_frames_.contains(render_frame_host_id)) {
    presentation_frames_.add(
        render_frame_host_id,
        scoped_ptr<PresentationFrame>(
            new PresentationFrame(web_contents_, router_)));
  }
  return presentation_frames_.get(render_frame_host_id);
}

void PresentationFrameManager::ClearDefaultPresentationRequest() {
  default_presentation_started_callback_.Reset();
  if (!default_presentation_request_)
    return;

  default_presentation_request_.reset();
  FOR_EACH_OBSERVER(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver,
      default_presentation_request_observers_, OnDefaultPresentationRemoved());
}

bool PresentationFrameManager::IsMainFrame(
    const RenderFrameHostId& render_frame_host_id) const {
  RenderFrameHost* main_frame = web_contents_->GetMainFrame();
  return main_frame && GetRenderFrameHostId(main_frame) == render_frame_host_id;
}

void PresentationFrameManager::SetDefaultPresentationRequest(
    const PresentationRequest& default_presentation_request) {
  if (default_presentation_request_ &&
      default_presentation_request_->Equals(default_presentation_request))
    return;

  default_presentation_request_.reset(
      new PresentationRequest(default_presentation_request));
  FOR_EACH_OBSERVER(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver,
      default_presentation_request_observers_,
      OnDefaultPresentationChanged(*default_presentation_request_));
}

void PresentationFrameManager::SetMediaRouterForTest(MediaRouter* router) {
  router_ = router;
}

PresentationServiceDelegateImpl*
PresentationServiceDelegateImpl::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  PresentationServiceDelegateImpl::CreateForWebContents(web_contents);
  return PresentationServiceDelegateImpl::FromWebContents(web_contents);
}

PresentationServiceDelegateImpl::PresentationServiceDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      router_(MediaRouterFactory::GetApiForBrowserContext(
          web_contents_->GetBrowserContext())),
      frame_manager_(new PresentationFrameManager(web_contents, router_)),
      weak_factory_(this) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationServiceDelegateImpl::~PresentationServiceDelegateImpl() {
}

void PresentationServiceDelegateImpl::AddObserver(int render_process_id,
                                                  int render_frame_id,
                                                  DelegateObserver* observer) {
  DCHECK(observer);
  frame_manager_->AddDelegateObserver(
      RenderFrameHostId(render_process_id, render_frame_id), observer);
}

void PresentationServiceDelegateImpl::RemoveObserver(int render_process_id,
                                                     int render_frame_id) {
  frame_manager_->RemoveDelegateObserver(
      RenderFrameHostId(render_process_id, render_frame_id));
}

bool PresentationServiceDelegateImpl::AddScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  return frame_manager_->SetScreenAvailabilityListener(
      RenderFrameHostId(render_process_id, render_frame_id), listener);
}

void PresentationServiceDelegateImpl::RemoveScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  frame_manager_->RemoveScreenAvailabilityListener(
      RenderFrameHostId(render_process_id, render_frame_id), listener);
}

void PresentationServiceDelegateImpl::Reset(int render_process_id,
                                            int render_frame_id) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  frame_manager_->Reset(render_frame_host_id);
}

void PresentationServiceDelegateImpl::SetDefaultPresentationUrl(
    int render_process_id,
    int render_frame_id,
    const std::string& default_presentation_url,
    const content::PresentationSessionStartedCallback& callback) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  frame_manager_->SetDefaultPresentationUrl(render_frame_host_id,
                                            default_presentation_url, callback);
}

void PresentationServiceDelegateImpl::OnJoinRouteResponse(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb,
    const RouteRequestResult& result) {
  if (!result.route()) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND, result.error()));
  } else {
    DVLOG(1) << "OnJoinRouteResponse: "
             << "route_id: " << result.route()->media_route_id()
             << ", presentation URL: " << session.presentation_url
             << ", presentation ID: " << session.presentation_id;
    DCHECK_EQ(session.presentation_id, result.presentation_id());
    frame_manager_->OnPresentationSessionStarted(
        RenderFrameHostId(render_process_id, render_frame_id), session,
        result.route()->media_route_id());
    success_cb.Run(session);
  }
}

void PresentationServiceDelegateImpl::OnStartSessionSucceeded(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionInfo& new_session,
    const MediaRoute::Id& route_id) {
  DVLOG(1) << "OnStartSessionSucceeded: "
           << "route_id: " << route_id
           << ", presentation URL: " << new_session.presentation_url
           << ", presentation ID: " << new_session.presentation_id;
  frame_manager_->OnPresentationSessionStarted(
      RenderFrameHostId(render_process_id, render_frame_id), new_session,
      route_id);
  success_cb.Run(new_session);
}

void PresentationServiceDelegateImpl::StartSession(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_url,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb) {
  if (presentation_url.empty() || !IsValidPresentationUrl(presentation_url)) {
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Invalid presentation arguments."));
    return;
  }

  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  scoped_ptr<CreatePresentationConnectionRequest> request(
      new CreatePresentationConnectionRequest(
          render_frame_host_id, presentation_url,
          GetLastCommittedURLForFrame(render_frame_host_id),
          base::Bind(&PresentationServiceDelegateImpl::OnStartSessionSucceeded,
                     weak_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id, success_cb),
          error_cb));
  MediaRouterDialogController* controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents_);
  if (!controller->ShowMediaRouterDialogForPresentation(std::move(request))) {
    LOG(ERROR) << "Media router dialog already exists. Ignoring StartSession.";
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Unable to create dialog."));
    return;
  }
}

void PresentationServiceDelegateImpl::JoinSession(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_url,
    const std::string& presentation_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb) {
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(base::Bind(
      &PresentationServiceDelegateImpl::OnJoinRouteResponse,
      weak_factory_.GetWeakPtr(), render_process_id, render_frame_id,
      content::PresentationSessionInfo(presentation_url, presentation_id),
      success_cb, error_cb));
  router_->JoinRoute(
      MediaSourceForPresentationUrl(presentation_url).id(), presentation_id,
      GetLastCommittedURLForFrame(
          RenderFrameHostId(render_process_id, render_frame_id))
          .GetOrigin(),
      web_contents_, route_response_callbacks, base::TimeDelta());
}

void PresentationServiceDelegateImpl::CloseConnection(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const RenderFrameHostId rfh_id(render_process_id, render_frame_id);
  const MediaRoute::Id& route_id =
      frame_manager_->GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }

  router_->DetachRoute(route_id);
  frame_manager_->RemoveConnection(rfh_id, presentation_id, route_id);
  // TODO(mfoltz): close() should always succeed so there is no need to keep the
  // state_changed_cb around - remove it and fire the ChangeEvent on the
  // PresentationConnection in Blink.
}

void PresentationServiceDelegateImpl::Terminate(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const RenderFrameHostId rfh_id(render_process_id, render_frame_id);
  const MediaRoute::Id& route_id =
      frame_manager_->GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }
  router_->TerminateRoute(route_id);
  frame_manager_->RemoveConnection(rfh_id, presentation_id, route_id);
}

void PresentationServiceDelegateImpl::ListenForSessionMessages(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationSessionMessageCallback& message_cb) {
  frame_manager_->ListenForSessionMessages(
      RenderFrameHostId(render_process_id, render_frame_id), session,
      message_cb);
}

void PresentationServiceDelegateImpl::SendMessage(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    scoped_ptr<content::PresentationSessionMessage> message,
    const SendMessageCallback& send_message_cb) {
  const MediaRoute::Id& route_id = frame_manager_->GetRouteId(
      RenderFrameHostId(render_process_id, render_frame_id),
      session.presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for  " << session.presentation_id;
    send_message_cb.Run(false);
    return;
  }

  if (message->is_binary()) {
    router_->SendRouteBinaryMessage(route_id, std::move(message->data),
                                    send_message_cb);
  } else {
    router_->SendRouteMessage(route_id, message->message, send_message_cb);
  }
}

void PresentationServiceDelegateImpl::ListenForConnectionStateChange(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  frame_manager_->ListenForConnectionStateChange(
      RenderFrameHostId(render_process_id, render_frame_id), connection,
      state_changed_cb);
}

void PresentationServiceDelegateImpl::OnRouteResponse(
    const PresentationRequest& presentation_request,
    const RouteRequestResult& result) {
  if (!result.route())
    return;

  content::PresentationSessionInfo session_info(
      presentation_request.presentation_url(), result.presentation_id());
  frame_manager_->OnDefaultPresentationSessionStarted(
      presentation_request, session_info, result.route()->media_route_id());
}

void PresentationServiceDelegateImpl::AddDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  frame_manager_->AddDefaultPresentationRequestObserver(observer);
}

void PresentationServiceDelegateImpl::RemoveDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  frame_manager_->RemoveDefaultPresentationRequestObserver(observer);
}

PresentationRequest
PresentationServiceDelegateImpl::GetDefaultPresentationRequest() const {
  DCHECK(HasDefaultPresentationRequest());
  return *frame_manager_->default_presentation_request();
}

bool PresentationServiceDelegateImpl::HasDefaultPresentationRequest() const {
  return frame_manager_->default_presentation_request() != nullptr;
}

base::WeakPtr<PresentationServiceDelegateImpl>
PresentationServiceDelegateImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PresentationServiceDelegateImpl::SetMediaRouterForTest(
    MediaRouter* router) {
  router_ = router;
  frame_manager_->SetMediaRouterForTest(router);
}

bool PresentationServiceDelegateImpl::HasScreenAvailabilityListenerForTest(
    int render_process_id,
    int render_frame_id,
    const MediaSource::Id& source_id) const {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  return frame_manager_->HasScreenAvailabilityListenerForTest(
      render_frame_host_id, source_id);
}

}  // namespace media_router
