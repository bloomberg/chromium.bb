// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Tab Capture API.

var binding = require('binding').Binding.create('tabCapture');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  function proxyToGetUserMedia(name, request, callback, response) {
    if (!callback)
      return;

    // TODO(miu): Propagate exceptions and always provide a useful error when
    // callback() is invoked with a null argument.  http://crbug.com/463679
    if (response) {
      var options = {};
      if (response.audioConstraints)
        options.audio = response.audioConstraints;
      if (response.videoConstraints)
        options.video = response.videoConstraints;

      try {
        navigator.webkitGetUserMedia(options,
                                     function(stream) { callback(stream); },
                                     function(exception) { callback(null); });
      } catch (e) {
        callback(null);
      }
    } else {
      callback(null);
    }
  }

  apiFunctions.setCustomCallback('capture', proxyToGetUserMedia);
  apiFunctions.setCustomCallback('captureOffscreenTab', proxyToGetUserMedia);
});

exports.binding = binding.generate();
