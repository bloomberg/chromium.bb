// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the pageActions API.

var binding = require('binding').Binding.create('pageActions');

var pageActionsNatives = requireNative('page_actions');
var GetCurrentPageActions = pageActionsNatives.GetCurrentPageActions;

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var pageActions = GetCurrentPageActions(extensionId);
  var pageActionsApi = bindingsAPI.compiledApi;
  var oldStyleEventName = 'pageActions';
  for (var i = 0; i < pageActions.length; ++i) {
    // Setup events for each extension_id/page_action_id string we find.
    pageActionsApi[pageActions[i]] = new chrome.Event(oldStyleEventName);
  }
});

exports.binding = binding.generate();
