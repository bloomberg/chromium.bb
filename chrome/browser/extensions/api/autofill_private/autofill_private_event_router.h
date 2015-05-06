// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_H_

#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace autofill {
class PersonalDataManager;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// An event router that observes changes to autofill addresses and credit cards
// and notifies listeners to the onAddressListChanged and
// onCreditCardListChanged events of changes.
class AutofillPrivateEventRouter :
    public KeyedService,
    public EventRouter::Observer,
    public autofill::PersonalDataManagerObserver {
 public:
  static AutofillPrivateEventRouter* Create(
      content::BrowserContext* browser_context);
  ~AutofillPrivateEventRouter() override;

 protected:
  explicit AutofillPrivateEventRouter(content::BrowserContext* context);

  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override;

 private:
  // Either listens or unlistens for changes to |personal_data_|, depending on
  // whether clients are listening to the autofillPrivate API events.
  void StartOrStopListeningForChanges();

  content::BrowserContext* context_;

  EventRouter* event_router_;

  autofill::PersonalDataManager* personal_data_;

  // Whether this class is currently listening for changes to |personal_data_|.
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_PRIVATE_EVENT_ROUTER_H_
