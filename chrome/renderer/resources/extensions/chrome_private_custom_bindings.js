// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the chromePrivate API.

var chromePrivate = requireNative('chrome_private');
var DecodeJPEG = chromePrivate.DecodeJPEG;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('chromePrivate', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('decodeJPEG', function(jpeg_image) {
    return DecodeJPEG(jpeg_image);
  });
});
