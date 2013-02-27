// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileBrowserPrivate API.

var binding = require('binding').Binding.create('fileBrowserPrivate');

var fileBrowserPrivateNatives = requireNative('file_browser_private');
var GetLocalFileSystem = fileBrowserPrivateNatives.GetLocalFileSystem;

var fileBrowserNatives = requireNative('file_browser_handler');
var GetExternalFileEntry = fileBrowserNatives.GetExternalFileEntry;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('requestLocalFileSystem',
                                 function(name, request, response) {
    var fs = null;
    if (response && !response.error)
      fs = GetLocalFileSystem(response.name, response.path);
    if (request.callback)
      request.callback(fs);
    request.callback = null;
  });

  apiFunctions.setCustomCallback('searchDrive',
                                 function(name, request, response) {
    if (response && !response.error && response.results) {
      for (var i = 0; i < response.results.length; i++) {
        response.results[i].entry =
            GetExternalFileEntry(response.results[i].entry);
      }
    }

    // So |request.callback| doesn't break if response is not defined.
    if (!response)
      response = {};

    if (request.callback)
      request.callback(response.results, response.nextFeed);
    request.callback = null;
  });

  apiFunctions.setCustomCallback('searchDriveMetadata',
                                 function(name, request, response) {
    if (response && !response.error) {
      for (var i = 0; i < response.length; i++) {
        response[i].entry =
            GetExternalFileEntry(response[i].entry);
      }
    }

    // So |request.callback| doesn't break if response is not defined.
    if (!response)
      response = {};

    if (request.callback)
      request.callback(response);
    request.callback = null;
  });
});

exports.binding = binding.generate();
