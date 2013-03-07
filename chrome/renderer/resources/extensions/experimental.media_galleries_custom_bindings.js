// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Media Gallery API.

var binding = require('binding').Binding.create('experimental.mediaGalleries');

var mediaGalleriesNatives = requireNative('mediaGalleries');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // extractEmbeddedThumbnails uses a renderer side handler so that it can
  // synchronously return a result.  The result object's state is computable
  // from the function's input.
  apiFunctions.setHandleRequest('extractEmbeddedThumbnails',
                                function(fileEntry) {
    return mediaGalleriesNatives.ExtractEmbeddedThumbnails(fileEntry);
  });
});

exports.binding = binding.generate();
