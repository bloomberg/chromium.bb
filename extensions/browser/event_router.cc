// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/events/lazy_event_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

// A dictionary of event names to lists of filters that this extension has
// registered from its lazy background page.
const char kFilteredEvents[] = "filtered_events";

// Sends a notification about an event to the API activity monitor and the
// ExtensionHost for |extension_id| on the UI thread. Can be called from any
// thread.
void NotifyEventDispatched(void* browser_context_id,
                           const std::string& extension_id,
                           const std::string& event_name,
                           const base::ListValue& args) {
  // Notify the ApiActivityMonitor about the event dispatch.
  BrowserContext* context = static_cast<BrowserContext*>(browser_context_id);
  activity_monitor::OnApiEventDispatched(context, extension_id, event_name,
                                         args);
}

// A global identifier used to distinguish extension events.
base::AtomicSequenceNumber g_extension_event_id;

}  // namespace

const char EventRouter::kRegisteredLazyEvents[] = "events";
const char EventRouter::kRegisteredServiceWorkerEvents[] =
    "serviceworkerevents";

// static
void EventRouter::DispatchExtensionMessage(IPC::Sender* ipc_sender,
                                           int worker_thread_id,
                                           void* browser_context_id,
                                           const std::string& extension_id,
                                           int event_id,
                                           const std::string& event_name,
                                           ListValue* event_args,
                                           UserGestureState user_gesture,
                                           const EventFilteringInfo& info) {
  NotifyEventDispatched(browser_context_id, extension_id, event_name,
                        *event_args);

  ExtensionMsg_DispatchEvent_Params params;
  params.worker_thread_id = worker_thread_id;
  params.extension_id = extension_id;
  params.event_name = event_name;
  params.event_id = event_id;
  params.is_user_gesture = user_gesture == USER_GESTURE_ENABLED;
  params.filtering_info = info;

  ipc_sender->Send(new ExtensionMsg_DispatchEvent(params, *event_args));
}

// static
EventRouter* EventRouter::Get(content::BrowserContext* browser_context) {
  return EventRouterFactory::GetForBrowserContext(browser_context);
}

// static
std::string EventRouter::GetBaseEventName(const std::string& full_event_name) {
  size_t slash_sep = full_event_name.find('/');
  return full_event_name.substr(0, slash_sep);
}

// static
void EventRouter::DispatchEventToSender(IPC::Sender* ipc_sender,
                                        void* browser_context_id,
                                        const std::string& extension_id,
                                        events::HistogramValue histogram_value,
                                        const std::string& event_name,
                                        std::unique_ptr<ListValue> event_args,
                                        UserGestureState user_gesture,
                                        const EventFilteringInfo& info) {
  int event_id = g_extension_event_id.GetNext();

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DoDispatchEventToSenderBookkeepingOnUI(browser_context_id, extension_id,
                                           event_id, histogram_value,
                                           event_name);
  } else {
    // This is called from WebRequest API.
    // TODO(lazyboy): Skip this entirely: http://crbug.com/488747.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&EventRouter::DoDispatchEventToSenderBookkeepingOnUI,
                   browser_context_id, extension_id, event_id, histogram_value,
                   event_name));
  }

  DispatchExtensionMessage(ipc_sender,
                           // TODO(lazyboy): |kNonWorkerThreadId| means these
                           // will not work for extension SW.
                           kNonWorkerThreadId, browser_context_id, extension_id,
                           event_id, event_name, event_args.get(), user_gesture,
                           info);
}

// static.
bool EventRouter::CanDispatchEventToBrowserContext(BrowserContext* context,
                                                   const Extension* extension,
                                                   const Event& event) {
  // Is this event from a different browser context than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool crosses_incognito = event.restrict_to_browser_context &&
                           context != event.restrict_to_browser_context;
  if (!crosses_incognito)
    return true;
  return ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(extension,
                                                                    context);
}

EventRouter::EventRouter(BrowserContext* browser_context,
                         ExtensionPrefs* extension_prefs)
    : browser_context_(browser_context),
      extension_prefs_(extension_prefs),
      extension_registry_observer_(this),
      listeners_(this),
      lazy_event_dispatch_util_(browser_context_),
      weak_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

EventRouter::~EventRouter() {
  for (auto* process : observed_process_set_)
    process->RemoveObserver(this);
}

void EventRouter::AddEventListener(const std::string& event_name,
                                   content::RenderProcessHost* process,
                                   const std::string& extension_id) {
  listeners_.AddListener(
      EventListener::ForExtension(event_name, extension_id, process, nullptr));
}

void EventRouter::AddServiceWorkerEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int worker_thread_id) {
  listeners_.AddListener(EventListener::ForExtensionServiceWorker(
      event_name, extension_id, process, service_worker_scope, worker_thread_id,
      nullptr));
}

void EventRouter::RemoveEventListener(const std::string& event_name,
                                      content::RenderProcessHost* process,
                                      const std::string& extension_id) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtension(event_name, extension_id, process, nullptr);
  listeners_.RemoveListener(listener.get());
}

void EventRouter::RemoveServiceWorkerEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope,
    int worker_thread_id) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtensionServiceWorker(event_name, extension_id,
                                               process, service_worker_scope,
                                               worker_thread_id, nullptr);
  listeners_.RemoveListener(listener.get());
}

void EventRouter::AddEventListenerForURL(const std::string& event_name,
                                         content::RenderProcessHost* process,
                                         const GURL& listener_url) {
  listeners_.AddListener(
      EventListener::ForURL(event_name, listener_url, process, nullptr));
}

void EventRouter::RemoveEventListenerForURL(const std::string& event_name,
                                            content::RenderProcessHost* process,
                                            const GURL& listener_url) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForURL(event_name, listener_url, process, nullptr);
  listeners_.RemoveListener(listener.get());
}

void EventRouter::RegisterObserver(Observer* observer,
                                   const std::string& event_name) {
  // Observing sub-event names like "foo.onBar/123" is not allowed.
  DCHECK(event_name.find('/') == std::string::npos);
  observers_[event_name] = observer;
}

void EventRouter::UnregisterObserver(Observer* observer) {
  for (ObserverMap::iterator it = observers_.begin(); it != observers_.end();) {
    if (it->second == observer)
      it = observers_.erase(it);
    else
      ++it;
  }
}

void EventRouter::OnListenerAdded(const EventListener* listener) {
  const EventListenerInfo details(listener->event_name(),
                                  listener->extension_id(),
                                  listener->listener_url(),
                                  listener->GetBrowserContext());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  ObserverMap::iterator observer = observers_.find(base_event_name);
  if (observer != observers_.end())
    observer->second->OnListenerAdded(details);

  content::RenderProcessHost* process = listener->process();
  if (process) {
    bool inserted = observed_process_set_.insert(process).second;
    if (inserted)
      process->AddObserver(this);
  }
}

void EventRouter::OnListenerRemoved(const EventListener* listener) {
  const EventListenerInfo details(listener->event_name(),
                                  listener->extension_id(),
                                  listener->listener_url(),
                                  listener->GetBrowserContext());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  ObserverMap::iterator observer = observers_.find(base_event_name);
  if (observer != observers_.end())
    observer->second->OnListenerRemoved(details);
}

void EventRouter::RenderProcessExited(content::RenderProcessHost* host,
                                      base::TerminationStatus status,
                                      int exit_code) {
  listeners_.RemoveListenersForProcess(host);
  observed_process_set_.erase(host);
  host->RemoveObserver(this);
}

void EventRouter::RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  listeners_.RemoveListenersForProcess(host);
  observed_process_set_.erase(host);
  host->RemoveObserver(this);
}

void EventRouter::AddLazyEventListener(const std::string& event_name,
                                       const ExtensionId& extension_id) {
  AddLazyEventListenerImpl(
      EventListener::ForExtension(event_name, extension_id, nullptr, nullptr),
      RegisteredEventType::kLazy);
}

void EventRouter::RemoveLazyEventListener(const std::string& event_name,
                                          const ExtensionId& extension_id) {
  RemoveLazyEventListenerImpl(
      EventListener::ForExtension(event_name, extension_id, nullptr, nullptr),
      RegisteredEventType::kLazy);
}

void EventRouter::AddLazyServiceWorkerEventListener(
    const std::string& event_name,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtensionServiceWorker(
          event_name, extension_id, nullptr, service_worker_scope,
          kNonWorkerThreadId,  // Lazy, without worker thread id.
          nullptr);
  AddLazyEventListenerImpl(std::move(listener),
                           RegisteredEventType::kServiceWorker);
}

void EventRouter::RemoveLazyServiceWorkerEventListener(
    const std::string& event_name,
    const ExtensionId& extension_id,
    const GURL& service_worker_scope) {
  std::unique_ptr<EventListener> listener =
      EventListener::ForExtensionServiceWorker(
          event_name, extension_id, nullptr, service_worker_scope,
          kNonWorkerThreadId,  // Lazy, without worker thread id.
          nullptr);
  RemoveLazyEventListenerImpl(std::move(listener),
                              RegisteredEventType::kServiceWorker);
}

// TODO(lazyboy): Support filters for extension SW events.
void EventRouter::AddFilteredEventListener(const std::string& event_name,
                                           content::RenderProcessHost* process,
                                           const std::string& extension_id,
                                           const base::DictionaryValue& filter,
                                           bool add_lazy_listener) {
  listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, process,
      std::unique_ptr<DictionaryValue>(filter.DeepCopy())));

  if (!add_lazy_listener)
    return;

  bool added = listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, nullptr,
      std::unique_ptr<DictionaryValue>(filter.DeepCopy())));
  if (added)
    AddFilterToEvent(event_name, extension_id, &filter);
}

// TODO(lazyboy): Support filters for extension SW events.
void EventRouter::RemoveFilteredEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const std::string& extension_id,
    const base::DictionaryValue& filter,
    bool remove_lazy_listener) {
  std::unique_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, process,
      std::unique_ptr<DictionaryValue>(filter.DeepCopy()));

  listeners_.RemoveListener(listener.get());

  if (remove_lazy_listener) {
    listener->MakeLazy();
    bool removed = listeners_.RemoveListener(listener.get());

    if (removed)
      RemoveFilterFromEvent(event_name, extension_id, &filter);
  }
}

bool EventRouter::HasEventListener(const std::string& event_name) const {
  return listeners_.HasListenerForEvent(event_name);
}

bool EventRouter::ExtensionHasEventListener(
    const std::string& extension_id,
    const std::string& event_name) const {
  return listeners_.HasListenerForExtension(extension_id, event_name);
}

std::set<std::string> EventRouter::GetRegisteredEvents(
    const std::string& extension_id,
    RegisteredEventType type) const {
  std::set<std::string> events;
  const ListValue* events_value = NULL;

  const char* pref_key = type == RegisteredEventType::kLazy
                             ? kRegisteredLazyEvents
                             : kRegisteredServiceWorkerEvents;
  if (!extension_prefs_ || !extension_prefs_->ReadPrefAsList(
                               extension_id, pref_key, &events_value)) {
    return events;
  }

  for (size_t i = 0; i < events_value->GetSize(); ++i) {
    std::string event;
    if (events_value->GetString(i, &event))
      events.insert(event);
  }
  return events;
}

void EventRouter::ClearRegisteredEventsForTest(
    const ExtensionId& extension_id) {
  SetRegisteredEvents(extension_id, std::set<std::string>(),
                      RegisteredEventType::kLazy);
  SetRegisteredEvents(extension_id, std::set<std::string>(),
                      RegisteredEventType::kServiceWorker);
}

bool EventRouter::HasLazyEventListenerForTesting(
    const std::string& event_name) {
  const EventListenerMap::ListenerList& listeners =
      listeners_.GetEventListenersByName(event_name);
  return std::any_of(listeners.begin(), listeners.end(),
                     [](const std::unique_ptr<EventListener>& listener) {
                       return listener->IsLazy();
                     });
}

void EventRouter::RemoveFilterFromEvent(const std::string& event_name,
                                        const std::string& extension_id,
                                        const DictionaryValue* filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id, kFilteredEvents);
  auto filtered_events = update.Create();
  ListValue* filter_list = NULL;
  if (!filtered_events ||
      !filtered_events->GetListWithoutPathExpansion(event_name, &filter_list)) {
    return;
  }

  for (size_t i = 0; i < filter_list->GetSize(); i++) {
    DictionaryValue* filter_value = nullptr;
    CHECK(filter_list->GetDictionary(i, &filter_value));
    if (filter_value->Equals(filter)) {
      filter_list->Remove(i, nullptr);
      break;
    }
  }
}

const DictionaryValue* EventRouter::GetFilteredEvents(
    const std::string& extension_id) {
  const DictionaryValue* events = NULL;
  extension_prefs_->ReadPrefAsDictionary(
      extension_id, kFilteredEvents, &events);
  return events;
}

void EventRouter::BroadcastEvent(std::unique_ptr<Event> event) {
  DispatchEventImpl(std::string(), linked_ptr<Event>(event.release()));
}

void EventRouter::DispatchEventToExtension(const std::string& extension_id,
                                           std::unique_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  DispatchEventImpl(extension_id, linked_ptr<Event>(event.release()));
}

void EventRouter::DispatchEventWithLazyListener(const std::string& extension_id,
                                                std::unique_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  std::string event_name = event->event_name;
  bool has_listener = ExtensionHasEventListener(extension_id, event_name);
  if (!has_listener)
    AddLazyEventListener(event_name, extension_id);
  DispatchEventToExtension(extension_id, std::move(event));
  if (!has_listener)
    RemoveLazyEventListener(event_name, extension_id);
}

void EventRouter::DispatchEventImpl(const std::string& restrict_to_extension_id,
                                    const linked_ptr<Event>& event) {
  // We don't expect to get events from a completely different browser context.
  DCHECK(!event->restrict_to_browser_context ||
         ExtensionsBrowserClient::Get()->IsSameContext(
             browser_context_, event->restrict_to_browser_context));
  std::set<const EventListener*> listeners(
      listeners_.GetEventListeners(*event));

  LazyEventDispatcher lazy_event_dispatcher(
      browser_context_, event,
      base::Bind(&EventRouter::DispatchPendingEvent,
                 weak_factory_.GetWeakPtr()));

  // We dispatch events for lazy background pages first because attempting to do
  // so will cause those that are being suspended to cancel that suspension.
  // As canceling a suspension entails sending an event to the affected
  // background page, and as that event needs to be delivered before we dispatch
  // the event we are dispatching here, we dispatch to the lazy listeners here
  // first.
  for (const EventListener* listener : listeners) {
    if (!restrict_to_extension_id.empty() &&
        restrict_to_extension_id != listener->extension_id()) {
      continue;
    }
    if (listener->IsLazy()) {
      if (listener->is_for_service_worker()) {
        lazy_event_dispatcher.DispatchToServiceWorker(
            listener->extension_id(), listener->listener_url(), nullptr);
      } else {
        lazy_event_dispatcher.DispatchToEventPage(listener->extension_id(),
                                                  listener->filter());
      }
    }
  }

  for (const EventListener* listener : listeners) {
    if (!restrict_to_extension_id.empty() &&
        restrict_to_extension_id != listener->extension_id()) {
      continue;
    }
    if (!listener->process())
      continue;
    if (lazy_event_dispatcher.HasAlreadyDispatched(
            listener->process()->GetBrowserContext(), listener)) {
      continue;
    }
    DispatchEventToProcess(listener->extension_id(), listener->listener_url(),
                           listener->process(), listener->worker_thread_id(),
                           event, listener->filter(), false /* did_enqueue */);
  }
}

void EventRouter::DispatchEventToProcess(
    const std::string& extension_id,
    const GURL& listener_url,
    content::RenderProcessHost* process,
    int worker_thread_id,
    const linked_ptr<Event>& event,
    const base::DictionaryValue* listener_filter,
    bool did_enqueue) {
  BrowserContext* listener_context = process->GetBrowserContext();
  ProcessMap* process_map = ProcessMap::Get(listener_context);

  // NOTE: |extension| being NULL does not necessarily imply that this event
  // shouldn't be dispatched. Events can be dispatched to WebUI and webviews as
  // well.  It all depends on what GetMostLikelyContextType returns.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions().GetByID(
          extension_id);

  if (!extension && !extension_id.empty()) {
    // Trying to dispatch an event to an extension that doesn't exist. The
    // extension could have been removed, but we do not unregister it until the
    // extension process is unloaded.
    return;
  }

  if (extension) {
    // Extension-specific checks.
    // Firstly, if the event is for a URL, the Extension must have permission
    // to access that URL.
    if (!event->event_url.is_empty() &&
        event->event_url.host() != extension->id() &&  // event for self is ok
        !extension->permissions_data()
             ->active_permissions()
             .HasEffectiveAccessToURL(event->event_url)) {
      return;
    }
    // Secondly, if the event is for incognito mode, the Extension must be
    // enabled in incognito mode.
    if (!CanDispatchEventToBrowserContext(listener_context, extension,
                                          *event)) {
      return;
    }
  }

  Feature::Context target_context =
      process_map->GetMostLikelyContextType(extension, process->GetID());

  // We shouldn't be dispatching an event to a webpage, since all such events
  // (e.g.  messaging) don't go through EventRouter.
  DCHECK_NE(Feature::WEB_PAGE_CONTEXT, target_context)
      << "Trying to dispatch event " << event->event_name << " to a webpage,"
      << " but this shouldn't be possible";

  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          event->event_name, extension, target_context, listener_url,
          CheckAliasStatus::ALLOWED);
  if (!availability.is_available()) {
    // It shouldn't be possible to reach here, because access is checked on
    // registration. However, for paranoia, check on dispatch as well.
    NOTREACHED() << "Trying to dispatch event " << event->event_name
                 << " which the target does not have access to: "
                 << availability.message();
    return;
  }

  if (!event->will_dispatch_callback.is_null() &&
      !event->will_dispatch_callback.Run(listener_context, extension,
                                         event.get(), listener_filter)) {
    return;
  }

  int event_id = g_extension_event_id.GetNext();
  DispatchExtensionMessage(process, worker_thread_id, listener_context,
                           extension_id, event_id, event->event_name,
                           event->event_args.get(), event->user_gesture,
                           event->filter_info);

  // TODO(lazyboy): This is wrong for extensions SW events. We need to:
  // 1. Increment worker ref count
  // 2. Add EventAck IPC to decrement that ref count.
  if (extension) {
    ReportEvent(event->histogram_value, extension, did_enqueue);
    IncrementInFlightEvents(listener_context, extension, event_id,
                            event->event_name);
  }
}

// static
void EventRouter::DoDispatchEventToSenderBookkeepingOnUI(
    void* browser_context_id,
    const std::string& extension_id,
    int event_id,
    events::HistogramValue histogram_value,
    const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      reinterpret_cast<BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return;
  EventRouter* event_router = EventRouter::Get(browser_context);
  event_router->IncrementInFlightEvents(browser_context, extension, event_id,
                                        event_name);
  event_router->ReportEvent(histogram_value, extension,
                            false /* did_enqueue */);
}

void EventRouter::IncrementInFlightEvents(BrowserContext* context,
                                          const Extension* extension,
                                          int event_id,
                                          const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only increment in-flight events if the lazy background page is active,
  // because that's the only time we'll get an ACK.
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ProcessManager* pm = ProcessManager::Get(context);
    ExtensionHost* host = pm->GetBackgroundHostForExtension(extension->id());
    if (host) {
      pm->IncrementLazyKeepaliveCount(extension);
      host->OnBackgroundEventDispatched(event_name, event_id);
    }
  }
}

void EventRouter::OnEventAck(BrowserContext* context,
                             const std::string& extension_id) {
  ProcessManager* pm = ProcessManager::Get(context);
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id);
  // The event ACK is routed to the background host, so this should never be
  // NULL.
  CHECK(host);
  // TODO(mpcomplete): We should never get this message unless
  // HasLazyBackgroundPage is true. Find out why we're getting it anyway.
  if (host->extension() &&
      BackgroundInfo::HasLazyBackgroundPage(host->extension()))
    pm->DecrementLazyKeepaliveCount(host->extension());
}

void EventRouter::ReportEvent(events::HistogramValue histogram_value,
                              const Extension* extension,
                              bool did_enqueue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record every event fired.
  UMA_HISTOGRAM_ENUMERATION("Extensions.Events.Dispatch", histogram_value,
                            events::ENUM_BOUNDARY);

  bool is_component = Manifest::IsComponentLocation(extension->location());

  // Record events for component extensions. These should be kept to a minimum,
  // especially if they wake its event page. Component extensions should use
  // declarative APIs as much as possible.
  if (is_component) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.Events.DispatchToComponent",
                              histogram_value, events::ENUM_BOUNDARY);
  }

  // Record events for background pages, if any. The most important statistic
  // is DispatchWithSuspendedEventPage. Events reported there woke an event
  // page. Implementing either filtered or declarative versions of these events
  // should be prioritised.
  //
  // Note: all we know is that the extension *has* a persistent or event page,
  // not that the event is being dispatched *to* such a page. However, this is
  // academic, since extensions with any background page have that background
  // page running (or in the case of suspended event pages, must be started)
  // regardless of where the event is being dispatched. Events are dispatched
  // to a *process* not a *frame*.
  if (BackgroundInfo::HasPersistentBackgroundPage(extension)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.Events.DispatchWithPersistentBackgroundPage",
        histogram_value, events::ENUM_BOUNDARY);
  } else if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    if (did_enqueue) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.Events.DispatchWithSuspendedEventPage", histogram_value,
          events::ENUM_BOUNDARY);
      if (is_component) {
        UMA_HISTOGRAM_ENUMERATION(
            "Extensions.Events.DispatchToComponentWithSuspendedEventPage",
            histogram_value, events::ENUM_BOUNDARY);
      }
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.Events.DispatchWithRunningEventPage", histogram_value,
          events::ENUM_BOUNDARY);
    }
  }
}

void EventRouter::DispatchPendingEvent(
    const linked_ptr<Event>& event,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> params) {
  if (!params)
    return;

  if (listeners_.HasProcessListener(params->render_process_host,
                                    params->worker_thread_id,
                                    params->extension_id)) {
    DispatchEventToProcess(
        params->extension_id, params->url, params->render_process_host,
        params->worker_thread_id, event, nullptr, true /* did_enqueue */);
  }
}

void EventRouter::SetRegisteredEvents(const std::string& extension_id,
                                      const std::set<std::string>& events,
                                      RegisteredEventType type) {
  auto events_value = base::MakeUnique<base::ListValue>();
  for (std::set<std::string>::const_iterator iter = events.begin();
       iter != events.end(); ++iter) {
    events_value->AppendString(*iter);
  }
  const char* pref_key = type == RegisteredEventType::kLazy
                             ? kRegisteredLazyEvents
                             : kRegisteredServiceWorkerEvents;
  extension_prefs_->UpdateExtensionPref(extension_id, pref_key,
                                        std::move(events_value));
}

void EventRouter::AddFilterToEvent(const std::string& event_name,
                                   const std::string& extension_id,
                                   const DictionaryValue* filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(extension_prefs_, extension_id,
                                                kFilteredEvents);
  auto filtered_events = update.Create();

  ListValue* filter_list = nullptr;
  if (!filtered_events->GetListWithoutPathExpansion(event_name, &filter_list)) {
    filtered_events->SetWithoutPathExpansion(
        event_name, base::MakeUnique<base::ListValue>());
    filtered_events->GetListWithoutPathExpansion(event_name, &filter_list);
  }

  filter_list->Append(filter->CreateDeepCopy());
}

void EventRouter::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // Add all registered lazy listeners to our cache.
  // TODO(lazyboy): Load extension SW lazy events.
  std::set<std::string> registered_events =
      GetRegisteredEvents(extension->id(), RegisteredEventType::kLazy);
  listeners_.LoadUnfilteredLazyListeners(extension->id(), registered_events);
  // TODO(lazyboy): Load extension SW filtered events when they are available.
  const DictionaryValue* filtered_events = GetFilteredEvents(extension->id());
  if (filtered_events)
    listeners_.LoadFilteredLazyListeners(extension->id(), *filtered_events);
}

void EventRouter::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  // Remove all registered listeners from our cache.
  listeners_.RemoveListenersForExtension(extension->id());
}

void EventRouter::AddLazyEventListenerImpl(
    std::unique_ptr<EventListener> listener,
    RegisteredEventType type) {
  const ExtensionId extension_id = listener->extension_id();
  const std::string event_name = listener->event_name();
  bool is_new = listeners_.AddListener(std::move(listener));
  if (is_new) {
    std::set<std::string> events = GetRegisteredEvents(extension_id, type);
    bool prefs_is_new = events.insert(event_name).second;
    if (prefs_is_new)
      SetRegisteredEvents(extension_id, events, type);
  }
}

void EventRouter::RemoveLazyEventListenerImpl(
    std::unique_ptr<EventListener> listener,
    RegisteredEventType type) {
  const ExtensionId extension_id = listener->extension_id();
  const std::string event_name = listener->event_name();
  bool did_exist = listeners_.RemoveListener(listener.get());
  if (did_exist) {
    std::set<std::string> events = GetRegisteredEvents(extension_id, type);
    bool prefs_did_exist = events.erase(event_name) > 0;
    DCHECK(prefs_did_exist);
    SetRegisteredEvents(extension_id, events, type);
  }
}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             std::unique_ptr<base::ListValue> event_args)
    : Event(histogram_value, event_name, std::move(event_args), nullptr) {}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             std::unique_ptr<base::ListValue> event_args,
             BrowserContext* restrict_to_browser_context)
    : Event(histogram_value,
            event_name,
            std::move(event_args),
            restrict_to_browser_context,
            GURL(),
            EventRouter::USER_GESTURE_UNKNOWN,
            EventFilteringInfo()) {}

Event::Event(events::HistogramValue histogram_value,
             const std::string& event_name,
             std::unique_ptr<ListValue> event_args_tmp,
             BrowserContext* restrict_to_browser_context,
             const GURL& event_url,
             EventRouter::UserGestureState user_gesture,
             const EventFilteringInfo& filter_info)
    : histogram_value(histogram_value),
      event_name(event_name),
      event_args(std::move(event_args_tmp)),
      restrict_to_browser_context(restrict_to_browser_context),
      event_url(event_url),
      user_gesture(user_gesture),
      filter_info(filter_info) {
  DCHECK(event_args);
  DCHECK_NE(events::UNKNOWN, histogram_value)
      << "events::UNKNOWN cannot be used as a histogram value.\n"
      << "If this is a test, use events::FOR_TEST.\n"
      << "If this is production code, it is important that you use a realistic "
      << "value so that we can accurately track event usage. "
      << "See extension_event_histogram_value.h for inspiration.";
}

Event::~Event() {}

Event* Event::DeepCopy() {
  Event* copy = new Event(
      histogram_value, event_name,
      std::unique_ptr<base::ListValue>(event_args->DeepCopy()),
      restrict_to_browser_context, event_url, user_gesture, filter_info);
  copy->will_dispatch_callback = will_dispatch_callback;
  return copy;
}

EventListenerInfo::EventListenerInfo(const std::string& event_name,
                                     const std::string& extension_id,
                                     const GURL& listener_url,
                                     content::BrowserContext* browser_context)
    : event_name(event_name),
      extension_id(extension_id),
      listener_url(listener_url),
      browser_context(browser_context) {
}

}  // namespace extensions
