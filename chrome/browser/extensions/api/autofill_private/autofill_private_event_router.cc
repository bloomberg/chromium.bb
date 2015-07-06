// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/browser_context.h"

using AddressEntryList = std::vector<linked_ptr<
    extensions::api::autofill_private::AddressEntry> >;
using CreditCardEntryList = std::vector<linked_ptr<
    extensions::api::autofill_private::CreditCardEntry> >;

namespace extensions {

AutofillPrivateEventRouter::AutofillPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context),
      event_router_(nullptr),
      personal_data_(nullptr),
      listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  event_router_ = EventRouter::Get(context_);
  if (!event_router_)
    return;

  personal_data_ = autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  if (!personal_data_)
    return;

  event_router_->RegisterObserver(
      this,
      api::autofill_private::OnAddressListChanged::kEventName);
  event_router_->RegisterObserver(
      this,
      api::autofill_private::OnCreditCardListChanged::kEventName);
  StartOrStopListeningForChanges();
}

AutofillPrivateEventRouter::~AutofillPrivateEventRouter() {
}

void AutofillPrivateEventRouter::Shutdown() {
  if (event_router_)
    event_router_->UnregisterObserver(this);
}

void AutofillPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to change events and propagate the original lists to
  // listeners.
  StartOrStopListeningForChanges();
  OnPersonalDataChanged();
}

void AutofillPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events if there are no more listeners.
  StartOrStopListeningForChanges();
}

void AutofillPrivateEventRouter::OnPersonalDataChanged() {
  DCHECK(personal_data_ && personal_data_->IsDataLoaded());

  scoped_ptr<AddressEntryList> addressList =
      extensions::autofill_util::GenerateAddressList(*personal_data_);
  scoped_ptr<base::ListValue> args(
      api::autofill_private::OnAddressListChanged::Create(*addressList)
      .release());
  scoped_ptr<Event> extension_event(new Event(
      events::AUTOFILL_PRIVATE_ON_ADDRESS_LIST_CHANGED,
      api::autofill_private::OnAddressListChanged::kEventName, args.Pass()));
  event_router_->BroadcastEvent(extension_event.Pass());

  scoped_ptr<CreditCardEntryList> creditCardList =
      extensions::autofill_util::GenerateCreditCardList(*personal_data_);
  args.reset(
      api::autofill_private::OnCreditCardListChanged::Create(*creditCardList)
      .release());
  extension_event.reset(new Event(
      events::AUTOFILL_PRIVATE_ON_CREDIT_CARD_LIST_CHANGED,
      api::autofill_private::OnCreditCardListChanged::kEventName, args.Pass()));
  event_router_->BroadcastEvent(extension_event.Pass());
}

void AutofillPrivateEventRouter::StartOrStopListeningForChanges() {
  if (!personal_data_ || !personal_data_->IsDataLoaded())
    return;

  bool should_listen_for_address_changes = event_router_->HasEventListener(
      api::autofill_private::OnAddressListChanged::kEventName);
  bool should_listen_for_credit_card_changes = event_router_->HasEventListener(
      api::autofill_private::OnCreditCardListChanged::kEventName);
  bool should_listen = should_listen_for_address_changes ||
      should_listen_for_credit_card_changes;

  if (should_listen && !listening_)
    personal_data_->AddObserver(this);
  else if (!should_listen && listening_)
    personal_data_->RemoveObserver(this);

  listening_ = should_listen;
}

AutofillPrivateEventRouter* AutofillPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new AutofillPrivateEventRouter(context);
}

}  // namespace extensions
