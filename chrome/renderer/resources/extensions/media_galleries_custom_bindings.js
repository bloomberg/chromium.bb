// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Media Gallery API.

var binding = require('binding').Binding.create('mediaGalleries');

var mediaGalleriesNatives = requireNative('mediaGalleries');

var mediaGalleriesMetadata = {};

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // getMediaFileSystems uses a custom callback so that it can instantiate and
  // return an array of file system objects.
  apiFunctions.setCustomCallback('getMediaFileSystems',
                                 function(name, request, response) {
    var result = null;
    mediaGalleriesMetadata = {};  // Clear any previous metadata.
    if (response) {
      result = [];
      for (var i = 0; i < response.length; i++) {
        var filesystem = mediaGalleriesNatives.GetMediaFileSystemObject(
            response[i].fsid);
        result.push(filesystem);
        var metadata = response[i];
        delete metadata.fsid;
        mediaGalleriesMetadata[filesystem.name] = metadata;
      }
    }
    if (request.callback)
      request.callback(result);
    request.callback = null;
  });

  apiFunctions.setHandleRequest('getMediaFileSystemMetadata',
                                function(filesystem) {
    if (filesystem && filesystem.name &&
        mediaGalleriesMetadata[filesystem.name]) {
      return mediaGalleriesMetadata[filesystem.name];
    }
    return {};
  });
});

exports.binding = binding.generate();
