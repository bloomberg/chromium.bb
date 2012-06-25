// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Requests the database from the backend.
 */
function requestResourcePrefetchPredictorDb() {
  chrome.send('requestResourcePrefetchPredictorDb');
}

/**
 * Callback from backend with the database contents. Sets up some globals and
 * calls to create the UI.
 * @param {Dictionary} database Information about ResourcePrefetchPredictor
 *     including the database as a flattened list, a boolean indicating if the
 *     system is enabled.
 */
function updateResourcePrefetchPredictorDb(database) {
  updateResourcePrefetchPredictorDbView(database);
}

/**
 * Truncates the string to keep the database readable.
 * @param {String} str The string to truncate.
 * @return {String} The truncated string.
 */
function truncateString(str) {
  return str.length < 100 ? str : str.substring(0, 99);
}

/**
 * Updates the table from the database.
 * @param {Dictionary} database Information about ResourcePrefetchPredictor
 *     including the database as a flattened list, a boolean indicating if the
 *     system is enabled and the current hit weight.
 */
function updateResourcePrefetchPredictorDbView(database) {
  if (!database.enabled) {
    $('resource_prefetch_predictor_enabled').style.display = 'none';
    $('resource_prefetch_predictor_disabled').style.display = 'block';
    return;
  } else {
    $('resource_prefetch_predictor_enabled').style.display = 'block';
    $('resource_prefetch_predictor_disabled').style.display = 'none';
  }

  if (database.db) {
    if (database.db.length > 0) {
      $('resource_prefetch_predictor_db').style.display = 'block';
      $('empty_resource_prefetch_predictor_db').style.display = 'none';
    } else {
      $('resource_prefetch_predictor_db').style.display = 'none';
      $('empty_resource_prefetch_predictor_db').style.display = 'block';
    }

    var dbSection = $('resource_prefetch_predictor_db_body');
    dbSection.textContent = '';
    for (var i = 0; i < database.db.length; ++i) {
      var main = database.db[i];

      for (var j = 0; j < main.resources.length; ++j) {
        var resource = main.resources[j];
        var row = document.createElement('tr');

        if (j == 0) {
          var t = document.createElement('td');
          t.rowSpan = main.resources.length;
          t.textContent = truncateString(main.main_frame_url);
          t.className = 'last';
          row.appendChild(t);
        }

        if (j == main.resources.length - 1)
          row.className = 'last';

        row.appendChild(document.createElement('td')).textContent =
            truncateString(resource.resource_url);
        row.appendChild(document.createElement('td')).textContent =
            resource.resource_type;
        row.appendChild(document.createElement('td')).textContent =
            resource.number_of_hits;
        row.appendChild(document.createElement('td')).textContent =
            resource.number_of_misses;
        row.appendChild(document.createElement('td')).textContent =
            resource.consecutive_misses;
        row.appendChild(document.createElement('td')).textContent =
            resource.position;
        row.appendChild(document.createElement('td')).textContent =
            resource.score;
        dbSection.appendChild(row);
      }
    }
  }
}

document.addEventListener('DOMContentLoaded',
                          requestResourcePrefetchPredictorDb);
