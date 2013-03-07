// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the app_window API.

var Binding = require('binding').Binding;
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var chrome = requireNative('chrome').GetChrome();
var sendRequest = require('sendRequest').sendRequest;
var appWindowNatives = requireNative('app_window');
var forEach = require('utils').forEach;
var GetView = appWindowNatives.GetView;
var OnContextReady = appWindowNatives.OnContextReady;

var appWindow = Binding.create('app.window');
appWindow.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('create',
                                 function(name, request, windowParams) {
    var view = null;
    if (windowParams.viewId)
      view = GetView(windowParams.viewId, windowParams.injectTitlebar);

    if (!view) {
      // No route to created window. If given a callback, trigger it with an
      // undefined object.
      if (request.callback) {
        request.callback();
        delete request.callback;
      }
      return;
    }

    if (windowParams.existingWindow) {
      // Not creating a new window, but activating an existing one, so trigger
      // callback with existing window and don't do anything else.
      if (request.callback) {
        request.callback(view.chrome.app.window.current());
        delete request.callback;
      }
      return;
    }

    // Initialize appWindowData in the newly created JS context
    view.chrome.app.window.initializeAppWindow(windowParams);

    var callback = request.callback;
    if (callback) {
      delete request.callback;
      if (!view) {
        callback(undefined);
        return;
      }

      var willCallback = OnContextReady(windowParams.viewId, function(success) {
        if (success) {
          callback(view.chrome.app.window.current());
        } else {
          callback(undefined);
        }
      });
      if (!willCallback) {
        callback(undefined);
      }
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

  chromeHidden.OnAppWindowClosed = function() {
    if (!chromeHidden.currentAppWindow)
      return;
    chromeHidden.currentAppWindow.onClosed.dispatch();
  };

  // This is an internal function, but needs to be bound with setHandleRequest
  // because it is called from a different JS context.
  apiFunctions.setHandleRequest('initializeAppWindow', function(params) {
    var currentWindowInternal =
        Binding.create('app.currentWindowInternal').generate();
    var AppWindow = function() {};
    forEach(currentWindowInternal, function(fn) {
      AppWindow.prototype[fn] =
          currentWindowInternal[fn];
    });
    AppWindow.prototype.moveTo = window.moveTo.bind(window);
    AppWindow.prototype.resizeTo = window.resizeTo.bind(window);
    AppWindow.prototype.contentWindow = window;
    AppWindow.prototype.onClosed = new chrome.Event;
    AppWindow.prototype.close = function() {
      this.contentWindow.close();
    };
    AppWindow.prototype.getBounds = function() {
      var bounds = chromeHidden.appWindowData.bounds;
      return { left: bounds.left, top: bounds.top,
               width: bounds.width, height: bounds.height };
    };
    AppWindow.prototype.isMinimized = function() {
      return chromeHidden.appWindowData.minimized;
    };
    AppWindow.prototype.isMaximized = function() {
      return chromeHidden.appWindowData.maximized;
    };

    Object.defineProperty(AppWindow.prototype, 'id', {get: function() {
      return chromeHidden.appWindowData.id;
    }});

    chromeHidden.appWindowData = {
      id: params.id || '',
      bounds: { left: params.bounds.left, top: params.bounds.top,
                width: params.bounds.width, height: params.bounds.height },
      minimized: false,
      maximized: false
    };
    chromeHidden.currentAppWindow = new AppWindow;
  });
});

function boundsEqual(bounds1, bounds2) {
  if (!bounds1 || !bounds2)
    return false;
  return (bounds1.left == bounds2.left && bounds1.top == bounds2.top &&
          bounds1.width == bounds2.width && bounds1.height == bounds2.height);
}

chromeHidden.updateAppWindowProperties = function(update) {
  if (!chromeHidden.appWindowData)
    return;
  var oldData = chromeHidden.appWindowData;
  update.id = oldData.id;
  chromeHidden.appWindowData = update;

  var currentWindow = chromeHidden.currentAppWindow;

  if (!boundsEqual(oldData.bounds, update.bounds))
    currentWindow["onBoundsChanged"].dispatch();

  if (!oldData.minimized && update.minimized)
    currentWindow["onMinimized"].dispatch();
  if (!oldData.maximized && update.maximized)
    currentWindow["onMaximized"].dispatch();

  if ((oldData.minimized && !update.minimized) ||
      (oldData.maximized && !update.maximized))
    currentWindow["onRestored"].dispatch();
};

exports.binding = appWindow.generate();
