// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extension_activity', function() {
  'use strict';

  function initialize() {
    var params = parseQueryParams(location);
    if (params.extensionId)
      chrome.send('requestExtensionData', [params.extensionId]);
  }

  function handleExtensionData(result) {
    var extension = result.extension;

    var item = document.querySelector('.extension-list-item');
    item.style.backgroundImage = 'url(' + extension.icon + ')';
    item.querySelector('.extension-title').textContent = extension.name;
    item.querySelector('.extension-version').textContent = extension.version;
    item.querySelector('.extension-description').textContent =
        extension.description;
  }

  function handleExtensionActivity(result) {
    var template = $('template-collection');

    var item =
        template.querySelector('.extension-activity-item').cloneNode(true);
    item.querySelector('.extension-activity-time').textContent =
        new Date().toLocaleTimeString();
    item.querySelector('.extension-activity-label').textContent =
        template.querySelector('.extension-activity-label-' + result.activity)
            .textContent;
    item.querySelector('.extension-activity-message').textContent =
        result.message;

    $('extension-activity-list').appendChild(item);
  }

  return {
    initialize: initialize,
    handleExtensionData: handleExtensionData,
    handleExtensionActivity: handleExtensionActivity
  };
});

window.addEventListener('load', extension_activity.initialize);
