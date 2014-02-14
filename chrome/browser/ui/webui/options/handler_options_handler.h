// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_HANDLER_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_HANDLER_OPTIONS_HANDLER_H_

#include <string>

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

////////////////////////////////////////////////////////////////////////////////
// HandlerOptionsHandler

// Listen for changes to protocol handlers (i.e. registerProtocolHandler()).
// This get triggered whenever a user allows a specific website or application
// to handle clicks on a link with a specified protocol (i.e. mailto: -> Gmail).

namespace base {
class DictionaryValue;
}

namespace options {

class HandlerOptionsHandler : public OptionsPageUIHandler,
                              public content::NotificationObserver {
 public:
  HandlerOptionsHandler();
  virtual ~HandlerOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Called when the user toggles whether custom handlers are enabled.
  void SetHandlersEnabled(const base::ListValue* args);

  // Called when the user sets a new default handler for a protocol.
  void SetDefault(const base::ListValue* args);

  // Called when the user clears the default handler for a protocol.
  // |args| is the string name of the protocol to clear.
  void ClearDefault(const base::ListValue* args);

  // Parses a ProtocolHandler out of the arguments passed back from the view.
  // |args| is a list of [protocol, url, title].
  ProtocolHandler ParseHandlerFromArgs(const base::ListValue* args) const;

  // Returns a JSON object describing the set of protocol handlers for the
  // given protocol.
  void GetHandlersForProtocol(const std::string& protocol,
                              base::DictionaryValue* value);

  // Returns a JSON list of the ignored protocol handlers.
  void GetIgnoredHandlers(base::ListValue* handlers);

  // Called when the JS PasswordManager object is initialized.
  void UpdateHandlerList();

  // Remove a handler.
  // |args| is a list of [protocol, url, title].
  void RemoveHandler(const base::ListValue* args);

  // Remove an ignored handler.
  // |args| is a list of [protocol, url, title].
  void RemoveIgnoredHandler(const base::ListValue* args);

  ProtocolHandlerRegistry* GetProtocolHandlerRegistry();

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(HandlerOptionsHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_HANDLER_OPTIONS_HANDLER_H_
