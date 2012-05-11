// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Media Gallery API.

var mediaGalleriesNatives = requireNative('experimental_mediaGalleries');

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('experimental.mediaGalleries',
                                function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // getMediaFileSystems uses a custom callback so that it can instantiate and
  // return an array of file system objects.
  apiFunctions.setCustomCallback('getMediaFileSystems',
                                 function(name, request, response) {
    var result = [];
    for (var i = 0; i < response.length; i++) {
      result.push(mediaGalleriesNatives.GetMediaFileSystemObject(response[i]));
    }
    if (request.callback)
      request.callback(result);
    request.callback = null;
  });

  // extractEmbeddedThumbnails uses a renderer side handler so that it can
  // synchronously return a result.  The result object's state is computable
  // from the function's input.
  apiFunctions.setHandleRequest('extractEmbeddedThumbnails',
                                 function(fileEntry) {
    return mediaGalleriesNatives.ExtractEmbeddedThumbnails(fileEntry);
  });
});
