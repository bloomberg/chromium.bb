// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// An event router that observes changes to saved passwords and password
// exceptions and notifies listeners to the onSavedPasswordsListChanged and
// onPasswordExceptionsListChanged events of changes.
class PasswordsPrivateEventRouter :
    public KeyedService,
    public EventRouter::Observer,
    public PasswordsPrivateDelegate::Observer {
 public:
  static PasswordsPrivateEventRouter* Create(
      content::BrowserContext* browser_context);
  ~PasswordsPrivateEventRouter() override;

 protected:
  explicit PasswordsPrivateEventRouter(content::BrowserContext* context);

  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // PasswordsPrivateDelegate::Observer overrides:
  void OnSavedPasswordsListChanged(const std::vector<linked_ptr<
      api::passwords_private::PasswordUiEntry>>& entries) override;
  void OnPasswordExceptionsListChanged(
      const std::vector<std::string>& exceptions) override;
  void OnPlaintextPasswordFetched(
      const std::string& origin_url,
      const std::string& username,
      const std::string& plaintext_password) override;

 private:
  // Either listens or unlistens for changes to saved passwords, password
  // exceptions, or the retrieval of plaintext passwords, depending on whether
  // clients are listening to the passwordsPrivate API events.
  void StartOrStopListeningForChanges();

  void SendSavedPasswordListToListeners();
  void SendPasswordExceptionListToListeners();

  content::BrowserContext* context_;

  EventRouter* event_router_;

  // Cached parameters which are saved so that when new listeners are added, the
  // most up-to-date lists can be sent to them immediately.
  scoped_ptr<base::ListValue> cached_saved_password_parameters_;
  scoped_ptr<base::ListValue> cached_password_exception_parameters_;

  // Whether this class is currently listening for changes to password changes.
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
