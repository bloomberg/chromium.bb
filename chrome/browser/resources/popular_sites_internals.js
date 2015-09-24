// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.popular_sites_internals', function() {
  'use strict';

  function initialize() {
    function submitDownload(event) {
      $('download-result').textContent = '';
      chrome.send('download', [$('url-input').value,
                               $('country-input').value,
                               $('version-input').value]);
      event.preventDefault();
    }

    $('submit-download').addEventListener('click', submitDownload);

    chrome.send('registerForEvents');
  }

  function receiveDownloadResult(result) {
    $('download-result').textContent = result;
  }

  function receiveSites(sites) {
    jstProcess(new JsEvalContext(sites), $('sites'));
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    receiveDownloadResult: receiveDownloadResult,
    receiveSites: receiveSites,
  };
});

document.addEventListener('DOMContentLoaded',
                          chrome.popular_sites_internals.initialize);

