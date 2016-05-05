// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/media_router_type_converters.h"
#include "chrome/browser/media/router/presentation_session_messages_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "extensions/browser/process_manager.h"

#define DVLOG_WITH_INSTANCE(level) \
  DVLOG(level) << "MR #" << instance_id_ << ": "

#define DLOG_WITH_INSTANCE(level) DLOG(level) << "MR #" << instance_id_ << ": "

namespace media_router {

using SinkAvailability = interfaces::MediaRouter::SinkAvailability;

namespace {

std::unique_ptr<content::PresentationSessionMessage>
ConvertToPresentationSessionMessage(interfaces::RouteMessagePtr input) {
  DCHECK(!input.is_null());
  std::unique_ptr<content::PresentationSessionMessage> output;
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

MediaRouterMojoImpl::MediaRoutesQuery::MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaRoutesQuery::~MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::~MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaRouterMojoImpl(
    extensions::EventPageTracker* event_page_tracker)
    : event_page_tracker_(event_page_tracker),
      instance_id_(base::GenerateGUID()),
      availability_(interfaces::MediaRouter::SinkAvailability::UNAVAILABLE),
      current_wake_reason_(MediaRouteProviderWakeReason::TOTAL_COUNT),
      weak_factory_(this) {
  DCHECK(event_page_tracker_);
#if defined(OS_WIN)
  CanFirewallUseLocalPorts(
      base::Bind(&MediaRouterMojoImpl::OnFirewallCheckComplete,
                 weak_factory_.GetWeakPtr()));
#endif
}

MediaRouterMojoImpl::~MediaRouterMojoImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
void MediaRouterMojoImpl::BindToRequest(
    const extensions::Extension* extension,
    content::BrowserContext* context,
    mojo::InterfaceRequest<interfaces::MediaRouter> request) {
  MediaRouterMojoImpl* impl = static_cast<MediaRouterMojoImpl*>(
      MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(impl);

  impl->BindToMojoRequest(std::move(request), *extension);
}

void MediaRouterMojoImpl::BindToMojoRequest(
    mojo::InterfaceRequest<interfaces::MediaRouter> request,
    const extensions::Extension& extension) {
  DCHECK(thread_checker_.CalledOnValidThread());

  binding_.reset(
      new mojo::Binding<interfaces::MediaRouter>(this, std::move(request)));
  binding_->set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));

  media_route_provider_extension_id_ = extension.id();
  if (!provider_version_was_recorded_) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(extension);
    provider_version_was_recorded_ = true;
  }
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

#if defined(OS_WIN)
  // The MRPM may have been upgraded or otherwise reload such that we could be
  // seeing an MRPM that doesn't know mDNS is enabled, even if we've told a
  // previously registered MRPM it should be enabled. Furthermore, there may be
  // a pending request to enable mDNS, so don't clear this flag after
  // ExecutePendingRequests().
  is_mdns_enabled_ = false;
#endif
  if (event_page_tracker_->IsEventPageSuspended(
          media_route_provider_extension_id_)) {
    DVLOG_WITH_INSTANCE(1)
        << "RegisterMediaRouteProvider was called while extension is "
           "suspended.";
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
#if defined(OS_WIN)
  // The MRPM extension already turns on mDNS discovery for platforms other than
  // Windows. It only relies on this signalling from MR on Windows to avoid
  // triggering a firewall prompt out of the context of MR from the user's
  // perspective. This particular call reminds the extension to enable mDNS
  // discovery when it wakes up, has been upgraded, etc.
  if (should_enable_mdns_discovery_) {
    DoEnsureMdnsDiscoveryEnabled();
  }
#endif
}

void MediaRouterMojoImpl::OnIssue(const interfaces::IssuePtr issue) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG_WITH_INSTANCE(1) << "OnIssue " << issue->title;
  const Issue& issue_converted = issue.To<Issue>();
  issue_manager_.AddIssue(issue_converted);
}

void MediaRouterMojoImpl::OnSinksReceived(
    const mojo::String& media_source,
    mojo::Array<interfaces::MediaSinkPtr> sinks,
    mojo::Array<mojo::String> origins) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG_WITH_INSTANCE(1) << "OnSinksReceived";
  auto it = sinks_queries_.find(media_source);
  if (it == sinks_queries_.end()) {
    DVLOG_WITH_INSTANCE(1) << "Received sink list without MediaSinksQuery.";
    return;
  }

  std::vector<GURL> origin_list;
  origin_list.reserve(origins.size());
  for (size_t i = 0; i < origins.size(); ++i) {
    GURL origin(origins[i].get());
    if (!origin.is_valid()) {
      LOG(WARNING) << "Received invalid origin: " << origin
                   << ". Dropping result.";
      return;
    }
    origin_list.push_back(origin);
  }

  std::vector<MediaSink> sink_list;
  sink_list.reserve(sinks.size());
  for (size_t i = 0; i < sinks.size(); ++i)
    sink_list.push_back(sinks[i].To<MediaSink>());

  auto* sinks_query = it->second;
  sinks_query->has_cached_result = true;
  sinks_query->origins.swap(origin_list);
  sinks_query->cached_sink_list.swap(sink_list);

  if (!sinks_query->observers.might_have_observers()) {
    DVLOG_WITH_INSTANCE(1)
        << "Received sink list without any active observers: " << media_source;
  } else {
    FOR_EACH_OBSERVER(
        MediaSinksObserver, sinks_query->observers,
        OnSinksUpdated(sinks_query->cached_sink_list, sinks_query->origins));
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

  FOR_EACH_OBSERVER(
      MediaRoutesObserver, it->second->observers,
      OnRoutesUpdated(routes_converted, joinable_routes_converted));
}

void MediaRouterMojoImpl::RouteResponseReceived(
    const std::string& presentation_id,
    bool off_the_record,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    interfaces::MediaRoutePtr media_route,
    const mojo::String& error_text,
    interfaces::RouteRequestResultCode result_code) {
  std::unique_ptr<RouteRequestResult> result;
  if (media_route.is_null()) {
    // An error occurred.
    DCHECK(!error_text.is_null());
    std::string error =
        !error_text.get().empty() ? error_text.get() : "Unknown error.";
    result = RouteRequestResult::FromError(
        error, mojo::RouteRequestResultCodeFromMojo(result_code));
  } else if (media_route->off_the_record != off_the_record) {
    std::string error = base::StringPrintf(
        "Mismatch in off the record status: request = %d, response = %d",
        off_the_record, media_route->off_the_record);
    result = RouteRequestResult::FromError(
        error, RouteRequestResult::OFF_THE_RECORD_MISMATCH);
  } else {
    // Off the record media routes do not support custom controllers.
    // crbug.com/590376
    if (media_route->off_the_record)
      media_route->custom_controller_path = nullptr;

    result = RouteRequestResult::FromSuccess(
        media_route.To<std::unique_ptr<MediaRoute>>(), presentation_id);
  }

  // TODO(imcheng): Add UMA histogram based on result code (crbug.com/583044).
  for (const MediaRouteResponseCallback& callback : callbacks)
    callback.Run(*result);
}

void MediaRouterMojoImpl::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool off_the_record) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    std::unique_ptr<RouteRequestResult> error_result(
        RouteRequestResult::FromError("Invalid origin",
                                      RouteRequestResult::INVALID_ORIGIN));
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(*error_result);
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::CREATE_ROUTE);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoCreateRoute,
                        base::Unretained(this), source_id, sink_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks, timeout, off_the_record));
}

void MediaRouterMojoImpl::JoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool off_the_record) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<RouteRequestResult> error_result;
  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    error_result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
  } else if (!HasLocalRoute()) {
    DVLOG_WITH_INSTANCE(1) << "No local routes";
    error_result = RouteRequestResult::FromError(
        "Route not found", RouteRequestResult::ROUTE_NOT_FOUND);
  }

  if (error_result) {
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(*error_result);
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::JOIN_ROUTE);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoJoinRoute,
                        base::Unretained(this), source_id, presentation_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks, timeout, off_the_record));
}

void MediaRouterMojoImpl::ConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool off_the_record) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(*result);
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::CONNECT_ROUTE_BY_ROUTE_ID);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoConnectRouteByRouteId,
                        base::Unretained(this), source_id, route_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks, timeout, off_the_record));
}

void MediaRouterMojoImpl::TerminateRoute(const MediaRoute::Id& route_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(2) << "TerminateRoute " << route_id;
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
    std::unique_ptr<std::vector<uint8_t>> data,
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

void MediaRouterMojoImpl::OnUserGesture() {
  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSourceForDesktop().id());

#if defined(OS_WIN)
  EnsureMdnsDiscoveryEnabled();
#endif
}

void MediaRouterMojoImpl::SearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    const MediaSinkSearchResponseCallback& sink_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SetWakeReason(MediaRouteProviderWakeReason::SEARCH_SINKS);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSearchSinks,
                        base::Unretained(this), sink_id, source_id,
                        search_input, domain, sink_callback));
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
    sinks_queries_.add(source_id, base::WrapUnique(sinks_query));
  } else {
    DCHECK(!sinks_query->observers.HasObserver(observer));
  }

  // If sink availability is UNAVAILABLE, then there is no need to call MRPM.
  // |observer| can be immediately notified with an empty list.
  sinks_query->observers.AddObserver(observer);
  if (availability_ == interfaces::MediaRouter::SinkAvailability::UNAVAILABLE) {
    observer->OnSinksUpdated(std::vector<MediaSink>(), std::vector<GURL>());
  } else {
    // Need to call MRPM to start observing sinks if the query is new.
    if (new_query) {
      SetWakeReason(MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_SINKS);
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaSinks,
                            base::Unretained(this), source_id));
    } else if (sinks_query->has_cached_result) {
      observer->OnSinksUpdated(sinks_query->cached_sink_list,
                               sinks_query->origins);
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
    routes_queries_.add(source_id, base::WrapUnique(routes_query));
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
    messages_observers_.add(route_id, base::WrapUnique(observer_list));
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

void MediaRouterMojoImpl::DoCreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool off_the_record) {
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  DVLOG_WITH_INSTANCE(1) << "DoCreateRoute " << source_id << "=>" << sink_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->CreateRoute(
      source_id, sink_id, presentation_id, origin, tab_id,
      timeout > base::TimeDelta() ? timeout.InMilliseconds() : 0,
      off_the_record, base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
                                 base::Unretained(this), presentation_id,
                                 off_the_record, callbacks));
}

void MediaRouterMojoImpl::DoJoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool off_the_record) {
  DVLOG_WITH_INSTANCE(1) << "DoJoinRoute " << source_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->JoinRoute(
      source_id, presentation_id, origin, tab_id,
      timeout > base::TimeDelta() ? timeout.InMilliseconds() : 0,
      off_the_record, base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
                                 base::Unretained(this), presentation_id,
                                 off_the_record, callbacks));
}

void MediaRouterMojoImpl::DoConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool off_the_record) {
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  DVLOG_WITH_INSTANCE(1) << "DoConnectRouteByRouteId " << source_id
                         << ", route ID: " << route_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->ConnectRouteByRouteId(
      source_id, route_id, presentation_id, origin, tab_id,
      timeout > base::TimeDelta() ? timeout.InMilliseconds() : 0,
      off_the_record, base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
                                 base::Unretained(this), presentation_id,
                                 off_the_record, callbacks));
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
    std::unique_ptr<std::vector<uint8_t>> data,
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

void MediaRouterMojoImpl::DoSearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    const MediaSinkSearchResponseCallback& sink_callback) {
  DVLOG_WITH_INSTANCE(1) << "SearchSinks";
  auto sink_search_criteria = interfaces::SinkSearchCriteria::New();
  sink_search_criteria->input = search_input;
  sink_search_criteria->domain = domain;
  media_route_provider_->SearchSinks(
      sink_id, source_id, std::move(sink_search_criteria), sink_callback);
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
      query->origins.clear();
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

void MediaRouterMojoImpl::OnPresentationConnectionClosed(
    const mojo::String& route_id,
    interfaces::MediaRouter::PresentationConnectionCloseReason reason,
    const mojo::String& message) {
  NotifyPresentationConnectionClose(
      route_id, mojo::PresentationConnectionCloseReasonFromMojo(reason),
      message);
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
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
        MediaRouteProviderWakeup::ERROR_TOO_MANY_RETRIES);
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
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeReason(
        current_wake_reason_);
    ClearWakeReason();
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
        MediaRouteProviderWakeup::SUCCESS);
    return;
  }

  // This is likely an non-retriable error. Drop the pending requests.
  DLOG_WITH_INSTANCE(ERROR)
      << "An error encountered while waking the event page.";
  ClearWakeReason();
  DrainPendingRequests();
  MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
      MediaRouteProviderWakeup::ERROR_UNKNOWN);
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

#if defined(OS_WIN)
void MediaRouterMojoImpl::EnsureMdnsDiscoveryEnabled() {
  if (is_mdns_enabled_)
    return;

  SetWakeReason(MediaRouteProviderWakeReason::ENABLE_MDNS_DISCOVERY);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoEnsureMdnsDiscoveryEnabled,
                        base::Unretained(this)));
  should_enable_mdns_discovery_ = true;
}

void MediaRouterMojoImpl::DoEnsureMdnsDiscoveryEnabled() {
  DVLOG_WITH_INSTANCE(1) << "DoEnsureMdnsDiscoveryEnabled";
  if (!is_mdns_enabled_) {
    media_route_provider_->EnableMdnsDiscovery();
    is_mdns_enabled_ = true;
  }
}

void MediaRouterMojoImpl::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports)
    EnsureMdnsDiscoveryEnabled();
}
#endif

void MediaRouterMojoImpl::UpdateMediaSinks(
    const MediaSource::Id& source_id) {
  SetWakeReason(MediaRouteProviderWakeReason::UPDATE_MEDIA_SINKS);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoUpdateMediaSinks,
                        base::Unretained(this), source_id));
}

void MediaRouterMojoImpl::DoUpdateMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoUpdateMediaSinks" << source_id;
  media_route_provider_->UpdateMediaSinks(source_id);
}

}  // namespace media_router
