// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * Reference to the backend providing all the data.
 * @type {mojom.ProcessInternalsHandlerPtr}
 */
let uiHandler = null;

/**
 * @param {string} id Tab id.
 * @return {boolean} True if successful.
 */
function selectTab(id) {
  const tabContents = document.querySelectorAll('#content > div');
  const tabHeaders = $('navigation').querySelectorAll('.tab-header');
  let found = false;
  for (let i = 0; i < tabContents.length; i++) {
    const tabContent = tabContents[i];
    const tabHeader = tabHeaders[i];
    const isTargetTab = tabContent.id == id;

    found = found || isTargetTab;
    tabContent.classList.toggle('selected', isTargetTab);
    tabHeader.classList.toggle('selected', isTargetTab);
  }
  if (!found)
    return false;
  window.location.hash = id;
  return true;
}

function onHashChange() {
  let hash = window.location.hash.slice(1).toLowerCase();
  if (!selectTab(hash))
    selectTab('general');
}

function setupTabs() {
  const tabContents = document.querySelectorAll('#content > div');
  for (let i = 0; i < tabContents.length; i++) {
    const tabContent = tabContents[i];
    const tabName = tabContent.querySelector('.content-header').textContent;

    let tabHeader = document.createElement('div');
    tabHeader.className = 'tab-header';
    let button = document.createElement('button');
    button.textContent = tabName;
    tabHeader.appendChild(button);
    tabHeader.addEventListener('click', selectTab.bind(null, tabContent.id));
    $('navigation').appendChild(tabHeader);
  }
  onHashChange();
}


document.addEventListener('DOMContentLoaded', function() {
  // Setup Mojo interface to the backend.
  uiHandler = new mojom.ProcessInternalsHandlerPtr;
  Mojo.bindInterface(
      mojom.ProcessInternalsHandler.name,
      mojo.makeRequest(uiHandler).handle);

  // Get the Site Isolation mode and populate it.
  uiHandler.getIsolationMode().then((response) => {
    document.getElementById('isolation-mode').innerText = response.mode;
  });
  uiHandler.getIsolatedOriginsSize().then((response) => {
    document.getElementById('isolated-origins').innerText = response.size;
  });

  // Setup the UI
  setupTabs();
});

})();
