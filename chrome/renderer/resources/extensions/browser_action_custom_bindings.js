// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the browserAction API.

var binding = require('binding').Binding.create('browserAction');

var setIcon = require('setIcon').setIcon;
var getExtensionViews = requireNative('runtime').GetExtensionViews;
var sendRequest = require('sendRequest').sendRequest;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, function(args) {
      sendRequest(this.name, [args, callback], this.definition.parameters);
    }.bind(this));
  });

  apiFunctions.setCustomCallback('openPopup',
                                 function(name, request, response) {
    if (!request.callback)
      return;

    if (chrome.runtime.lastError) {
      request.callback();
    } else {
      var views = getExtensionViews(-1, 'POPUP');
      request.callback(views.length > 0 ? views[0] : null);
    }
    request.callback = null;
  });
});

exports.binding = binding.generate();
