// Copyright 2014 The Chromium Authors. All rights reserved.
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
 * @param {Object} database Information about ResourcePrefetchPredictor
 *     including the database as a flattened list, a boolean indicating if the
 *     system is enabled.
 */
function updateResourcePrefetchPredictorDb(database) {
  updateResourcePrefetchPredictorDbView(database);
}

/**
 * Truncates the string to keep the database readable.
 * @param {string} str The string to truncate.
 * @return {string} The truncated string.
 */
function truncateString(str) {
  return str.length < 100 ? str : str.substring(0, 99);
}

/**
 * Updates the table from the database.
 * @param {Object} database Information about ResourcePrefetchPredictor
 *     including the database as a flattened list, a boolean indicating if the
 *     system is enabled and the current hit weight.
 */
function updateResourcePrefetchPredictorDbView(database) {
  if (!database.enabled) {
    $('rpp_enabled').style.display = 'none';
    $('rpp_disabled').style.display = 'block';
    return;
  }

  $('rpp_enabled').style.display = 'block';
  $('rpp_disabled').style.display = 'none';

  var hasUrlData = database.url_db && database.url_db.length > 0;
  var hasHostData = database.host_db && database.host_db.length > 0;
  var hasOriginData = database.origin_db && database.origin_db.length > 0;

  if (hasUrlData)
    renderCacheData($('rpp_url_body'), database.url_db);
  if (hasHostData)
    renderCacheData($('rpp_host_body'), database.host_db);
  if (hasOriginData)
    renderOriginData($('rpp_origin_body'), database.origin_db);
}

/**
 * Renders cache data for URL or host based data.
 * @param {HTMLElement} body element of table to render into.
 * @param {Object} database to render.
 */
function renderCacheData(body, database) {
  body.textContent = '';
  for (let main of database) {
    for (var j = 0; j < main.resources.length; ++j) {
      var resource = main.resources[j];
      var row = document.createElement('tr');

      if (j == 0) {
        var t = document.createElement('td');
        t.rowSpan = main.resources.length;
        t.textContent = truncateString(main.main_frame_url);
        row.appendChild(t);
      }

      row.className =
          resource.is_prefetchable ? 'action-prerender' : 'action-none';

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
      row.appendChild(document.createElement('td')).textContent =
          resource.before_first_contentful_paint;
      body.appendChild(row);
    }
  }
}

/**
 * Renders the content of the predictor origin table.
 * @param {HTMLElement} body element of table to render into.
 * @param {Object} database to render.
 */
function renderOriginData(body, database) {
  body.textContent = '';
  for (let main of database) {
    for (var j = 0; j < main.origins.length; ++j) {
      var origin = main.origins[j];
      var row = document.createElement('tr');

      if (j == 0) {
        var t = document.createElement('td');
        t.rowSpan = main.origins.length;
        t.textContent = truncateString(main.main_frame_host);
        row.appendChild(t);
      }

      row.className = 'action-none';
      row.appendChild(document.createElement('td')).textContent =
          truncateString(origin.origin);
      row.appendChild(document.createElement('td')).textContent =
          origin.number_of_hits;
      row.appendChild(document.createElement('td')).textContent =
          origin.number_of_misses;
      row.appendChild(document.createElement('td')).textContent =
          origin.consecutive_misses;
      row.appendChild(document.createElement('td')).textContent =
          origin.position;
      row.appendChild(document.createElement('td')).textContent =
          origin.always_access_network;
      row.appendChild(document.createElement('td')).textContent =
          origin.accessed_network;
      row.appendChild(document.createElement('td')).textContent =
          origin.score;
      body.appendChild(row);
    }
  }
}

document.addEventListener(
    'DOMContentLoaded', requestResourcePrefetchPredictorDb);
