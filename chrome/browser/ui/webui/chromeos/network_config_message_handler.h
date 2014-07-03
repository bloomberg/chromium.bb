// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_CONFIG_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_CONFIG_MESSAGE_HANDLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

// This class provides support for network configuration from WebUI components.
// It implements network_config.js which is a drop-in replacement for the
// networkingPrivate extention API. TODO(stevenjb): Implement the remaining
// networkingPrivate methods as needed.
class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkConfigMessageHandler();
  virtual ~NetworkConfigMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // WebUI::RegisterMessageCallback callbacks. These callbacks collect the
  // requested information and call the associated JS method. The first
  // argument in |arg_list| is always the callback id which is passed back
  // to the callback method.
  void GetNetworks(const base::ListValue* arg_list) const;
  void GetProperties(const base::ListValue* arg_list);
  void GetManagedProperties(const base::ListValue* arg_list);
  void GetPropertiesSuccess(int callback_id,
                            const std::string& service_path,
                            const base::DictionaryValue& dictionary) const;
  void InvokeCallback(const base::ListValue& arg_list) const;
  void ErrorCallback(int callback_id,
                     const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) const;

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigMessageHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_CONFIG_MESSAGE_HANDLER_H_
