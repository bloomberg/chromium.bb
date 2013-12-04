// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileBrowserPrivate API.

var binding = require('binding').Binding.create('fileBrowserPrivate');

var eventBindings = require('event_bindings');
var fileBrowserPrivateNatives = requireNative('file_browser_private');
var GetFileSystem = fileBrowserPrivateNatives.GetFileSystem;

var fileBrowserNatives = requireNative('file_browser_handler');
var GetExternalFileEntry = fileBrowserNatives.GetExternalFileEntry;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('requestFileSystem',
                                 function(name, request, response) {
    var fs = null;
    if (response && !response.error)
      fs = GetFileSystem(response.name, response.root_url);
    if (request.callback)
      request.callback(fs);
    request.callback = null;
  });

  apiFunctions.setCustomCallback('searchDrive',
                                 function(name, request, response) {
    if (response && !response.error && response.entries) {
      response.entries = response.entries.map(function(entry) {
        return GetExternalFileEntry(entry);
      });
    }

    // So |request.callback| doesn't break if response is not defined.
    if (!response)
      response = {};

    if (request.callback)
      request.callback(response.entries, response.nextFeed);
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

eventBindings.registerArgumentMassager(
    'fileBrowserPrivate.onDirectoryChanged', function(args, dispatch) {
  // Convert the entry arguments into a real Entry object.
  args[0].entry = GetExternalFileEntry(args[0].entry);
  dispatch(args);
});

exports.binding = binding.generate();
