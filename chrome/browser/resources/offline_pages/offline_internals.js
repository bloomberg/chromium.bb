// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('offlineInternals', function() {
  'use strict';

  /** @type {!Array<OfflinePage>} */
  var offlinePages = [];

  /** @type {!Array<SavePageRequest>} */
  var savePageRequests = [];

  /** @type {!offlineInternals.OfflineInternalsBrowserProxy} */
  var browserProxy =
      offlineInternals.OfflineInternalsBrowserProxyImpl.getInstance();

  /**
   * Helper to fill enabled labels based on boolean value.
   * @param {boolean} enabled Whether the text should show on or off.
   * @return {string}
   */
  function getTextLabel(enabled) {
    return enabled ? 'On' : 'Off';
  }

  /**
   * Fill stored pages table.
   * @param {!Array<OfflinePage>} pages An array object representing
   *     stored offline pages.
   */
  function fillStoredPages(pages) {
    var storedPagesTable = $('stored-pages');
    storedPagesTable.textContent = '';

    for (var i = 0; i < pages.length; i++) {
      var row = document.createElement('tr');

      var checkboxCell = document.createElement('td');
      var checkbox = document.createElement('input');
      checkbox.setAttribute('type', 'checkbox');
      checkbox.setAttribute('name', 'stored');
      checkbox.setAttribute('value', pages[i].id);

      checkboxCell.appendChild(checkbox);
      row.appendChild(checkboxCell);

      var cell = document.createElement('td');
      var link = document.createElement('a');
      link.setAttribute('href', pages[i].onlineUrl);
      link.textContent = pages[i].onlineUrl;
      cell.appendChild(link);
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = pages[i].namespace;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = Math.round(pages[i].size / 1024);
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = pages[i].isExpired;
      row.appendChild(cell);

      storedPagesTable.appendChild(row);
    }
    offlinePages = pages;
  }

  /**
   * Fill requests table.
   * @param {!Array<SavePageRequest>} requests An array object representing
   *     the request queue.
   */
  function fillRequestQueue(requests) {
    var requestQueueTable = $('request-queue');
    requestQueueTable.textContent = '';

    for (var i = 0; i < requests.length; i++) {
      var row = document.createElement('tr');

      var checkboxCell = document.createElement('td');
      var checkbox = document.createElement('input');
      checkbox.setAttribute('type', 'checkbox');
      checkbox.setAttribute('name', 'requests');
      checkbox.setAttribute('value', requests[i].id);

      checkboxCell.appendChild(checkbox);
      row.appendChild(checkboxCell);

      var cell = document.createElement('td');
      cell.textContent = requests[i].onlineUrl;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = new Date(requests[i].creationTime);
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = requests[i].status;
      row.appendChild(cell);

      requestQueueTable.appendChild(row);
    }
    savePageRequests = requests;
  }

  /**
   * Fills the event logs section.
   * @param {!Array<string>} logs A list of log strings.
   */
  function fillEventLog(logs) {
    var element = $('logs');
    element.textContent = '';
    for (let log of logs) {
      var logItem = document.createElement('li');
      logItem.textContent = log;
      element.appendChild(logItem);
    }
  }

  /**
   * Refresh all displayed information.
   */
  function refreshAll() {
    browserProxy.getStoredPages().then(fillStoredPages);
    browserProxy.getRequestQueue().then(fillRequestQueue);
    browserProxy.getNetworkStatus().then(function(networkStatus) {
      $('current-status').textContent = networkStatus;
    });
    refreshLog();
  }

  /**
   * Delete all pages in the offline store.
   */
  function deleteAllPages() {
    var checkboxes = document.getElementsByName('stored');
    var selectedIds = [];

    for (var i = 0; i < checkboxes.length; i++) {
      selectedIds.push(checkboxes[i].value);
    }

    browserProxy.deleteSelectedPages(selectedIds).then(pagesDeleted);
  }

  /**
   * Delete all pending SavePageRequest items in the request queue.
   */
  function deleteAllRequests() {
    var checkboxes = document.getElementsByName('requests');
    var selectedIds = [];

    for (var i = 0; i < checkboxes.length; i++) {
      selectedIds.push(checkboxes[i].value);
    }

    browserProxy.deleteSelectedRequests(selectedIds).then(requestsDeleted);
  }

  /**
   * Callback when pages are deleted.
   * @param {string} status The status of the request.
   */
  function pagesDeleted(status) {
    $('page-actions-info').textContent = status;
    browserProxy.getStoredPages().then(fillStoredPages);
  }

  /**
   * Callback when requests are deleted.
   */
  function requestsDeleted(status) {
    $('request-queue-actions-info').textContent = status;
    browserProxy.getRequestQueue().then(fillRequestQueue);
  }

  /**
   * Callback for prefetch actions.
   * @param {string} info The result of performing the prefetch actions.
   */
  function setPrefetchResult(info) {
    $('prefetch-actions-info').textContent = info;
  }

  /**
   * Downloads all the stored page and request queue information into a file.
   * TODO(chili): Create a CSV writer that can abstract out the line joining.
   */
  function download() {
    var json = JSON.stringify(
        {offlinePages: offlinePages, savePageRequests: savePageRequests}, null,
        2);

    window.open(
        'data:application/json,' + encodeURIComponent(json), 'dump.json');
  }

  /**
   * Updates the status strings.
   * @param {!IsLogging} logStatus Status of logging.
   */
  function updateLogStatus(logStatus) {
    $('model-status').textContent = getTextLabel(logStatus.modelIsLogging);
    $('request-status').textContent = getTextLabel(logStatus.queueIsLogging);
    $('prefetch-status').textContent =
        getTextLabel(logStatus.prefetchIsLogging);
  }

  /**
   * Delete selected pages from the offline store.
   */
  function deleteSelectedPages() {
    var checkboxes = document.getElementsByName('stored');
    var selectedIds = [];

    for (var i = 0; i < checkboxes.length; i++) {
      if (checkboxes[i].checked)
        selectedIds.push(checkboxes[i].value);
    }

    browserProxy.deleteSelectedPages(selectedIds).then(pagesDeleted);
  }

  /**
   * Delete selected SavePageRequest items from the request queue.
   */
  function deleteSelectedRequests() {
    var checkboxes = document.getElementsByName('requests');
    var selectedIds = [];

    for (var i = 0; i < checkboxes.length; i++) {
      if (checkboxes[i].checked)
        selectedIds.push(checkboxes[i].value);
    }

    browserProxy.deleteSelectedRequests(selectedIds).then(requestsDeleted);
  }

  /**
   * Refreshes the logs.
   */
  function refreshLog() {
    browserProxy.getEventLogs().then(fillEventLog);
    browserProxy.getLoggingState().then(updateLogStatus);
  }

  function initialize() {
    /**
     * @param {boolean} enabled Whether to enable Logging. If the
     * OfflinePageModlel does not exist in this context, the action is ignored.
     */
    function togglePageModelLog(enabled) {
      browserProxy.setRecordPageModel(enabled);
      $('model-status').textContent = getTextLabel(enabled);
    }

    /**
     * @param {boolean} enabled Whether to enable Logging. If the
     * OfflinePageModlel does not exist in this context, the action is ignored.
     */
    function toggleRequestQueueLog(enabled) {
      browserProxy.setRecordRequestQueue(enabled);
      $('request-status').textContent = getTextLabel(enabled);
    }

    /**
     * @param {boolean} enabled Whether to enable Logging. If the
     * OfflinePageModlel does not exist in this context, the action is ignored.
     */
    function togglePrefetchServiceLog(enabled) {
      browserProxy.setRecordPrefetchService(enabled);
      $('prefetch-status').textContent = getTextLabel(enabled);
    }

    var incognito = loadTimeData.getBoolean('isIncognito');
    $('delete-all-pages').disabled = incognito;
    $('delete-selected-pages').disabled = incognito;
    $('delete-all-requests').disabled = incognito;
    $('delete-selected-requests').disabled = incognito;
    $('log-model-on').disabled = incognito;
    $('log-model-off').disabled = incognito;
    $('log-request-on').disabled = incognito;
    $('log-request-off').disabled = incognito;
    $('refresh').disabled = incognito;

    $('delete-all-pages').onclick = deleteAllPages;
    $('delete-selected-pages').onclick = deleteSelectedPages;
    $('delete-all-requests').onclick = deleteAllRequests;
    $('delete-selected-requests').onclick = deleteSelectedRequests;
    $('refresh').onclick = refreshAll;
    $('download').onclick = download;
    $('log-model-on').onclick = togglePageModelLog.bind(this, true);
    $('log-model-off').onclick = togglePageModelLog.bind(this, false);
    $('log-request-on').onclick = toggleRequestQueueLog.bind(this, true);
    $('log-request-off').onclick = toggleRequestQueueLog.bind(this, false);
    $('log-prefetch-on').onclick = togglePrefetchServiceLog.bind(this, true);
    $('log-prefetch-off').onclick = togglePrefetchServiceLog.bind(this, false);
    $('refresh-logs').onclick = refreshLog;
    $('add-to-queue').onclick = function() {
      var saveUrls = $('url').value.split(',');
      var counter = saveUrls.length;
      $('save-url-state').textContent = '';
      for (let i = 0; i < saveUrls.length; i++) {
        browserProxy.addToRequestQueue(saveUrls[i]).then(function(state) {
          if (state) {
            $('save-url-state').textContent +=
                saveUrls[i] + ' has been added to queue.\n';
            $('url').value = '';
            counter--;
            if (counter == 0) {
              browserProxy.getRequestQueue().then(fillRequestQueue);
            }
          } else {
            $('save-url-state').textContent +=
                saveUrls[i] + ' failed to be added to queue.\n';
          }
        });
      }
    };
    $('schedule-nwake').onclick = function() {
      browserProxy.scheduleNwake().then(setPrefetchResult);
    };
    $('cancel-nwake').onclick = function() {
      browserProxy.cancelNwake().then(setPrefetchResult);
    };
    $('generate-page-bundle').onclick = function() {
      browserProxy.generatePageBundle($('generate-urls').value)
          .then(setPrefetchResult);
    };
    $('get-operation').onclick = function() {
      browserProxy.getOperation($('operation-name').value)
          .then(setPrefetchResult);
    };
    if (!incognito)
      refreshAll();
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', offlineInternals.initialize);
