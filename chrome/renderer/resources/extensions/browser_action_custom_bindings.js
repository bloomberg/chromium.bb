// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the browserAction API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var setIcon = require('setIcon').setIcon;

chromeHidden.registerCustomHook('browserAction', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, callback, this.name, this.definition.parameters,
        'browser action');
  });
});
