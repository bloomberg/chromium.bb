// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the app_window API.

var appWindowNatives = requireNative('app_window_natives');
var runtimeNatives = requireNative('runtime');
var Binding = require('binding').Binding;
var Event = require('event_bindings').Event;
var forEach = require('utils').forEach;
var renderViewObserverNatives = requireNative('renderViewObserverNatives');

var appWindowData = null;
var currentAppWindow = null;

var appWindow = Binding.create('app.window');
appWindow.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('create',
                                 function(name, request, windowParams) {
    var view = null;
    if (windowParams.viewId) {
      view = appWindowNatives.GetView(
          windowParams.viewId, windowParams.injectTitlebar);
    }

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

      var willCallback =
          renderViewObserverNatives.OnDocumentElementCreated(
              windowParams.viewId,
              function(success) {
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
    if (!currentAppWindow) {
      console.error('The JavaScript context calling ' +
                    'chrome.app.window.current() has no associated AppWindow.');
      return null;
    }
    return currentAppWindow;
  });

  apiFunctions.setHandleRequest('getAll', function() {
    var views = runtimeNatives.GetExtensionViews(-1, 'SHELL');
    return $Array.map(views, function(win) {
      return win.chrome.app.window.current();
    });
  });

  apiFunctions.setHandleRequest('get', function(id) {
    var windows = $Array.filter(chrome.app.window.getAll(), function(win) {
      return win.id == id;
    });
    return windows.length > 0 ? windows[0] : null;
  });

  // This is an internal function, but needs to be bound with setHandleRequest
  // because it is called from a different JS context.
  apiFunctions.setHandleRequest('initializeAppWindow', function(params) {
    var currentWindowInternal =
        Binding.create('app.currentWindowInternal').generate();
    var AppWindow = function() {};
    forEach(currentWindowInternal, function(key, value) {
      AppWindow.prototype[key] = value;
    });
    AppWindow.prototype.moveTo = $Function.bind(window.moveTo, window);
    AppWindow.prototype.resizeTo = $Function.bind(window.resizeTo, window);
    AppWindow.prototype.contentWindow = window;
    AppWindow.prototype.onClosed = new Event();
    AppWindow.prototype.close = function() {
      this.contentWindow.close();
    };
    AppWindow.prototype.getBounds = function() {
      var bounds = appWindowData.bounds;
      return { left: bounds.left, top: bounds.top,
               width: bounds.width, height: bounds.height };
    };
    AppWindow.prototype.getMinWidth = function() {
      return appWindowData.minWidth;
    };
    AppWindow.prototype.getMinHeight = function() {
      return appWindowData.minHeight;
    };
    AppWindow.prototype.getMaxWidth = function() {
      return appWindowData.maxWidth;
    };
    AppWindow.prototype.getMaxHeight = function() {
      return appWindowData.maxHeight;
    };
    AppWindow.prototype.isFullscreen = function() {
      return appWindowData.fullscreen;
    };
    AppWindow.prototype.isMinimized = function() {
      return appWindowData.minimized;
    };
    AppWindow.prototype.isMaximized = function() {
      return appWindowData.maximized;
    };
    AppWindow.prototype.isAlwaysOnTop = function() {
      return appWindowData.alwaysOnTop;
    };

    Object.defineProperty(AppWindow.prototype, 'id', {get: function() {
      return appWindowData.id;
    }});

    appWindowData = {
      id: params.id || '',
      bounds: { left: params.bounds.left, top: params.bounds.top,
                width: params.bounds.width, height: params.bounds.height },
      minWidth: params.minWidth,
      minHeight: params.minHeight,
      maxWidth: params.maxWidth,
      maxHeight: params.maxHeight,
      fullscreen: params.fullscreen,
      minimized: params.minimized,
      maximized: params.maximized,
      alwaysOnTop: params.alwaysOnTop
    };
    currentAppWindow = new AppWindow;
  });
});

function boundsEqual(bounds1, bounds2) {
  if (!bounds1 || !bounds2)
    return false;
  return (bounds1.left == bounds2.left && bounds1.top == bounds2.top &&
          bounds1.width == bounds2.width && bounds1.height == bounds2.height);
}

function dispatchEventIfExists(target, name) {
  // Sometimes apps like to put their own properties on the window which
  // break our assumptions.
  var event = target[name];
  if (event && (typeof event.dispatch == 'function'))
    event.dispatch();
  else
    console.warn('Could not dispatch ' + name + ', event has been clobbered');
}

function updateAppWindowProperties(update) {
  if (!appWindowData)
    return;

  var oldData = appWindowData;
  update.id = oldData.id;
  appWindowData = update;

  var currentWindow = currentAppWindow;

  if (!boundsEqual(oldData.bounds, update.bounds))
    dispatchEventIfExists(currentWindow, "onBoundsChanged");

  if (!oldData.fullscreen && update.fullscreen)
    dispatchEventIfExists(currentWindow, "onFullscreened");
  if (!oldData.minimized && update.minimized)
    dispatchEventIfExists(currentWindow, "onMinimized");
  if (!oldData.maximized && update.maximized)
    dispatchEventIfExists(currentWindow, "onMaximized");

  if ((oldData.fullscreen && !update.fullscreen) ||
      (oldData.minimized && !update.minimized) ||
      (oldData.maximized && !update.maximized))
    dispatchEventIfExists(currentWindow, "onRestored");
};

function onAppWindowClosed() {
  if (!currentAppWindow)
    return;
  dispatchEventIfExists(currentAppWindow, "onClosed");
}

exports.binding = appWindow.generate();
exports.onAppWindowClosed = onAppWindowClosed;
exports.updateAppWindowProperties = updateAppWindowProperties;
