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
  apiFunctions.setCustomCallback('create',
                                 function(name, request, windowParams) {
    if (!windowParams.viewId) {
      // Create failed? If given a callback, trigger it with an undefined
      // object.
      if (request.callback) {
        request.callback()
        delete request.callback;
      }
      return;
    }

    var view = GetView(windowParams.viewId, windowParams.injectTitlebar);

    // Initialize appWindowData in the newly created JS context
    view.chrome.app.window.initializeAppWindow(windowParams);

    if (request.callback) {
      request.callback(view.chrome.app.window.current());
      delete request.callback;
    }
  });

  apiFunctions.setHandleRequest('current', function() {
    if (!chromeHidden.currentAppWindow) {
      console.error('chrome.app.window.current() is null -- window not ' +
                    'created with chrome.app.window.create()');
      return null;
    }
    return chromeHidden.currentAppWindow;
  });

  // This is an internal function, but needs to be bound with setHandleRequest
  // because it is called from a different JS context
  apiFunctions.setHandleRequest('initializeAppWindow', function(params) {
    var AppWindow = function() {};
    forEach(chromeHidden.internalAPIs.app.currentWindowInternal, function(fn) {
      AppWindow.prototype[fn] =
          chromeHidden.internalAPIs.app.currentWindowInternal[fn];
    });
    AppWindow.prototype.moveTo = window.moveTo.bind(window);
    AppWindow.prototype.resizeTo = window.resizeTo.bind(window);
    AppWindow.prototype.contentWindow = window;

    Object.defineProperty(AppWindow.prototype, 'id', {get: function() {
      return chromeHidden.appWindowData.id;
    }});

    chromeHidden.appWindowData = {
      id: params.id || ''
    };
    chromeHidden.currentAppWindow = new AppWindow;
  });
});
