// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_mojo_impl.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_type_converters.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/presentation_session_messages_observer.h"
#include "extensions/browser/process_manager.h"

#define DVLOG_WITH_INSTANCE(level) \
  DVLOG(level) << "MR #" << instance_id_ << ": "

#define DLOG_WITH_INSTANCE(level) DLOG(level) << "MR #" << instance_id_ << ": "

namespace media_router {
namespace {

// Converts the callback result of calling Mojo CreateRoute()/JoinRoute()
// into a local callback.
void RouteResponseReceived(
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
  }

  for (const MediaRouteResponseCallback& callback : callbacks)
    callback.Run(route.get(), actual_presentation_id, error);
}

// TODO(imcheng): We should handle failure in this case. One way is to invoke
// all pending requests with failure. (crbug.com/490787)
void EventPageWakeComplete(bool success) {
  if (!success)
    LOG(ERROR) << "An error encountered while waking the event page.";
}

scoped_ptr<content::PresentationSessionMessage>
ConvertToPresentationSessionMessage(interfaces::RouteMessagePtr input) {
  DCHECK(!input.is_null());
  scoped_ptr<content::PresentationSessionMessage> output;
  switch (input->type) {
    case interfaces::RouteMessage::Type::TYPE_TEXT: {
      DCHECK(!input->message.is_null());
      DCHECK(input->data.is_null());
      output.reset(new content::PresentationSessionMessage(
          content::PresentationMessageType::TEXT));
      input->message.Swap(&output->message);
      return output.Pass();
    }
    case interfaces::RouteMessage::Type::TYPE_BINARY: {
      DCHECK(!input->data.is_null());
      DCHECK(input->message.is_null());
      output.reset(new content::PresentationSessionMessage(
          content::PresentationMessageType::ARRAY_BUFFER));
      output->data.reset(new std::vector<uint8_t>);
      input->data.Swap(output->data.get());
      return output.Pass();
    }
  }

  NOTREACHED() << "Invalid route message type " << input->type;
  return output.Pass();
}

}  // namespace

MediaRouterMojoImpl::MediaRouterMojoImpl(
    extensions::EventPageTracker* event_page_tracker)
    : event_page_tracker_(event_page_tracker),
      instance_id_(base::GenerateGUID()) {
  DCHECK(event_page_tracker_);
}

MediaRouterMojoImpl::~MediaRouterMojoImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
void MediaRouterMojoImpl::BindToRequest(
    const std::string& extension_id,
    content::BrowserContext* context,
    mojo::InterfaceRequest<interfaces::MediaRouter> request) {
  MediaRouterMojoImpl* impl = static_cast<MediaRouterMojoImpl*>(
      MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(impl);

  impl->BindToMojoRequest(request.Pass(), extension_id);
}

void MediaRouterMojoImpl::BindToMojoRequest(
    mojo::InterfaceRequest<interfaces::MediaRouter> request,
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  binding_.reset(
      new mojo::Binding<interfaces::MediaRouter>(this, request.Pass()));
  binding_->set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));

  media_route_provider_extension_id_ = extension_id;
}

// TODO(imcheng): If this occurs while there are pending requests, we should
// probably invoke them with failure. (crbug.com/490787)
void MediaRouterMojoImpl::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_route_provider_.reset();
  binding_.reset();
}

void MediaRouterMojoImpl::RegisterMediaRouteProvider(
    interfaces::MediaRouteProviderPtr media_route_provider_ptr,
    const interfaces::MediaRouter::RegisterMediaRouteProviderCallback&
        callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  media_route_provider_ = media_route_provider_ptr.Pass();
  media_route_provider_.set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));
  callback.Run(instance_id_);
  ExecutePendingRequests();
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
  std::vector<MediaSink> sinks_converted;
  sinks_converted.reserve(sinks.size());

  for (size_t i = 0; i < sinks.size(); ++i) {
    sinks_converted.push_back(sinks[i].To<MediaSink>());
  }

  auto it = sinks_observers_.find(media_source);
  if (it == sinks_observers_.end()) {
    DVLOG_WITH_INSTANCE(1)
        << "Received sink list without any active observers: " << media_source;
  } else {
    FOR_EACH_OBSERVER(MediaSinksObserver, *it->second,
                      OnSinksReceived(sinks_converted));
  }
}

void MediaRouterMojoImpl::OnRoutesUpdated(
    mojo::Array<interfaces::MediaRoutePtr> routes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG_WITH_INSTANCE(1) << "OnRoutesUpdated";

  std::vector<MediaRoute> routes_converted;
  routes_converted.reserve(routes.size());

  for (size_t i = 0; i < routes.size(); ++i) {
    routes_converted.push_back(routes[i].To<MediaRoute>());
  }

  FOR_EACH_OBSERVER(MediaRoutesObserver, routes_observers_,
                    OnRoutesUpdated(routes_converted));
}

void MediaRouterMojoImpl::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(nullptr, "", "Invalid origin");
    return;
  }
  RunOrDefer(base::Bind(
      &MediaRouterMojoImpl::DoCreateRoute, base::Unretained(this), source_id,
      sink_id, origin.is_empty() ? "" : origin.spec(), tab_id, callbacks));
}

void MediaRouterMojoImpl::JoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const GURL& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    for (const MediaRouteResponseCallback& callback : callbacks)
      callback.Run(nullptr, "", "Invalid origin");
    return;
  }
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoJoinRoute,
                        base::Unretained(this), source_id, presentation_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks));
}

void MediaRouterMojoImpl::CloseRoute(const MediaRoute::Id& route_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoCloseRoute,
                        base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::SendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSendSessionMessage,
                        base::Unretained(this), route_id, message, callback));
}

void MediaRouterMojoImpl::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    scoped_ptr<std::vector<uint8>> data,
    const SendRouteMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSendSessionBinaryMessage,
                        base::Unretained(this), route_id,
                        base::Passed(data.Pass()), callback));
}

void MediaRouterMojoImpl::ClearIssue(const Issue::Id& issue_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  issue_manager_.ClearIssue(issue_id);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoClearIssue,
                        base::Unretained(this), issue_id));
}

void MediaRouterMojoImpl::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Create an observer list for the media source and add |observer|
  // to it. Fail if |observer| is already registered.
  const std::string& source_id = observer->source().id();
  base::ObserverList<MediaSinksObserver>* observer_list =
      sinks_observers_.get(source_id);
  if (!observer_list) {
    observer_list = new base::ObserverList<MediaSinksObserver>;
    sinks_observers_.add(source_id, make_scoped_ptr(observer_list));
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  // We need to call DoStartObservingMediaSinks every time an observer is
  // added to ensure the observer will be notified with a fresh set of results.
  // TODO(imcheng): Implement caching. (crbug.com/492451)
  observer_list->AddObserver(observer);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaSinks,
                        base::Unretained(this), source_id));
}

void MediaRouterMojoImpl::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const MediaSource::Id& source_id = observer->source().id();
  auto* observer_list = sinks_observers_.get(source_id);
  if (!observer_list || !observer_list->HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  observer_list->RemoveObserver(observer);
  if (!observer_list->might_have_observers()) {
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopObservingMediaSinks,
                          base::Unretained(this), source_id));
    sinks_observers_.erase(source_id);
  }
}

void MediaRouterMojoImpl::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!routes_observers_.HasObserver(observer));

  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaRoutes,
                        base::Unretained(this)));
  routes_observers_.AddObserver(observer);
}

void MediaRouterMojoImpl::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  if (!routes_observers_.HasObserver(observer))
    return;

  routes_observers_.RemoveObserver(observer);
  if (!routes_observers_.might_have_observers()) {
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopObservingMediaRoutes,
                          base::Unretained(this)));
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
  if (!observer_list->might_have_observers())
    messages_observers_.erase(route_id);
  // TODO(imcheng): Queue a task to stop listening for messages by asking
  // the extension to invoke the oustanding Mojo callback with empty list. We
  // don't want the Mojo callback to exist indefinitely on the extension side
  // and there is currently no way to cancel the callback from this side.
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
      base::Bind(&RouteResponseReceived, presentation_id, callbacks));
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
      base::Bind(&RouteResponseReceived, presentation_id, callbacks));
}

void MediaRouterMojoImpl::DoCloseRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoCloseRoute " << route_id;
  media_route_provider_->CloseRoute(route_id);
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
    scoped_ptr<std::vector<uint8>> data,
    const SendRouteMessageCallback& callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteBinaryMessage " << route_id;
  mojo::Array<uint8> mojo_array;
  mojo_array.Swap(data.get());
  media_route_provider_->SendRouteBinaryMessage(route_id, mojo_array.Pass(),
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

void MediaRouterMojoImpl::OnRouteMessagesReceived(
    const MediaRoute::Id& route_id,
    mojo::Array<interfaces::RouteMessagePtr> messages) {
  DVLOG(1) << "OnRouteMessagesReceived";

  // Check if there are any observers remaining. If not, the messages
  // can be discarded and we can stop listening for the next batch of messages.
  auto* observer_list = messages_observers_.get(route_id);
  if (!observer_list) {
    route_ids_listening_for_messages_.erase(route_id);
    return;
  }

  // Empty |messages| means we told the extension that we were no longer
  // listening for messages on that route. But since now we have observers
  // again, we should keep listening.
  if (messages.storage().empty()) {
    DVLOG(2) << "Received empty messages for " << route_id;
  } else {
    ScopedVector<content::PresentationSessionMessage> session_messages;
    session_messages.reserve(messages.size());
    for (size_t i = 0; i < messages.size(); ++i) {
      session_messages.push_back(
          ConvertToPresentationSessionMessage(messages[i].Pass()).Pass());
    }

    // TODO(imcheng): If there is only 1 observer, we should be able to pass
    // the messages to avoid additional copies. (crbug.com/517234)
    FOR_EACH_OBSERVER(PresentationSessionMessagesObserver, *observer_list,
                      OnMessagesReceived(session_messages));
  }

  // Listen for more messages.
  media_route_provider_->ListenForRouteMessages(
      route_id, base::Bind(&MediaRouterMojoImpl::OnRouteMessagesReceived,
                           base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::DoClearIssue(const Issue::Id& issue_id) {
  DVLOG_WITH_INSTANCE(1) << "DoClearIssue " << issue_id;
  media_route_provider_->ClearIssue(issue_id);
}

void MediaRouterMojoImpl::DoStartObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaSinks: " << source_id;
  media_route_provider_->StartObservingMediaSinks(source_id);
}

void MediaRouterMojoImpl::DoStopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaSinks: " << source_id;
  media_route_provider_->StopObservingMediaSinks(source_id);
}

void MediaRouterMojoImpl::DoStartObservingMediaRoutes() {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaRoutes";
  media_route_provider_->StartObservingMediaRoutes();
}

void MediaRouterMojoImpl::DoStopObservingMediaRoutes() {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaRoutes";
  media_route_provider_->StopObservingMediaRoutes();
}

void MediaRouterMojoImpl::EnqueueTask(const base::Closure& closure) {
  pending_requests_.push_back(closure);
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
    if (!event_page_tracker_->WakeEventPage(
            media_route_provider_extension_id_,
            base::Bind(&EventPageWakeComplete))) {
      LOG(ERROR) << "An error encountered while waking the event page.";
    }
    media_route_provider_.reset();
  } else if (!media_route_provider_) {
    DVLOG_WITH_INSTANCE(1) << "Extension is awake, awaiting ProvideMediaRouter "
                              " to be called.";
    EnqueueTask(request);
  } else {
    request.Run();
  }
}

void MediaRouterMojoImpl::ExecutePendingRequests() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(media_route_provider_);
  DCHECK(event_page_tracker_);
  DCHECK(!media_route_provider_extension_id_.empty());

  if (event_page_tracker_->IsEventPageSuspended(
          media_route_provider_extension_id_)) {
    DVLOG_WITH_INSTANCE(1)
        << "ExecutePendingRequests was called while extension is suspended.";
    return;
  }

  for (const auto& next_request : pending_requests_)
    next_request.Run();

  pending_requests_.clear();
}

}  // namespace media_router
