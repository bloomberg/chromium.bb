// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the GCM API.

var binding = require('binding').Binding.create('gcm');
var forEach = require('utils').forEach;

binding.registerCustomHook(function(api) {
  var gcm = api.compiledApi;
  var apiFunctions = api.apiFunctions;

  apiFunctions.setUpdateArgumentsPostValidate(
    'send', function (message, callback) {
      var payloadSize = 0;
      forEach(message.data, function(property, value) {
        // Issue error for forbidden prefixes of property names.
        if (property.startsWith("goog.") || property.startsWith("google"))
          throw new Error("Invalid key: " + property);

        payloadSize += property.length + value.length;
      });

      // Issue error for messages larger than allowed limit.
      if (payloadSize > gcm.MAX_MESSAGE_SIZE)
        throw new Error("Payload exceeded size limit");
    });
});

exports.binding = binding.generate();
