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

    var template = $('stored-pages-table-row');
    var td = template.content.querySelectorAll('td');
    for (let page of pages) {
      var checkbox = td[0].querySelector('input');
      checkbox.setAttribute('value', page.id);

      var link = td[1].querySelector('a');
      link.setAttribute('href', page.onlineUrl);
      link.textContent = page.onlineUrl;

      td[2].textContent = page.namespace;
      td[3].textContent = Math.round(page.size / 1024);
      td[4].textContent = page.isExpired;
      td[5].textContent = page.requestOrigin;

      var row = document.importNode(template.content, true);
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

    var template = $('request-queue-table-row');
    var td = template.content.querySelectorAll('td');
    for (let request of requests) {
      var checkbox = td[0].querySelector('input');
      checkbox.setAttribute('value', request.id);

      td[1].textContent = request.onlineUrl;
      td[2].textContent = new Date(request.creationTime);
      td[3].textContent = request.status;
      td[4].textContent = request.requestOrigin;

      var row = document.importNode(template.content, true);
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
   * Error callback for prefetch actions.
   * @param {*} error The error that resulted from the prefetch call.
   */
  function prefetchResultError(error) {
    var errorText = error && error.message ? error.message : error;

    $('prefetch-actions-info').textContent = 'Error: ' + errorText;
  }

  /**
   * Downloads all the stored page and request queue information into a file.
   * Also translates all the fields representing datetime into human-readable
   * date strings.
   * TODO(chili): Create a CSV writer that can abstract out the line joining.
   */
  function dumpAsJson() {
    var json = JSON.stringify(
        {offlinePages: offlinePages, savePageRequests: savePageRequests},
        function(key, value) {
          return key.endsWith('Time') ? new Date(value).toString() : value;
        },
        2);

    $('dump-box').value = json;
    $('dump-info').textContent = '';
    $('dump-modal').showModal();
    $('dump-box').select();
  }

  function closeDump() {
    $('dump-modal').close();
    $('dump-box').value = '';
  }

  function copyDump() {
    $('dump-box').select();
    document.execCommand('copy');
    $('dump-info').textContent = 'Copied to clipboard!';
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
    ['delete-all-pages', 'delete-selected-pages', 'delete-all-requests',
     'delete-selected-requests', 'log-model-on', 'log-model-off',
     'log-request-on', 'log-request-off', 'refresh']
        .forEach(el => $(el).disabled = incognito);

    $('delete-all-pages').onclick = deleteAllPages;
    $('delete-selected-pages').onclick = deleteSelectedPages;
    $('delete-all-requests').onclick = deleteAllRequests;
    $('delete-selected-requests').onclick = deleteSelectedRequests;
    $('refresh').onclick = refreshAll;
    $('dump').onclick = dumpAsJson;
    $('close-dump').onclick = closeDump;
    $('copy-to-clipboard').onclick = copyDump;
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
      browserProxy.scheduleNwake()
          .then(setPrefetchResult)
          .catch(prefetchResultError);
    };
    $('cancel-nwake').onclick = function() {
      browserProxy.cancelNwake()
          .then(setPrefetchResult)
          .catch(prefetchResultError);
    };
    $('show-notification').onclick = function() {
      browserProxy.showPrefetchNotification().then(setPrefetchResult);
    };
    $('generate-page-bundle').onclick = function() {
      browserProxy.generatePageBundle($('generate-urls').value)
          .then(setPrefetchResult)
          .catch(prefetchResultError);
    };
    $('get-operation').onclick = function() {
      browserProxy.getOperation($('operation-name').value)
          .then(setPrefetchResult)
          .catch(prefetchResultError);
    };
    $('download-archive').onclick = function() {
      browserProxy.downloadArchive($('download-name').value);
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
