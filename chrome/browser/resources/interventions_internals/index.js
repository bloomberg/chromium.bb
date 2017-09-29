// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('interventions_internals', () => {
  let pageHandler = null;

  function init(handler) {
    pageHandler = handler;
    getPreviewsEnabled();
  }

  function getPreviewsEnabled() {
    pageHandler.getPreviewsEnabled()
        .then((response) => {
          let statuses = $('previewsStatuses');

          response.statuses.forEach((value, key) => {
            let message = value.description + ': ';
            message += value.enabled ? 'Enabled' : 'Disabled';

            assert(!$(key), 'Component ' + key + ' already existed!');

            let node = document.createElement('p');
            node.setAttribute('id', key);
            node.textContent = message;
            statuses.appendChild(node);
          });
        })
        .catch((error) => {
          console.error(error.message);
        });
  }

  return {
    init: init,
  };
});

window.setupFn = window.setupFn || function() {
  return Promise.resolve();
};

document.addEventListener('DOMContentLoaded', () => {
  let pageHandler = null;

  window.setupFn().then(() => {
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
