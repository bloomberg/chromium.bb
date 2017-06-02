// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Cast Streaming Session API.

var binding =
    apiBridge ||
    require('binding').Binding.create('cast.streaming.receiverSession');
var natives = requireNative('cast_streaming_natives');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequest(
      'createAndBind',
      function(ap, vp, local, weidgth, height, fr, url, cb, op) {
        natives.StartCastRtpReceiver(
            ap, vp, local, weidgth, height, fr, url, cb, op);
  });
});

if (!apiBridge)
  exports.$set('binding', binding.generate());
