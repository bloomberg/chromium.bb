// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Takes the |nearbyUrlsData| input argument which holds metadata for web pages
 * broadcast by nearby devices.
 * @param {Object} nearbyUrlsData Information about web pages broadcast by
 *      nearby devices
 */
function renderTemplate(nearbyUrlsData) {
  if (nearbyUrlsData['metadata'].length != 0) {
    $('empty-list-container').hidden = true;
  }
  // This is a workaround with jstemplate. Jstemplate render only works on empty
  // node. When we need to rerender things, we have to remove previous nodes.
  let renderContainer = document.getElementById('render-container');
  // Remove existing childNode.
  while (renderContainer.hasChildNodes()) {
    renderContainer.removeChild(renderContainer.lastChild);
  }
  let templateDiv = document.getElementById('render-template').cloneNode(true);
  renderContainer.appendChild(templateDiv);

  // This is the javascript code that processes the template:
  jstProcess(new JsEvalContext(nearbyUrlsData), templateDiv);

  assignImageLoadErrorHandlers();

  let bodyContainer = $('body-container');
  bodyContainer.hidden = false;
}

function physicalWebPageLoaded() {
  chrome.send('physicalWebPageLoaded');
}

function physicalWebItemClicked(index) {
  chrome.send('physicalWebItemClicked', [index]);
}

function pushNearbyURLs(nearbyUrlsData) {
  renderTemplate(nearbyUrlsData);
}

function assignImageLoadErrorHandlers() {
  var pwIcons = document.querySelectorAll('.physicalWebIcon');
  pwIcons.forEach(function(e) {
    let img = e.getElementsByTagName('img')[0];
    img.addEventListener('error', function() {
      img.src =
          'chrome://physical-web/ic_link_grey600_36dp.png';
    });
  });
}

document.addEventListener('DOMContentLoaded', physicalWebPageLoaded);
