// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

localStrings = new LocalStrings();

/**
 * Requests the list of crashes from the backend.
 */
function requestCrashes() {
  chrome.send('requestCrashList', [])
}

/**
 * Callback from backend with the list of crashes. Builds the UI.
 * @param {boolean} enabled Whether or not crash reporting is enabled.
 * @param {array} crashes The list of crashes.
 */
function updateCrashList(enabled, crashes) {
  $('countBanner').textContent = localStrings.getStringF('crashCountFormat',
                                                         crashes.length);

  var crashSection = $('crashList');

  $('enabledMode').hidden = !enabled;
  $('disabledMode').hidden = enabled;

  if (!enabled)
    return;

  // Clear any previous list.
  crashSection.textContent = '';

  for (var i = 0; i < crashes.length; i++) {
    var crash = crashes[i];

    var crashBlock = document.createElement('div');
    var title = document.createElement('h3');
    title.textContent = localStrings.getStringF('crashHeaderFormat',
                                                crash['id']);
    crashBlock.appendChild(title);
    var date = document.createElement('p');
    date.textContent = localStrings.getStringF('crashTimeFormat',
                                               crash['time']);
    crashBlock.appendChild(date);
    var linkBlock = document.createElement('p');
    var link = document.createElement('a');
    link.href = 'http://code.google.com/p/chromium/issues/entry?' +
        'comment=URL%20(if%20applicable)%20where%20crash%20occurred:%20%0A%0A' +
        'What%20steps%20will%20reproduce%20this%20crash?%0A1.%0A2.%0A3.%0A%0A' +
        '****DO%20NOT%20CHANGE%20BELOW%20THIS%20LINE****%0Areport_id:' +
        crash['id'];
    link.target = '_blank';
    link.textContent = localStrings.getString('bugLinkText');
    linkBlock.appendChild(link);
    crashBlock.appendChild(linkBlock);
    crashSection.appendChild(crashBlock);
  }

  $('noCrashes').hidden = crashes.length != 0;
}

document.addEventListener('DOMContentLoaded', requestCrashes);
