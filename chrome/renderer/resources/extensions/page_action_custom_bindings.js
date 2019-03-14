// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the pageAction API.

var setIcon = require('setIcon').setIcon;
var sendRequest = bindingUtil ?
    $Function.bind(bindingUtil.sendRequest, bindingUtil) :
    require('sendRequest').sendRequest;

apiBridge.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, $Function.bind(function(args) {
      sendRequest('pageAction.setIcon', [args, callback],
                  bindingUtil ? undefined : this.definition.parameters,
                  undefined);
    }, this));
  });
});
