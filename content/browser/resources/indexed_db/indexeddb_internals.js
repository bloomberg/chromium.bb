// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('indexeddb', function() {
  'use strict';

  function initialize() {
    chrome.send('getAllOrigins');
  }

  function onOriginsReady(origins, path) {
    var template = jstGetTemplate('indexeddb-list-template');
    var container = $('indexeddb-list');
    container.appendChild(template);
    jstProcess(new JsEvalContext({ idbs: origins, path: path}), template);
  }

  return {
      initialize: initialize,
      onOriginsReady: onOriginsReady,
  };
});

document.addEventListener('DOMContentLoaded', indexeddb.initialize);
