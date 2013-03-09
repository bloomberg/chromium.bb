// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webRequestInternal API.

var binding = require('binding').Binding.create('webRequestInternal');

var sendRequest = require('sendRequest').sendRequest;

binding.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('addEventListener', function() {
    var args = Array.prototype.slice.call(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });

  apiFunctions.setHandleRequest('eventHandled', function() {
    var args = Array.prototype.slice.call(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });
});

exports.binding = binding.generate();
