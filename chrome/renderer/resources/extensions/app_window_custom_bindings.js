// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the app_window API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var appWindowNatives = requireNative('app_window');

chromeHidden.registerCustomHook('appWindow', function() {
  var internal_appWindow_create = chrome.appWindow.create;
  chrome.appWindow.create = function(url, opts, cb) {
    internal_appWindow_create(url, opts, function(view_id) {
      var dom = appWindowNatives.GetView(view_id);
      cb(dom);
    });
  };
});
