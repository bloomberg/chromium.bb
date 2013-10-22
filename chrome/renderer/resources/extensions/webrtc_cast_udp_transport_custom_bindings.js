// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webrtc custom transport API.

var binding = require('binding').Binding.create('webrtc.castUdpTransport');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('create',
                                function(callback) {
    // invoke impl here.
  });
});

exports.binding = binding.generate();
