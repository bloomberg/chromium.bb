// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/dom_ui/options_ui.h"

class SkBitmap;
namespace views {
class WindowDelegate;
}

// ChromeOS internet options page UI handler.
class InternetOptionsHandler : public OptionsPageUIHandler,
                               public chromeos::NetworkLibrary::Observer {
 public:
  InternetOptionsHandler();
  virtual ~InternetOptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(chromeos::NetworkLibrary* obj);
  virtual void NetworkTraffic(chromeos::NetworkLibrary* cros,
                              int traffic_type) {}

 private:
  // Open a modal popup dialog.
  void CreateModalPopup(views::WindowDelegate* view);
  // Open options dialog for network.
  // |value| will be [ network_type, service_path, command ]
  // And command is one of 'options', 'connect', disconnect', or 'forget'
  void ButtonClickCallback(const Value* value);

    // Creates the map of a network
  ListValue* GetNetwork(const std::string& service_path, const SkBitmap& icon,
      const std::string& name, bool connecting, bool connected,
      int connection_type, bool remembered);

  // Creates the map of wired networks
  ListValue* GetWiredList();
  // Creates the map of wireless networks
  ListValue* GetWirelessList();
  // Creates the map of remembered networks
  ListValue* GetRememberedList();

  DISALLOW_COPY_AND_ASSIGN(InternetOptionsHandler);
};

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_INTERNET_OPTIONS_HANDLER_H_
