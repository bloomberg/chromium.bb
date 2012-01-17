// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the chromePrivate API.

(function() {

native function GetChromeHidden();
native function DecodeJPEG(jpegImage);

GetChromeHidden().registerCustomHook('chromePrivate', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest(
      "chromePrivate.decodeJPEG", function(jpeg_image) {
    return DecodeJPEG(jpeg_image);
  });
});

})();
