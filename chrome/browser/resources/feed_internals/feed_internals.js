// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Reference to the backend.
 * @type {feedInternals.mojom.PageHandlerProxy}
 */
let pageHandler = null;

(function() {

/**
 * Get and display general properties.
 */
function updatePageWithProperties() {
  pageHandler.getGeneralProperties().then(response => {
    /** @type {!feedInternals.mojom.Properties} */
    const properties = response.properties;
    $('is-feed-enabled').textContent = properties.isFeedEnabled;
  });
}

/**
 * Get and display user classifier properties.
 */
function updatePageWithUserClass() {
  pageHandler.getUserClassifierProperties().then(response => {
    /** @type {!feedInternals.mojom.UserClassifier} */
    const properties = response.properties;
    $('user-class-description').textContent = properties.userClassDescription;
    $('avg-hours-between-views').textContent = properties.avgHoursBetweenViews;
    $('avg-hours-between-uses').textContent = properties.avgHoursBetweenUses;
  });
}

/**
 * Get and display last fetch data.
 */
function updatePageWithLastFetchProperties() {
  pageHandler.getLastFetchProperties().then(response => {
    /** @type {!feedInternals.mojom.LastFetchProperties} */
    const properties = response.properties;
    $('last-fetch-status').textContent = properties.lastFetchStatus;
    $('last-fetch-time').textContent = toDateString(properties.lastFetchTime);
    $('refresh-suppress-time').textContent =
        toDateString(properties.refreshSuppressTime);
  });
}

/**
 * Convert time to string for display.
 *
 * @param {feedInternals.mojom.Time|undefined} time
 * @return {string}
 */
function toDateString(time) {
  return time == null ? '' : new Date(time.msSinceEpoch).toLocaleString();
}

/**
 * Hook up buttons to event listeners.
 */
function setupEventListeners() {
  $('clear-user-classification').addEventListener('click', function() {
    pageHandler.clearUserClassifierProperties();
    updatePageWithUserClass();
  });

  $('clear-cached-data').addEventListener('click', function() {
    pageHandler.clearCachedDataAndRefreshFeed();

    // TODO(chouinard): Investigate whether the Feed library's
    // AppLifecycleListener.onClearAll methods could accept a callback to notify
    // when cache clear and Feed refresh operations are complete. If not,
    // consider adding backend->frontend mojo communication to listen for
    // updates, rather than waiting an arbitrary period of time.
    setTimeout(updatePageWithLastFetchProperties, 1000);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = feedInternals.mojom.PageHandler.getProxy();

  updatePageWithProperties();
  updatePageWithUserClass();
  updatePageWithLastFetchProperties();

  setupEventListeners();
});
})();
