// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the pageActions API.

var pageActionsNatives = requireNative('page_actions');
var GetCurrentPageActions = pageActionsNatives.GetCurrentPageActions;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('pageActions',
                                function(bindingsAPI, extensionId) {
  var pageActions = GetCurrentPageActions(extensionId);
  var oldStyleEventName = 'pageActions';
  for (var i = 0; i < pageActions.length; ++i) {
    // Setup events for each extension_id/page_action_id string we find.
    chrome.pageActions[pageActions[i]] = new chrome.Event(oldStyleEventName);
  }
});
