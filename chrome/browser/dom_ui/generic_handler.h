// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_GENERIC_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_GENERIC_HANDLER_H_
#pragma once

#include "chrome/browser/dom_ui/dom_ui.h"

class ListValue;

// A place to add handlers for messages shared across all DOMUI pages.
class GenericHandler : public WebUIMessageHandler {
 public:
  GenericHandler();
  virtual ~GenericHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void HandleNavigateToUrl(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(GenericHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_GENERIC_HANDLER_H_
