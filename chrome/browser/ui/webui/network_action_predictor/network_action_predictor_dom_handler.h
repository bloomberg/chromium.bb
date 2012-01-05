// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NETWORK_ACTION_PREDICTOR_NETWORK_ACTION_PREDICTOR_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NETWORK_ACTION_PREDICTOR_NETWORK_ACTION_PREDICTOR_DOM_HANDLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

class NetworkActionPredictor;
class Profile;

// The handler for Javascript messages for about:network-action-predictor.
class NetworkActionPredictorDOMHandler : public content::WebUIMessageHandler {
 public:
  explicit NetworkActionPredictorDOMHandler(Profile* profile);
  virtual ~NetworkActionPredictorDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Synchronously fetches the database from NetworkActionPredictor and calls
  // into JS with the resulting DictionaryValue.
  void RequestNetworkActionPredictorDb(const base::ListValue* args);

  NetworkActionPredictor* network_action_predictor_;

  DISALLOW_COPY_AND_ASSIGN(NetworkActionPredictorDOMHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NETWORK_ACTION_PREDICTOR_NETWORK_ACTION_PREDICTOR_DOM_HANDLER_H_
