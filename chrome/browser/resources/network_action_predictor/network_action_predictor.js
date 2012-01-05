// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Requests the database from the backend.
 */
function requestNetworkActionPredictorDb() {
  console.debug('Requesting NAP DB');
  chrome.send('requestNetworkActionPredictorDb', [])
}

/**
 * Callback from backend with the database contents. Builds the UI.
 * @param {boolean} enabled Whether or not NetworkActionPredictor is enabled.
 * @param {array} database The database as a flattened list.
 */
function updateDatabaseTable(enabled, database) {
  console.debug('Updating Table NAP DB');
  $('countBanner').textContent = 'Entries: ' + database.length;

  $('enabledMode').hidden = !enabled;
  $('disabledMode').hidden = enabled;

  if (!enabled)
    return;

  var databaseSection = $('databaseTableBody');

  // Clear any previous list.
  databaseSection.textContent = '';

  for (var i = 0; i < database.length; ++i) {
    var entry = database[i];
    var row = document.createElement('tr');

    row.appendChild(document.createElement('td')).textContent = entry.user_text;
    row.appendChild(document.createElement('td')).textContent = entry.url;
    row.appendChild(document.createElement('td')).textContent = entry.hit_count;
    row.appendChild(document.createElement('td')).textContent =
        entry.miss_count;
    row.appendChild(document.createElement('td')).textContent =
        entry.confidence;

    databaseSection.appendChild(row);
  }
}

document.addEventListener('DOMContentLoaded', requestNetworkActionPredictorDb);
