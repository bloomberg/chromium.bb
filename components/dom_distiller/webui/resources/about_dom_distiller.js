// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Callback from the backend with the list of entries to display.
 * This call will build the entries section of the DOM distiller page, or hide
 * that section if there are none to display.
 * @param {!Array.<string>} entries The entries.
 */
function onGotEntries(entries) {
  $('entries-section').hidden = !entries.length;
  if (entries.length > 0) {
    var list = document.createElement('ul');
    for (var i = 0; i < entries.length; i++) {
      var listItem = document.createElement('li');
      var link = document.createElement('a');
      link.innerText = entries[i].title;
      link.setAttribute('href', entries[i].url);
      listItem.appendChild(link);
      list.appendChild(listItem);
    }
    $('entries-list').appendChild(list);
  }
}

/* All the work we do on load. */
function onLoadWork() {
  chrome.send('requestEntries');
}

document.addEventListener('DOMContentLoaded', onLoadWork);
