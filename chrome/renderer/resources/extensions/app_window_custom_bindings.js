// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the app_window API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var appWindowNatives = requireNative('app_window');
var GetView = appWindowNatives.GetView;

chromeHidden.registerCustomHook('appWindow', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setCustomCallback('create', function(name, request, viewId) {
    var view = null;
    if (viewId)
      view = GetView(viewId);
    if (request.callback) {
      request.callback(view);
      delete request.callback;
    }
  })
  apiFunctions.setHandleRequest('moveTo', function(x, y) {
    window.moveTo(x, y);
  })
  apiFunctions.setHandleRequest('resizeTo', function(width, height) {
    window.resizeTo(width, height);
  })
});
