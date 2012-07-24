// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Media Gallery API.

var mediaGalleriesNatives = requireNative('mediaGalleries');

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('mediaGalleries',
                                function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // getMediaFileSystems uses a custom callback so that it can instantiate and
  // return an array of file system objects.
  apiFunctions.setCustomCallback('getMediaFileSystems',
                                 function(name, request, response) {
    var result = null;
    if (response) {
      result = [];
      for (var i = 0; i < response.length; i++) {
        result.push(mediaGalleriesNatives.GetMediaFileSystemObject(
            response[i].fsid, response[i].dirname));
      }
    }
    if (request.callback)
      request.callback(result);
    request.callback = null;
  });
});
