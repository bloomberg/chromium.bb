// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_EXTENSION_PROXY_H_
#define CHROME_TEST_AUTOMATION_EXTENSION_PROXY_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/automation/automation_handle_tracker.h"

class AutomationMessageSender;
class BrowserProxy;

// This class presents the interface to actions that can be performed on
// a given extension. This refers to a particular version of that extension.
// For example, it refers to Google Translate 1.0. If the extension is
// updated to a newer version, this proxy is invalidated.
class ExtensionProxy : public AutomationResourceProxy {
 public:
  // Creates an extension proxy referring to an extension id.
  ExtensionProxy(AutomationMessageSender* sender,
                 AutomationHandleTracker* tracker,
                 int handle);

  // Uninstalls this extension. Returns true on success.
  bool Uninstall() WARN_UNUSED_RESULT;

  // Enables this extension. Returns true on success. The extension
  // should be disabled when this is called.
  bool Enable() WARN_UNUSED_RESULT;

  // Disables this extension. Returns true on success. The extension
  // should be enabled when this is called.
  bool Disable() WARN_UNUSED_RESULT;

  // Executes the action associated with this extension. This may be a page
  // action or a browser action. This is similar to clicking, but does not
  // work with popups. Also, for page actions, this will execute the action
  // even if the page action is not shown for the active tab. Returns true on
  // success.
  // TODO(kkania): Add support for popups.
  bool ExecuteActionInActiveTabAsync(BrowserProxy* browser)
      WARN_UNUSED_RESULT;

  // Moves the browser action from its current location in the browser action
  // toolbar to a new |index|. Index should be less than the number of browser
  // actions in the toolbar. Returns true on success.
  bool MoveBrowserAction(int index) WARN_UNUSED_RESULT;

  // Gets the id of this extension. Returns true on success.
  bool GetId(std::string* id) WARN_UNUSED_RESULT;

  // Gets the name of this extension. Returns true on success.
  bool GetName(std::string* name) WARN_UNUSED_RESULT;

  // Gets the version string of this extension. Returns true on success.
  bool GetVersion(std::string* version) WARN_UNUSED_RESULT;

  // Gets the index (zero-based) of this extension's browser action in
  // the browser action toolbar. |index| will be set to -1 if the extension
  // does not have a browser action in the toolbar. Returns true on success.
  bool GetBrowserActionIndex(int* index) WARN_UNUSED_RESULT;

 private:
  // Gets the string value of the property of type |type|. Returns true on
  // success.
  bool GetProperty(AutomationMsg_ExtensionProperty type, std::string* value)
      WARN_UNUSED_RESULT;
};

#endif  // CHROME_TEST_AUTOMATION_EXTENSION_PROXY_H_
