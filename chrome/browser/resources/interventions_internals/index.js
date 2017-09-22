// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('interventions_internals', function() {
  let pageHandler = null;

  function init(handler) {
    pageHandler = handler;
    getPreviewsEnabled();
  }

  function getPreviewsEnabled() {
    pageHandler.getPreviewsEnabled()
        .then(function(response) {
          let message = 'OfflinePreviews: ';
          message += response.enabled ? 'Enabled' : 'Disabled';
          $('offlinePreviews').textContent = message;
        })
        .catch(function(error) {
          node.textContent = error.message;
        });
  }

  return {
    init: init,
  };
});

window.setupFn = window.setupFn || function() {
  return Promise.resolve();
};

document.addEventListener('DOMContentLoaded', function() {
  let pageHandler = null;

  window.setupFn().then(function() {
    if (window.testPageHandler) {
      pageHandler = window.testPageHandler;
    } else {
      pageHandler = new mojom.InterventionsInternalsPageHandlerPtr;
      Mojo.bindInterface(
          mojom.InterventionsInternalsPageHandler.name,
          mojo.makeRequest(pageHandler).handle);
    }
    interventions_internals.init(pageHandler);
  });
});
