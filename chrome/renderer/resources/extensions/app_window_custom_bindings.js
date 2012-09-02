// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the app_window API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var appWindowNatives = requireNative('app_window');
var forEach = require('utils').forEach;
var GetView = appWindowNatives.GetView;

chromeHidden.registerCustomHook('app.window', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setCustomCallback('create', function(name, request, viewId) {
    var view = null;
    if (viewId)
      view = GetView(viewId);
    if (request.callback) {
      request.callback(view.chrome.app.window.current());
      delete request.callback;
    }
  })
  var AppWindow = function() {};
  forEach(chromeHidden.internalAPIs.app.currentWindowInternal, function(fn) {
    AppWindow.prototype[fn] =
        chromeHidden.internalAPIs.app.currentWindowInternal[fn];
  });
  AppWindow.prototype.moveTo = window.moveTo.bind(window);
  AppWindow.prototype.resizeTo = window.resizeTo.bind(window);
  AppWindow.prototype.dom = window;
  apiFunctions.setHandleRequest('current', function() {
    return new AppWindow;
  })
});
