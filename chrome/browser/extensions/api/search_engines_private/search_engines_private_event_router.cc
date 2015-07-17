// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search_engines_private/search_engines_private_event_router.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/api/search_engines_private.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

namespace search_engines_private = api::search_engines_private;

SearchEnginesPrivateEventRouter::SearchEnginesPrivateEventRouter(
    content::BrowserContext* context)
    : template_url_service_(nullptr),
      context_(context),
      listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router)
    return;

  event_router->RegisterObserver(
      this, search_engines_private::OnSearchEnginesChanged::kEventName);
  template_url_service_ = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  StartOrStopListeningForSearchEnginesChanges();
}

SearchEnginesPrivateEventRouter::~SearchEnginesPrivateEventRouter() {
  DCHECK(!listening_);
}

void SearchEnginesPrivateEventRouter::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_)
    template_url_service_->RemoveObserver(this);

  listening_ = false;
}

void SearchEnginesPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to search engines change events.
  StartOrStopListeningForSearchEnginesChanges();
}

void SearchEnginesPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events if there are no more listeners.
  StartOrStopListeningForSearchEnginesChanges();
}

void SearchEnginesPrivateEventRouter::OnTemplateURLServiceChanged() {
  if (!template_url_service_ || !template_url_service_->loaded())
    return;

  const TemplateURL* default_url =
      template_url_service_->GetDefaultSearchProvider();
  std::vector<TemplateURL*> urls = template_url_service_->GetTemplateURLs();

  std::vector<linked_ptr<search_engines_private::SearchEngine>> engines;

  for (TemplateURL* url : urls) {
    search_engines_private::SearchEngine* engine =
        new search_engines_private::SearchEngine();
    engine->guid = url->sync_guid();
    engine->name = base::UTF16ToUTF8(url->short_name());
    engine->keyword = base::UTF16ToUTF8(url->keyword());
    engine->url = url->url();
    engine->type = url->show_in_default_list()
        ? search_engines_private::SearchEngineType::SEARCH_ENGINE_TYPE_DEFAULT
        : search_engines_private::SearchEngineType::SEARCH_ENGINE_TYPE_OTHER;

    if (url == default_url)
      engine->is_selected = scoped_ptr<bool>(new bool(true));

    engines.push_back(
        linked_ptr<search_engines_private::SearchEngine>(engine));
  }

  scoped_ptr<base::ListValue> args(
      search_engines_private::OnSearchEnginesChanged::Create(engines));
  scoped_ptr<Event> extension_event(new Event(
      events::SEARCH_ENGINES_PRIVATE_ON_SEARCH_ENGINES_CHANGED,
      search_engines_private::OnSearchEnginesChanged::kEventName, args.Pass()));
  EventRouter::Get(context_)->BroadcastEvent(extension_event.Pass());
}

void SearchEnginesPrivateEventRouter::
    StartOrStopListeningForSearchEnginesChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen = event_router->HasEventListener(
      search_engines_private::OnSearchEnginesChanged::kEventName);

  if (should_listen && !listening_) {
    template_url_service_->Load();
    template_url_service_->AddObserver(this);
  } else if (!should_listen && listening_) {
    template_url_service_->RemoveObserver(this);
  }

  listening_ = should_listen;
}

SearchEnginesPrivateEventRouter* SearchEnginesPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new SearchEnginesPrivateEventRouter(context);
}

}  // namespace extensions
