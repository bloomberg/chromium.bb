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

    function viewJson(event) {
      $('json-value').textContent = '';
      chrome.send('viewJson');
      event.preventDefault();
    }

    $('view-json').addEventListener('click', viewJson);

    chrome.send('registerForEvents');
  }

  function receiveDownloadResult(result) {
    $('download-result').textContent = result;
  }

  function receiveSites(sites) {
    jstProcess(new JsEvalContext(sites), $('sites'));
    // Also clear the json string, since it's likely stale now.
    $('json-value').textContent = '';
  }

  function receiveJson(json) {
    $('json-value').textContent = json;
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    receiveDownloadResult: receiveDownloadResult,
    receiveSites: receiveSites,
    receiveJson: receiveJson,
  };
});

document.addEventListener('DOMContentLoaded',
                          chrome.popular_sites_internals.initialize);

