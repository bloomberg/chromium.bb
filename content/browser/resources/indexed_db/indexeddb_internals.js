// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('indexeddb', function() {
  'use strict';

  function initialize() {
    chrome.send('getAllOrigins');
  }

  function progressNodeFor(link) {
    return link.parentNode.querySelector('.download-status');
  }

  function downloadOriginData(event) {
    var link = event.target;
    progressNodeFor(link).style.display = 'inline';
    chrome.send('downloadOriginData', [link.idb_partition_path,
                                       link.idb_origin_url]);
    return false;
  }

  // Fired from the backend after the data has been zipped up, and the
  // download manager has begun downloading the file.
  function onOriginDownloadReady(partition_path, origin_url) {
    var downloadLinks = document.querySelectorAll('a.download');
    for (var i = 0; i < downloadLinks.length; ++i) {
      var link = downloadLinks[i];
      if (partition_path == link.idb_partition_path &&
          origin_url == link.idb_origin_url) {
        progressNodeFor(link).style.display = 'none';
      }
    }
  }

  // Fired from the backend with a single partition's worth of
  // IndexedDB metadata.
  function onOriginsReady(origins, partition_path) {
    var template = jstGetTemplate('indexeddb-list-template');
    var container = $('indexeddb-list');
    container.appendChild(template);
    jstProcess(new JsEvalContext({ idbs: origins,
                                   partition_path: partition_path}), template);

    var downloadLinks = container.querySelectorAll('a.download');
    for (var i = 0; i < downloadLinks.length; ++i) {
      downloadLinks[i].addEventListener('click', downloadOriginData, false);
    }
  }

  return {
    initialize: initialize,
    onOriginDownloadReady: onOriginDownloadReady,
    onOriginsReady: onOriginsReady,
  };
});

document.addEventListener('DOMContentLoaded', indexeddb.initialize);
