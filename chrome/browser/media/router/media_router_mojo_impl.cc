// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_mojo_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/local_media_routes_observer.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/media/router/media_router_type_converters.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/presentation_session_messages_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "extensions/browser/process_manager.h"

#define DVLOG_WITH_INSTANCE(level) \
  DVLOG(level) << "MR #" << instance_id_ << ": "

#define DLOG_WITH_INSTANCE(level) DLOG(level) << "MR #" << instance_id_ << ": "

namespace media_router {

using SinkAvailability = interfaces::MediaRouter::SinkAvailability;

namespace {

scoped_ptr<content::PresentationSessionMessage>
ConvertToPresentationSessionMessage(interfaces::RouteMessagePtr input) {
  DCHECK(!input.is_null());
  scoped_ptr<content::PresentationSessionMessage> output;
  switch (input->type) {
    case interfaces::RouteMessage::Type::TEXT: {
      DCHECK(!input->message.is_null());
      DCHECK(input->data.is_null());
      output.reset(new content::PresentationSessionMessage(
          content::PresentationMessageType::TEXT));
      input->message.Swap(&output->message);
      return output;
    }
    case interfaces::RouteMessage::Type::BINARY: {
      DCHECK(!input->data.is_null());
      DCHECK(input->message.is_null());
      output.reset(new content::PresentationSessionMessage(
          content::PresentationMessageType::ARRAY_BUFFER));
      output->data.reset(new std::vector<uint8_t>);
      input->data.Swap(output->data.get());
      return output;
    }
  }

  NOTREACHED() << "Invalid route message type " << input->type;
  return output;
}

}  // namespace

MediaRouterMojoImpl::MediaRouterMediaRoutesObserver::
MediaRouterMediaRoutesObserver(MediaRouterMojoImpl* router)
    : MediaRoutesObserver(router),
      router_(router) {
  DCHECK(router);
}

MediaRouterMojoImpl::MediaRouterMediaRoutesObserver::
~MediaRouterMediaRoutesObserver() {
}

MediaRouterMojoImpl::MediaRoutesQuery::MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaRoutesQuery::~MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::~MediaSinksQuery() = default;

void MediaRouterMojoImpl::MediaRouterMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  bool has_local_display_route =
      std::find_if(routes.begin(), routes.end(),
                   [](const media_router::MediaRoute& route) {
                     return route.is_local() && route.for_display();
                   }) != routes.end();

  // |this| will be deleted in UpdateHasLocalDisplayRoute() if
  // |has_local_display_route| is false. Note that ObserverList supports
  // removing an observer while iterating through it.
  router_->UpdateHasLocalDisplayRoute(has_local_display_route);
}

MediaRouterMojoImpl::MediaRouterMojoImpl(
    extensions::EventPageTracker* event_page_tracker)
    : event_page_tracker_(event_page_tracker),
      instance_id_(base::GenerateGUID()),
      has_local_display_route_(false),
      availability_(interfaces::MediaRouter::SinkAvailability::UNAVAILABLE),
      wakeup_attempt_count_(0),
      current_wake_reason_(MediaRouteProviderWakeReason::TOTAL_COUNT),
      weak_factory_(this) {
  DCHECK(event_page_tracker_);
}

MediaRouterMojoImpl::~MediaRouterMojoImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Make sure |routes_observer_| is destroyed first, because it triggers
  // additional cleanup logic in this class that depends on other fields.
  routes_observer_.reset();
}

// static
void MediaRouterMojoImpl::BindToRequest(
    const std::string& extension_id,
    content::BrowserContext* context,
    mojo::InterfaceRequest<interfaces::MediaRouter> request) {
  MediaRouterMojoImpl* impl = static_cast<MediaRouterMojoImpl*>(
      MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(impl);

  impl->BindToMojoRequest(std::move(request), extension_id);
}

void MediaRouterMojoImpl::BindToMojoRequest(
    mojo::InterfaceRequest<interfaces::MediaRouter> request,
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  binding_.reset(
      new mojo::Binding<interfaces::MediaRouter>(this, std::move(request)));
  binding_->set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));

  media_route_provider_extension_id_ = extension_id;
}

void MediaRouterMojoImpl::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_route_provider_.reset();
  binding_.reset();

  // If |OnConnectionError| is invoked while there are pending requests, then
  // it means we tried to wake the extension, but weren't able to complete the
  // connection to media route provider. Since we do not know whether the error
  // is transient, reattempt the wakeup.
  if (!pending_requests_.empty()) {
    DLOG_WITH_INSTANCE(ERROR) << "A connection error while there are pending "
                                 "requests.";
    SetWakeReason(MediaRouteProviderWakeReason::CONNECTION_ERROR);
    AttemptWakeEventPage();
  }
}

void MediaRouterMojoImpl::RegisterMediaRouteProvider(
    interfaces::MediaRouteProviderPtr media_route_provider_ptr,
    const interfaces::MediaRouter::RegisterMediaRouteProviderCallback&
        callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (event_page_tracker_->IsEventPageSuspended(
          media_route_provider_extension_id_)) {
    DVLOG_WITH_INSTANCE(1)
        << "ExecutePendingRequests was called while extension is suspended.";
    media_route_provider_.reset();
    SetWakeReason(MediaRouteProviderWakeReason::REGISTER_MEDIA_ROUTE_PROVIDER);
    AttemptWakeEventPage();
    return;
  }

  media_route_provider_ = std::move(media_route_provider_ptr);
  media_route_provider_.set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));
  callback.Run(instance_id_);
  ExecutePendingRequests();
  wakeup_attempt_count_ = 0;
}

void MediaRouterMojoImpl::OnIssue(const interfaces::IssuePtr issue) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG_WITH_INSTANCE(1) << "OnIssue " << issue->title;
  const Issue& issue_converted = issue.To<Issue>();
  issue_manager_.AddIssue(issue_converted);
}

void MediaRouterMojoImpl::OnSinksReceived(
    const mojo::String& media_source,
    mojo::Array<interfaces::MediaSinkPtr> sinks) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG_WITH_INSTANCE(1) << "OnSinksReceived";
  auto it = sinks_queries_.find(media_source);
  if (it == sinks_queries_.end()) {
    DVLOG_WITH_INSTANCE(1) << "Received sink list without MediaSinksQuery.";
    return;
  }

  auto* sinks_query = it->second;
  sinks_query->has_cached_result = true;
  auto& cached_sink_list = sinks_query->cached_sink_list;
  cached_sink_list.clear();
  cached_sink_list.reserve(sinks.size());
  for (size_t i = 0; i < sinks.size(); ++i)
    cached_sink_list.push_back(sinks[i].To<MediaSink>());

  if (!sinks_query->observers.might_have_observers()) {
    DVLOG_WITH_INSTANCE(1)
        << "Received sink list without any active observers: " << media_source;
  } else {
    FOR_EACH_OBSERVER(MediaSinksObserver, sinks_query->observers,
                      OnSinksReceived(cached_sink_list));
  }
}

void MediaRouterMojoImpl::OnRoutesUpdated(
    mojo::Array<interfaces::MediaRoutePtr> routes,
    const mojo::String& media_source,
    mojo::Array<mojo::String> joinable_route_ids) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG_WITH_INSTANCE(1) << "OnRoutesUpdated";
  auto it = routes_queries_.find(media_source);
  if (it == routes_queries_.end() ||
      !(it->second->observers.might_have_observers())) {
    DVLOG_WITH_INSTANCE(1)
        << "Received route list without any active observers: " << media_source;
    return;
  }

  std::vector<MediaRoute> routes_converted;
  routes_converted.reserve(routes.size());

  for (size_t i = 0; i < routes.size(); ++i)
    routes_converted.push_back(routes[i].To<MediaRoute>());

  std::vector<MediaRoute::Id> joinable_routes_converted =
      joinable_route_ids.To<std::vector<std::string>>();

  FOR_EACH_OBSERVER(MediaRoutesObserver, it->second->observers,
                    OnRoutesUpdated(routes_converted,
                                    joinable_routes_converted));
}

void MediaRouterMojoImpl::RouteResponseReceived(
    const std::string& presentation_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    interfaces::MediaRoutePtr media_route,
    const mojo::String& error_text) {
  scoped_ptr<MediaRoute> route;
  std::string actual_presentation_id;
  std::string error;
  if (media_route.is_null()) {
    // An error occurred.
    DCHECK(!error_text.is_null());
    error = !error_text.get().empty() ? error_text.get() : "Unknown error.";
  } else {
    route = media_route.To<scoped_ptr<MediaRoute>>();
    actual_presentation_id = presentation_id;

    if (route->is_local() && route->for_display()) {
      UpdateHasLocalDisplayRoute(true);

      if (!routes_observer_)
        routes_observer_.reset(new MediaRouterMediaRoutesObserver(this));
    }
  }

  for (const MediaRouteResponseCallback& callback : callbacks)
    callback.Run(route.get(), actual_presentation_id, error);
}

void MediaRouterMojoImpl::UpdateHasLocalDisplayRoute(
    bool has_local_display_route) {
  if (has_local_display_route_ == has_local_display_route)
    return;

  has_local_display_route_ = has_local_display_route;

  if (!has_local_display_route_)
    routes_observer_.reset();

  FOR_EACH_OBSERVER(LocalMediaRoutesObserver, local_routes_observers_,
                    OnHasLocalDisplayRouteUpdated(has_local_display_route_));
}

void MediaRouterMojoImpl::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(nullptr, "", "Invalid origin");
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::CREATE_ROUTE);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(
      &MediaRouterMojoImpl::DoCreateRoute, base::Unretained(this), source_id,
      sink_id, origin.is_empty() ? "" : origin.spec(), tab_id, callbacks));
}

void MediaRouterMojoImpl::JoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(nullptr, "", "Invalid origin");
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::JOIN_ROUTE);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoJoinRoute,
                        base::Unretained(this), source_id, presentation_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks));
}

void MediaRouterMojoImpl::ConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(nullptr, "", "Invalid origin");
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::CONNECT_ROUTE_BY_ROUTE_ID);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoConnectRouteByRouteId,
                        base::Unretained(this), source_id, route_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks));
}

void MediaRouterMojoImpl::TerminateRoute(const MediaRoute::Id& route_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetWakeReason(MediaRouteProviderWakeReason::TERMINATE_ROUTE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoTerminateRoute,
                        base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::DetachRoute(const MediaRoute::Id& route_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetWakeReason(MediaRouteProviderWakeReason::DETACH_ROUTE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoDetachRoute,
                        base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::SendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetWakeReason(MediaRouteProviderWakeReason::SEND_SESSION_MESSAGE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSendSessionMessage,
                        base::Unretained(this), route_id, message, callback));
}

void MediaRouterMojoImpl::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    scoped_ptr<std::vector<uint8_t>> data,
    const SendRouteMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetWakeReason(MediaRouteProviderWakeReason::SEND_SESSION_BINARY_MESSAGE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSendSessionBinaryMessage,
                        base::Unretained(this), route_id,
                        base::Passed(std::move(data)), callback));
}

void MediaRouterMojoImpl::AddIssue(const Issue& issue) {
  DCHECK(thread_checker_.CalledOnValidThread());
  issue_manager_.AddIssue(issue);
}

void MediaRouterMojoImpl::ClearIssue(const Issue::Id& issue_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  issue_manager_.ClearIssue(issue_id);
}

bool MediaRouterMojoImpl::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Create an observer list for the media source and add |observer|
  // to it. Fail if |observer| is already registered.
  const std::string& source_id = observer->source().id();
  auto* sinks_query = sinks_queries_.get(source_id);
  bool new_query = false;
  if (!sinks_query) {
    new_query = true;
    sinks_query = new MediaSinksQuery;
    sinks_queries_.add(source_id, make_scoped_ptr(sinks_query));
  } else {
    DCHECK(!sinks_query->observers.HasObserver(observer));
  }

  // If sink availability is UNAVAILABLE, then there is no need to call MRPM.
  // |observer| can be immediately notified with an empty list.
  sinks_query->observers.AddObserver(observer);
  if (availability_ == interfaces::MediaRouter::SinkAvailability::UNAVAILABLE) {
    observer->OnSinksReceived(std::vector<MediaSink>());
  } else {
    // Need to call MRPM to start observing sinks if the query is new.
    if (new_query) {
      SetWakeReason(MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_SINKS);
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaSinks,
                            base::Unretained(this), source_id));
    } else if (sinks_query->has_cached_result) {
      observer->OnSinksReceived(sinks_query->cached_sink_list);
    }
  }
  return true;
}

void MediaRouterMojoImpl::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const MediaSource::Id& source_id = observer->source().id();
  auto* sinks_query = sinks_queries_.get(source_id);
  if (!sinks_query || !sinks_query->observers.HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  sinks_query->observers.RemoveObserver(observer);
  if (!sinks_query->observers.might_have_observers()) {
    // Only ask MRPM to stop observing media sinks if the availability is not
    // UNAVAILABLE.
    // Otherwise, the MRPM would have discarded the queries already.
    if (availability_ !=
        interfaces::MediaRouter::SinkAvailability::UNAVAILABLE) {
      SetWakeReason(MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_SINKS);
      // The |sinks_queries_| entry will be removed in the immediate or deferred
      // |DoStopObservingMediaSinks| call.
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopObservingMediaSinks,
                            base::Unretained(this), source_id));
    } else {
      sinks_queries_.erase(source_id);
    }
  }
}

void MediaRouterMojoImpl::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const MediaSource::Id source_id = observer->source_id();
  auto* routes_query = routes_queries_.get(source_id);
  if (!routes_query) {
    routes_query = new MediaRoutesQuery;
    routes_queries_.add(source_id, make_scoped_ptr(routes_query));
  } else {
    DCHECK(!routes_query->observers.HasObserver(observer));
  }

  routes_query->observers.AddObserver(observer);
  SetWakeReason(MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_ROUTES);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaRoutes,
                        base::Unretained(this), source_id));
}

void MediaRouterMojoImpl::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  const MediaSource::Id source_id = observer->source_id();
  auto* routes_query = routes_queries_.get(source_id);
  if (!routes_query || !routes_query->observers.HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing routes for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  routes_query->observers.RemoveObserver(observer);
  if (!routes_query->observers.might_have_observers()) {
    SetWakeReason(MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_ROUTES);
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopObservingMediaRoutes,
                          base::Unretained(this), source_id));
  }
}

void MediaRouterMojoImpl::RegisterIssuesObserver(IssuesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  issue_manager_.RegisterObserver(observer);
}

void MediaRouterMojoImpl::UnregisterIssuesObserver(IssuesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  issue_manager_.UnregisterObserver(observer);
}

void MediaRouterMojoImpl::RegisterPresentationSessionMessagesObserver(
    PresentationSessionMessagesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observer);
  const MediaRoute::Id& route_id = observer->route_id();
  auto* observer_list = messages_observers_.get(route_id);
  if (!observer_list) {
    observer_list = new base::ObserverList<PresentationSessionMessagesObserver>;
    messages_observers_.add(route_id, make_scoped_ptr(observer_list));
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  bool should_listen = !observer_list->might_have_observers();
  observer_list->AddObserver(observer);
  if (should_listen) {
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoListenForRouteMessages,
                          base::Unretained(this), route_id));
  }
}

void MediaRouterMojoImpl::UnregisterPresentationSessionMessagesObserver(
    PresentationSessionMessagesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observer);

  const MediaRoute::Id& route_id = observer->route_id();
  auto* observer_list = messages_observers_.get(route_id);
  if (!observer_list || !observer_list->HasObserver(observer))
    return;

  observer_list->RemoveObserver(observer);
  if (!observer_list->might_have_observers()) {
    messages_observers_.erase(route_id);
    SetWakeReason(
        MediaRouteProviderWakeReason::STOP_LISTENING_FOR_ROUTE_MESSAGES);
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopListeningForRouteMessages,
                          base::Unretained(this), route_id));
  }
}

void MediaRouterMojoImpl::RegisterLocalMediaRoutesObserver(
    LocalMediaRoutesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observer);

  DCHECK(!local_routes_observers_.HasObserver(observer));
  local_routes_observers_.AddObserver(observer);
}

void MediaRouterMojoImpl::UnregisterLocalMediaRoutesObserver(
    LocalMediaRoutesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(observer);

  if (!local_routes_observers_.HasObserver(observer))
    return;
  local_routes_observers_.RemoveObserver(observer);
}

void MediaRouterMojoImpl::DoCreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  std::string presentation_id("mr_");
  presentation_id += base::GenerateGUID();
  DVLOG_WITH_INSTANCE(1) << "DoCreateRoute " << source_id << "=>" << sink_id
                         << ", presentation ID: " << presentation_id;
  media_route_provider_->CreateRoute(
      source_id, sink_id, presentation_id, origin, tab_id,
      base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
          base::Unretained(this), presentation_id, callbacks));
}

void MediaRouterMojoImpl::DoJoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  DVLOG_WITH_INSTANCE(1) << "DoJoinRoute " << source_id
                         << ", presentation ID: " << presentation_id;
  media_route_provider_->JoinRoute(
      source_id, presentation_id, origin, tab_id,
      base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
          base::Unretained(this), presentation_id, callbacks));
}

void MediaRouterMojoImpl::DoConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  std::string presentation_id("mr_");
  presentation_id += base::GenerateGUID();
  DVLOG_WITH_INSTANCE(1) << "DoConnectRouteByRouteId " << source_id
                         << ", route ID: " << route_id
                         << ", presentation ID: " << presentation_id;
  media_route_provider_->ConnectRouteByRouteId(
      source_id, route_id, presentation_id, origin, tab_id,
      base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
          base::Unretained(this), presentation_id, callbacks));
}

void MediaRouterMojoImpl::DoTerminateRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoTerminateRoute " << route_id;
  media_route_provider_->TerminateRoute(route_id);
}

void MediaRouterMojoImpl::DoDetachRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoDetachRoute " << route_id;
  media_route_provider_->DetachRoute(route_id);
}

void MediaRouterMojoImpl::DoSendSessionMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteMessage " << route_id;
  media_route_provider_->SendRouteMessage(route_id, message, callback);
}

void MediaRouterMojoImpl::DoSendSessionBinaryMessage(
    const MediaRoute::Id& route_id,
    scoped_ptr<std::vector<uint8_t>> data,
    const SendRouteMessageCallback& callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteBinaryMessage " << route_id;
  mojo::Array<uint8_t> mojo_array;
  mojo_array.Swap(data.get());
  media_route_provider_->SendRouteBinaryMessage(route_id, std::move(mojo_array),
                                                callback);
}

void MediaRouterMojoImpl::DoListenForRouteMessages(
    const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "ListenForRouteMessages";
  if (!ContainsValue(route_ids_listening_for_messages_, route_id)) {
    route_ids_listening_for_messages_.insert(route_id);
    media_route_provider_->ListenForRouteMessages(
        route_id, base::Bind(&MediaRouterMojoImpl::OnRouteMessagesReceived,
                             base::Unretained(this), route_id));
  }
}

void MediaRouterMojoImpl::DoStopListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "StopListeningForRouteMessages";

  // No need to erase |route_ids_listening_for_messages_| entry here.
  // It will be removed when there are no more observers by the time
  // |OnRouteMessagesReceived| is invoked.
  media_route_provider_->StopListeningForRouteMessages(route_id);
}

void MediaRouterMojoImpl::OnRouteMessagesReceived(
    const MediaRoute::Id& route_id,
    mojo::Array<interfaces::RouteMessagePtr> messages,
    bool error) {
  DVLOG(1) << "OnRouteMessagesReceived";

  // If |messages| is null, then no more messages will come from this route.
  // We can stop listening.
  if (error) {
    DVLOG(2) << "Encountered error in OnRouteMessagesReceived for " << route_id;
    route_ids_listening_for_messages_.erase(route_id);
    return;
  }

  // Check if there are any observers remaining. If not, the messages
  // can be discarded and we can stop listening for the next batch of messages.
  auto* observer_list = messages_observers_.get(route_id);
  if (!observer_list) {
    route_ids_listening_for_messages_.erase(route_id);
    return;
  }

  // If |messages| is empty, then |StopListeningForRouteMessages| was invoked
  // but we have added back an observer since. Keep listening for more messages,
  // but do not notify observers with empty list.
  if (!messages.storage().empty()) {
    ScopedVector<content::PresentationSessionMessage> session_messages;
    session_messages.reserve(messages.size());
    for (size_t i = 0; i < messages.size(); ++i) {
      session_messages.push_back(
          ConvertToPresentationSessionMessage(std::move(messages[i])));
    }
    base::ObserverList<PresentationSessionMessagesObserver>::Iterator
        observer_it(observer_list);
    bool single_observer =
        observer_it.GetNext() != nullptr && observer_it.GetNext() == nullptr;
    FOR_EACH_OBSERVER(PresentationSessionMessagesObserver, *observer_list,
                      OnMessagesReceived(session_messages, single_observer));
  }

  // Listen for more messages.
  media_route_provider_->ListenForRouteMessages(
      route_id, base::Bind(&MediaRouterMojoImpl::OnRouteMessagesReceived,
                           base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::OnSinkAvailabilityUpdated(
    SinkAvailability availability) {
  if (availability_ == availability)
    return;

  availability_ = availability;
  if (availability_ == interfaces::MediaRouter::SinkAvailability::UNAVAILABLE) {
    // Sinks are no longer available. MRPM has already removed all sink queries.
    for (auto& source_and_query : sinks_queries_) {
      auto* query = source_and_query.second;
      query->is_active = false;
      query->has_cached_result = false;
      query->cached_sink_list.clear();
    }
  } else {
    // Sinks are now available. Tell MRPM to start all sink queries again.
    for (const auto& source_and_query : sinks_queries_) {
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaSinks,
                            base::Unretained(this), source_and_query.first));
    }
  }
}

void MediaRouterMojoImpl::OnPresentationConnectionStateChanged(
    const mojo::String& route_id,
    interfaces::MediaRouter::PresentationConnectionState state) {
  NotifyPresentationConnectionStateChange(
      route_id, mojo::PresentationConnectionStateFromMojo(state));
}

bool MediaRouterMojoImpl::HasLocalDisplayRoute() const {
  return has_local_display_route_;
}

void MediaRouterMojoImpl::DoStartObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaSinks: " << source_id;
  // No need to call MRPM if there are no sinks available.
  if (availability_ == interfaces::MediaRouter::SinkAvailability::UNAVAILABLE)
    return;

  // No need to call MRPM if all observers have been removed in the meantime.
  auto* sinks_query = sinks_queries_.get(source_id);
  if (!sinks_query || !sinks_query->observers.might_have_observers())
    return;

  DVLOG_WITH_INSTANCE(1) << "MRPM.StartObservingMediaSinks: " << source_id;
  media_route_provider_->StartObservingMediaSinks(source_id);
  sinks_query->is_active = true;
}

void MediaRouterMojoImpl::DoStopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaSinks: " << source_id;

  auto* sinks_query = sinks_queries_.get(source_id);
  // No need to call MRPM if observers have been added in the meantime,
  // or StopObservingMediaSinks has already been called.
  if (!sinks_query || !sinks_query->is_active ||
      sinks_query->observers.might_have_observers()) {
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "MRPM.StopObservingMediaSinks: " << source_id;
  media_route_provider_->StopObservingMediaSinks(source_id);
  sinks_queries_.erase(source_id);
}

void MediaRouterMojoImpl::DoStartObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaRoutes";

  // No need to call MRPM if all observers have been removed in the meantime.
  auto* routes_query = routes_queries_.get(source_id);
  if (!routes_query || !routes_query->observers.might_have_observers())
    return;

  DVLOG_WITH_INSTANCE(1) << "MRPM.StartObservingMediaRoutes: " << source_id;
  media_route_provider_->StartObservingMediaRoutes(source_id);
  routes_query->is_active = true;
}

void MediaRouterMojoImpl::DoStopObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaRoutes";

  // No need to call MRPM if observers have been added in the meantime,
  // or StopObservingMediaRoutes has already been called.
  auto* routes_query = routes_queries_.get(source_id);
  if (!routes_query || !routes_query->is_active ||
      routes_query->observers.might_have_observers()) {
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "MRPM.StopObservingMediaRoutes: " << source_id;
  media_route_provider_->StopObservingMediaRoutes(source_id);
  routes_queries_.erase(source_id);
}

void MediaRouterMojoImpl::EnqueueTask(const base::Closure& closure) {
  pending_requests_.push_back(closure);
  if (pending_requests_.size() > kMaxPendingRequests) {
    DLOG_WITH_INSTANCE(ERROR) << "Reached max queue size. Dropping oldest "
                              << "request.";
    pending_requests_.pop_front();
  }
  DVLOG_WITH_INSTANCE(2) << "EnqueueTask (queue-length="
                         << pending_requests_.size() << ")";
}

void MediaRouterMojoImpl::RunOrDefer(const base::Closure& request) {
  DCHECK(event_page_tracker_);

  if (media_route_provider_extension_id_.empty()) {
    DVLOG_WITH_INSTANCE(1) << "Extension ID not known yet.";
    EnqueueTask(request);
  } else if (event_page_tracker_->IsEventPageSuspended(
                 media_route_provider_extension_id_)) {
    DVLOG_WITH_INSTANCE(1) << "Waking event page.";
    EnqueueTask(request);
    AttemptWakeEventPage();
    media_route_provider_.reset();
  } else if (!media_route_provider_) {
    DVLOG_WITH_INSTANCE(1) << "Extension is awake, awaiting ProvideMediaRouter "
                              " to be called.";
    EnqueueTask(request);
  } else {
    request.Run();
  }
}

void MediaRouterMojoImpl::AttemptWakeEventPage() {
  ++wakeup_attempt_count_;
  if (wakeup_attempt_count_ > kMaxWakeupAttemptCount) {
    DLOG_WITH_INSTANCE(ERROR) << "Attempted too many times to wake up event "
                              << "page.";
    DrainPendingRequests();
    wakeup_attempt_count_ = 0;
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "Attempting to wake up event page: attempt "
                         << wakeup_attempt_count_;

  // This return false if the extension is already awake.
  // Callback is bound using WeakPtr because |event_page_tracker_| outlives
  // |this|.
  if (!event_page_tracker_->WakeEventPage(
          media_route_provider_extension_id_,
          base::Bind(&MediaRouterMojoImpl::EventPageWakeComplete,
                     weak_factory_.GetWeakPtr()))) {
    DLOG_WITH_INSTANCE(ERROR) << "Failed to schedule a wakeup for event page.";
  }
}

void MediaRouterMojoImpl::ExecutePendingRequests() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(media_route_provider_);
  DCHECK(event_page_tracker_);
  DCHECK(!media_route_provider_extension_id_.empty());

  for (const auto& next_request : pending_requests_)
    next_request.Run();

  pending_requests_.clear();
}

void MediaRouterMojoImpl::EventPageWakeComplete(bool success) {
  if (success) {
    MediaRouterMetrics::RecordMediaRouteProviderWakeReason(
        current_wake_reason_);
    ClearWakeReason();
    return;
  }

  // This is likely an non-retriable error. Drop the pending requests.
  DLOG_WITH_INSTANCE(ERROR)
      << "An error encountered while waking the event page.";
  ClearWakeReason();
  DrainPendingRequests();
}

void MediaRouterMojoImpl::DrainPendingRequests() {
  DLOG_WITH_INSTANCE(ERROR)
      << "Draining request queue. (queue-length=" << pending_requests_.size()
      << ")";
  pending_requests_.clear();
}

void MediaRouterMojoImpl::SetWakeReason(MediaRouteProviderWakeReason reason) {
  DCHECK(reason != MediaRouteProviderWakeReason::TOTAL_COUNT);
  if (current_wake_reason_ == MediaRouteProviderWakeReason::TOTAL_COUNT)
    current_wake_reason_ = reason;
}

void MediaRouterMojoImpl::ClearWakeReason() {
  DCHECK(current_wake_reason_ != MediaRouteProviderWakeReason::TOTAL_COUNT);
  current_wake_reason_ = MediaRouteProviderWakeReason::TOTAL_COUNT;
}

}  // namespace media_router
