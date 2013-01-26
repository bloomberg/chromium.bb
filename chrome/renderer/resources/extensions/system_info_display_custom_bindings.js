// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Bluetooth API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var lastError = require('lastError');

// Use custom bindings to create an undocumented event listener that will
// receive events about device discovery and call the event listener that was
// provided with the request to begin discovery.
chromeHidden.registerCustomHook('systemInfo.display', function(api) {
  chromeHidden.display = {};
  chromeHidden.display.onDisplayChanged =
      new chrome.Event("systemInfo.display.onDisplayChanged");
});
