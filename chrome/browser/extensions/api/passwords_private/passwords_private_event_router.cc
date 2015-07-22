// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

PasswordsPrivateEventRouter::PasswordsPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context),
      event_router_(nullptr),
      listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  event_router_ = EventRouter::Get(context_);
  if (!event_router_)
    return;

  event_router_->RegisterObserver(
      this,
      api::passwords_private::OnSavedPasswordsListChanged::kEventName);
  event_router_->RegisterObserver(
      this,
      api::passwords_private::OnPasswordExceptionsListChanged::kEventName);
  event_router_->RegisterObserver(
      this,
      api::passwords_private::OnPlaintextPasswordRetrieved::kEventName);
}

PasswordsPrivateEventRouter::~PasswordsPrivateEventRouter() {}

void PasswordsPrivateEventRouter::Shutdown() {
  if (event_router_)
    event_router_->UnregisterObserver(this);

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(context_,
                                                            false /* create */);
  if (delegate)
    delegate->RemoveObserver(this);
}

void PasswordsPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to change events and propagate the original lists to
  // listeners.
  StartOrStopListeningForChanges();
  SendSavedPasswordListToListeners();
  SendPasswordExceptionListToListeners();
}

void PasswordsPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events if there are no more listeners.
  StartOrStopListeningForChanges();
}

void PasswordsPrivateEventRouter::OnSavedPasswordsListChanged(
    const std::vector<linked_ptr<
        api::passwords_private::PasswordUiEntry>>& entries) {
  cached_saved_password_parameters_ =
      api::passwords_private::OnSavedPasswordsListChanged::Create(entries);
  SendSavedPasswordListToListeners();
}

void PasswordsPrivateEventRouter::SendSavedPasswordListToListeners() {
  if (!cached_saved_password_parameters_.get())
    // If there is nothing to send, return early.
    return;

  scoped_ptr<Event> extension_event(
      new Event(events::PASSWORDS_PRIVATE_ON_SAVED_PASSWORDS_LIST_CHANGED,
                api::passwords_private::OnSavedPasswordsListChanged::kEventName,
                cached_saved_password_parameters_->CreateDeepCopy()));
  event_router_->BroadcastEvent(extension_event.Pass());
}

void PasswordsPrivateEventRouter::OnPasswordExceptionsListChanged(
      const std::vector<std::string>& exceptions) {
  cached_password_exception_parameters_ =
      api::passwords_private::OnPasswordExceptionsListChanged::Create(
          exceptions);
  SendPasswordExceptionListToListeners();
}

void PasswordsPrivateEventRouter::SendPasswordExceptionListToListeners() {
  if (!cached_password_exception_parameters_.get())
    // If there is nothing to send, return early.
    return;

  scoped_ptr<Event> extension_event(new Event(
      events::PASSWORDS_PRIVATE_ON_PASSWORD_EXCEPTIONS_LIST_CHANGED,
      api::passwords_private::OnPasswordExceptionsListChanged::kEventName,
      cached_password_exception_parameters_->CreateDeepCopy()));
  event_router_->BroadcastEvent(extension_event.Pass());
}

void PasswordsPrivateEventRouter::OnPlaintextPasswordFetched(
        const std::string& origin_url,
        const std::string& username,
        const std::string& plaintext_password) {
  api::passwords_private::PlaintextPasswordEventParameters params;
  params.login_pair.origin_url = origin_url;
  params.login_pair.username = username;
  params.plaintext_password = plaintext_password;

  scoped_ptr<base::ListValue> event_value(new base::ListValue);
  event_value->Append(params.ToValue());

  scoped_ptr<Event> extension_event(new Event(
      events::PASSWORDS_PRIVATE_ON_PLAINTEXT_PASSWORD_RETRIEVED,
      api::passwords_private::OnPlaintextPasswordRetrieved::kEventName,
      event_value.Pass()));
  event_router_->BroadcastEvent(extension_event.Pass());
}

void PasswordsPrivateEventRouter::StartOrStopListeningForChanges() {
  bool should_listen_for_saved_password_changes =
      event_router_->HasEventListener(
          api::passwords_private::OnSavedPasswordsListChanged::kEventName);
  bool should_listen_for_password_exception_changes =
      event_router_->HasEventListener(
          api::passwords_private::OnPasswordExceptionsListChanged::kEventName);
  bool should_listen_for_plaintext_password_retrieval =
      event_router_->HasEventListener(
          api::passwords_private::OnPlaintextPasswordRetrieved::kEventName);
  bool should_listen = should_listen_for_saved_password_changes ||
      should_listen_for_password_exception_changes ||
      should_listen_for_plaintext_password_retrieval;

  PasswordsPrivateDelegate* delegate =
      PasswordsPrivateDelegateFactory::GetForBrowserContext(context_,
                                                            true /* create */);
  if (should_listen && !listening_)
    delegate->AddObserver(this);
  else if (!should_listen && listening_)
    delegate->RemoveObserver(this);

  listening_ = should_listen;
}

PasswordsPrivateEventRouter* PasswordsPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new PasswordsPrivateEventRouter(context);
}

}  // namespace extensions
