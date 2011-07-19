// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_EVENTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_EVENTS_H_
#pragma once

#include <string>

#include "base/basictypes.h"

// Static utility functions for dealing with extension devtools event names.
// The format of the event names is <prefix>.<tab id>.<event name>
// Equivalent name munging is done in the extension process in JavaScript
// by chrome/renderer/resources/extension_process_bindings.js
class ExtensionDevToolsEvents {
 public:
  // Checks if an event name is a magic devtools event name.  If so,
  // the tab id of the event is put in *tab_id.
  static bool IsDevToolsEventName(const std::string& event_name, int* tab_id);

  // Generates the event string for an onPageEvent for a given tab.
  static std::string OnPageEventNameForTab(int tab_id);

  // Generates the event string for an onTabCloseEvent for a given tab.
  static std::string OnTabCloseEventNameForTab(int tab_id);

 private:

  DISALLOW_IMPLICIT_CONSTRUCTORS(ExtensionDevToolsEvents);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DEVTOOLS_EVENTS_H_

