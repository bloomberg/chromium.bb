// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var api = chrome.experienceSamplingPrivate;
var MAX_TIMEOUT = 5000;

// These listeners reflect the events back to the ExtensionApiTest.
api.onDecision.addListener(function(element, decision) {
  chrome.test.sendMessage("onDecision fired/element=" + element.name +
                          "/decision=" + decision.name);
});

api.onDisplayed.addListener(function(element) {
  chrome.test.sendMessage("onDisplayed fired/element=" + element.name);
});

// Timeout if the listener's never catch an event. This prevents the entire test
// from timing out, and let's us know that no event fired.
setTimeout(function() { chrome.test.sendMessage("timeout"); }, MAX_TIMEOUT);
