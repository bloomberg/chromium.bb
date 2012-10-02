// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the syncFileSystem API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var syncFileSystemNatives = requireNative('sync_file_system');

chromeHidden.registerCustomHook('syncFileSystem', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setCustomCallback('requestFileSystem',
                                 function(name, request, response) {
    var result = null;
    if (response) {
      result = syncFileSystemNatives.GetSyncFileSystemObject(
          response.name, response.root);
    }
    if (request.callback)
      request.callback(result);
    request.callback = null;
  });
});
