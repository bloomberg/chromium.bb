// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the browserAction API.

var binding = apiBridge || require('binding').Binding.create('browserAction');

var setIcon = require('setIcon').setIcon;
var getExtensionViews = requireNative('runtime').GetExtensionViews;
var sendRequest = apiBridge ?
    apiBridge.sendRequest.bind(apiBridge) : require('sendRequest').sendRequest;
var lastError = require('lastError');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, function(args) {
      sendRequest('browserAction.setIcon',
                  [args, callback],
                  apiBridge ? undefined : this.definition.parameters);
    }.bind(this));
  });

  apiFunctions.setCustomCallback('openPopup',
      function(name, request, callback, response) {
    if (!callback)
      return;

    if (lastError.hasError(chrome)) {
      callback();
    } else {
      var views = getExtensionViews(-1, -1, 'POPUP');
      callback(views.length > 0 ? views[0] : null);
    }
  });
});

if (!apiBridge)
  exports.$set('binding', binding.generate());
