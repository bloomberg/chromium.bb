// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var shellNatives = requireNative('shell_natives');
var Binding = require('binding').Binding;
var forEach = require('utils').forEach;
var renderViewObserverNatives = requireNative('renderViewObserverNatives');

var currentAppWindow = null;

var shell = Binding.create('shell');
shell.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('createWindow',
                                 function(name, request, windowParams) {
    var view = null;

    // When window creation fails, |windowParams| will be undefined.
    if (windowParams && windowParams.viewId) {
      view = shellNatives.GetView(windowParams.viewId);
    }

    if (!view) {
      // No route to created window. If given a callback, trigger it with an
      // undefined object.
      if (request.callback) {
        request.callback(undefined);
        delete request.callback;
      }
      return;
    }

    // Initialize the app window in the newly created JS context
    view.chrome.shell.initializeAppWindow();

    var callback = request.callback;
    if (callback) {
      delete request.callback;

      var willCallback =
          renderViewObserverNatives.OnDocumentElementCreated(
              windowParams.viewId,
              function(success) {
                if (success) {
                  callback(view.chrome.shell.currentWindow());
                } else {
                  callback(undefined);
                }
              });
      if (!willCallback) {
        callback(undefined);
      }
    }
  });

  apiFunctions.setHandleRequest('currentWindow', function() {
    if (!currentAppWindow) {
      console.error(
          'The JavaScript context calling chrome.shell.currentWindow() has' +
          ' no associated AppWindow.');
      return null;
    }
    return currentAppWindow;
  });

  // This is an internal function, but needs to be bound into a closure
  // so the correct JS context is used for global variables such as
  // currentAppWindow.
  apiFunctions.setHandleRequest('initializeAppWindow', function() {
    var AppWindow = function() {};
    AppWindow.prototype.contentWindow = window;
    currentAppWindow = new AppWindow;
  });
});

exports.binding = shell.generate();
