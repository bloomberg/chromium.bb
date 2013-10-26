// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webrtc custom transport API.

var binding = require('binding').Binding.create('webrtc.castUdpTransport');
var webrtc = requireNative('webrtc_natives');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('create', function(callback) {
    webrtc.CreateCastUdpTransport(callback);
  });
  apiFunctions.setHandleRequest('destroy', function(transportId) {
    webrtc.DestroyCastUdpTransport(transportId);
  });
  apiFunctions.setHandleRequest('start',
      function(transportId, remoteParams) {
        webrtc.StartCastUdpTransport(transportId, remoteParams);
  });
  apiFunctions.setHandleRequest('stop', function(transportId) {
    webrtc.StopCastUdpTransport(transportId);
  });
});

exports.binding = binding.generate();
