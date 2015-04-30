// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_EVENT_ROUTER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// This is an event router that will observe changes to the available and
// selected search engines via the TemplateURLService, and notify listeners to
// events on the searchEnginesPrivate API of changes.
class SearchEnginesPrivateEventRouter : public KeyedService,
                                        public EventRouter::Observer,
                                        public TemplateURLServiceObserver {
 public:
  static SearchEnginesPrivateEventRouter* Create(
      content::BrowserContext* browser_context);
  ~SearchEnginesPrivateEventRouter() override;

  // TemplateURLServiceObserver implementation.
  void OnTemplateURLServiceChanged() override;

 protected:
  explicit SearchEnginesPrivateEventRouter(content::BrowserContext* context);

  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  TemplateURLService* template_url_service_;

 private:
  // Decide if we should listen for search engines changes or not. If there are
  // any JavaScript listeners registered for the onDefaultSearchEnginesChanged
  // event, then we want to register for change notifications.
  // Otherwise, we want to unregister and not be listening for changes.
  void StartOrStopListeningForSearchEnginesChanges();

  content::BrowserContext* context_;
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(SearchEnginesPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SEARCH_ENGINES_PRIVATE_SEARCH_ENGINES_PRIVATE_EVENT_ROUTER_H_
