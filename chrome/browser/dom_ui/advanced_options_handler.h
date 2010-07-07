// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"

#if defined(OS_MACOSX)
#include "chrome/browser/dom_ui/advanced_options_utils_mac.h"
#endif

// Chrome advanced options page UI handler.
class AdvancedOptionsHandler : public OptionsPageUIHandler {
 public:
  AdvancedOptionsHandler();
  virtual ~AdvancedOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
#if defined(OS_MACOSX)
  // Callback for the "showNetworkProxySettings" message. This will invoke
  // the Mac OS X Network panel for configuring proxy settings.
  void ShowNetworkProxySettings(const Value* value);

  // Callback for the "showManageSSLCertificates" message. This will invoke
  // the Mac OS X Keychain application for inspecting certificates.
  void ShowManageSSLCertificates(const Value* value);
#endif

  DISALLOW_COPY_AND_ASSIGN(AdvancedOptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_ADVANCED_OPTIONS_HANDLER_H_
