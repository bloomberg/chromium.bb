// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the webRequestInternal API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;

chromeHidden.registerCustomHook('webRequestInternal', function(api) {
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
