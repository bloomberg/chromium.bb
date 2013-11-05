// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webrtc custom transport API.

var binding = require('binding').Binding.create('webrtc.castSendTransport');
var webrtc = requireNative('webrtc_natives');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('create',
      function(innerTransportId, track, callback) {
        webrtc.CreateCastSendTransport(innerTransportId, track, callback);
  });
  apiFunctions.setHandleRequest('destroy',
      function(transportId) {
        webrtc.DestroyCastSendTransport(transportId);
  });
  apiFunctions.setHandleRequest('getCaps',
      function(transportId, callback) {
        webrtc.GetCapsCastSendTransport(transportId, callback);
  });
  apiFunctions.setHandleRequest('createParams',
      function(transportId, remoteCaps, callback) {
        webrtc.CreateParamsCastSendTransport(transportId, remoteCaps,
                                             callback);
  });
  apiFunctions.setHandleRequest('start',
      function(transportId, params) {
        webrtc.StartCastSendTransport(transportId, params);
  });
  apiFunctions.setHandleRequest('stop',
      function(transportId) {
        webrtc.StopCastSendTransport(transportId);
  });
});

exports.binding = binding.generate();
