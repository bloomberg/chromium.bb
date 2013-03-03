// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Tab Capture API.
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('tabCapture',
                                function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('capture',
                                 function(name, request, response) {
    if (response && request.callback) {
      var callback = request.callback;
      var successFunc = function(stream) {
        callback(stream);
      };
      var errorFunc = function() {
        callback(null);
      };

      var options = {};
      if (response.audioConstraints)
        options.audio = response.audioConstraints;
      if (response.videoConstraints)
        options.video = response.videoConstraints;

      navigator.webkitGetUserMedia(options, successFunc, errorFunc);
    } else {
      request.callback();
    }
    request.callback = null;
  });
});
