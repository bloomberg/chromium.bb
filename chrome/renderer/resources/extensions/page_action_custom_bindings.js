// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the pageAction API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var setIcon = require('setIcon').setIcon;

chromeHidden.registerCustomHook('pageAction', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, callback, this.name, this.definition.parameters,
        'page action');
  });
});
