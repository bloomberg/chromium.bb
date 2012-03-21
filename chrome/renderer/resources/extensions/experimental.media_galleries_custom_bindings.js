// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Media Gallery API.

var mediaGalleriesNatives = requireNative('experimental_mediaGalleries');
var CreateMediaGalleryObject = mediaGalleriesNatives.CreateMediaGalleryObject;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('experimental.mediaGalleries',
                                function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('getMediaGalleries',
                                 function(name, request, response) {
    var result = [];
    var resp = response ? chromeHidden.JSON.parse(response) : [];
    for (var i = 0; i < resp.length; i++) {
      result.push(CreateMediaGalleryObject(resp[i].id, resp[i].name,
                                           resp[i].flags));

    }
    if (request.callback)
      request.callback(result);
    request.callback = null;
  });
});
