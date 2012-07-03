// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the runtime API.

var runtimeNatives = requireNative('runtime');
var extensionNatives = requireNative('extension');
var GetExtensionViews = extensionNatives.GetExtensionViews;
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('runtime', function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('getBackgroundPage',
                                 function(name, request, response) {
    if (request.callback) {
      var bg = GetExtensionViews(-1, 'BACKGROUND')[0] || null;
      request.callback(bg);
    }
    request.callback = null;
  });

  apiFunctions.setHandleRequest('getManifest', function() {
    return runtimeNatives.GetManifest();
  });

  apiFunctions.setHandleRequest('getURL', function(path) {
    path = String(path);
    if (!path.length || path[0] != '/')
      path = '/' + path;
    return 'chrome-extension://' + extensionId + path;
  });

});
