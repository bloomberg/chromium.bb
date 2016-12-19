// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initialize() {
  navigator.serviceWorker.register('service_worker.js');

  window.addEventListener('appinstalled', () => {
    window.document.title = 'Got appinstalled';
  });
}

function addManifestLinkTag() {
  var url = window.location.href;
  var manifestIndex = url.indexOf("?manifest=");
  var manifestUrl =
      (manifestIndex >= 0) ? url.slice(manifestIndex + 10) : 'manifest.json';
  var linkTag = document.createElement("link");
  linkTag.rel = "manifest";
  linkTag.href = manifestUrl;
  document.head.append(linkTag);
}
