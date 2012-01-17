// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the fileBrowserPrivate API.

(function() {

native function GetChromeHidden();
native function GetLocalFileSystem(name, path);

var chromeHidden = GetChromeHidden();

chromeHidden.registerCustomHook('fileBrowserPrivate', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback(
      "fileBrowserPrivate.requestLocalFileSystem",
      function(name, request, response) {
    var resp = response ? [chromeHidden.JSON.parse(response)] : [];
    var fs = null;
    if (!resp[0].error)
      fs = GetLocalFileSystem(resp[0].name, resp[0].path);
    if (request.callback)
      request.callback(fs);
    request.callback = null;
  });
});

})();
