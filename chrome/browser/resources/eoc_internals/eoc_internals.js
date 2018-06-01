// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

// Reference to the backend.
let pageHandler = null;

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = new eocInternals.mojom.PageHandlerPtr;
  Mojo.bindInterface(
      eocInternals.mojom.PageHandler.name,
      mojo.makeRequest(pageHandler).handle);
});
})();