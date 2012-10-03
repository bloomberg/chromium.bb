// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

init();

function init() {
  var extensionIds = location.hash.substring(1).split(',');
  populateHeading(extensionIds.length);

  var template = document.querySelector('.row');
  extensionIds.forEach(populateRow.bind(null, template));
}

function populateHeading(numExtensions) {
  // TODO(aa): Internationalize. Copy whatever bookmark manager is doing.
  var singular = ' extension is interacting with this page';
  var plural = ' extensions are interacting with this page';
  var phrase = numExtensions == 1 ? singular : plural;
  document.querySelector('h1').textContent = numExtensions + phrase;
}

function populateRow(template, extensionId) {
  chrome.management.get(extensionId, function(info) {
    var row = template.cloneNode(true);
    row.querySelector('.icon').src =
        'chrome://extension-icon/' + extensionId + '/16/1';
    row.querySelector('.name').textContent = info.name;
    row.classList.remove('template');
    template.parentNode.appendChild(row);
  });
}
