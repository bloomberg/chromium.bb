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
 * Callback from backend with the database contents. Sets up some globals and
 * calls to create the UI.
 * @param {boolean} enabled Whether or not NetworkActionPredictor is enabled.
 * @param {Array} database The database as a flattened list.
 */
function updateDatabaseTable(enabled, database) {
  console.debug('Updating Table NAP DB');

  var filter = $('filter');
  filter.disabled = false;
  filter.onchange = function() {
    updateDatabaseView(enabled, database);
  };

  updateDatabaseView(enabled, database);
}

/**
 * Updates the table from the database.
 * @param {boolean} enabled Whether or not NetworkActionPredictor is enabled.
 * @param {Array} database The database as a flattened list.
 */
function updateDatabaseView(enabled, database) {
  var databaseSection = $('databaseTableBody');
  var showEnabled = database && enabled;

  $('enabledMode').hidden = !showEnabled;
  $('disabledMode').hidden = showEnabled;

  if (!showEnabled)
    return;

  var filter = $('filter');

  // Clear any previous list.
  databaseSection.textContent = '';

  for (var i = 0; i < database.length; ++i) {
    var entry = database[i];

    if (!filter.checked || entry.confidence > 0) {
      var row = document.createElement('tr');

      row.appendChild(document.createElement('td')).textContent =
          entry.user_text;
      row.appendChild(document.createElement('td')).textContent = entry.url;
      row.appendChild(document.createElement('td')).textContent =
          entry.hit_count;
      row.appendChild(document.createElement('td')).textContent =
          entry.miss_count;
      row.appendChild(document.createElement('td')).textContent =
          entry.confidence;

      databaseSection.appendChild(row);
    }
  }
  $('countBanner').textContent = 'Entries: ' + databaseSection.children.length;
}

document.addEventListener('DOMContentLoaded', requestNetworkActionPredictorDb);
