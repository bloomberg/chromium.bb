// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('offlineInternals', function() {
  'use strict';

  /** @type {!Array<OfflinePage>} */
  let offlinePages = [];

  /** @type {!Array<SavePageRequest>} */
  let savePageRequests = [];

  /** @type {!offlineInternals.OfflineInternalsBrowserProxy} */
  const browserProxy =
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
    const storedPagesTable = $('stored-pages');
    storedPagesTable.textContent = '';

    const template = $('stored-pages-table-row');
    const td = template.content.querySelectorAll('td');
    for (let pageIndex = 0; pageIndex < pages.length; pageIndex++) {
      const page = pages[pageIndex];
      td[0].textContent = pageIndex + 1;
      const checkbox = td[1].querySelector('input');
      checkbox.setAttribute('value', page.id);

      const link = td[2].querySelector('a');
      link.setAttribute('href', page.onlineUrl);
      const maxUrlCharsPerLine = 50;
      if (page.onlineUrl.length > maxUrlCharsPerLine) {
        link.textContent = '';
        for (let i = 0; i < page.onlineUrl.length; i += maxUrlCharsPerLine) {
          link.textContent += page.onlineUrl.slice(i, i + maxUrlCharsPerLine);
          link.textContent += '\r\n';
        }
      } else {
        link.textContent = page.onlineUrl;
      }

      td[3].textContent = page.namespace;
      td[4].textContent = Math.round(page.size / 1024);

      const row = document.importNode(template.content, true);
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
    const requestQueueTable = $('request-queue');
    requestQueueTable.textContent = '';

    const template = $('request-queue-table-row');
    const td = template.content.querySelectorAll('td');
    for (const request of requests) {
      const checkbox = td[0].querySelector('input');
      checkbox.setAttribute('value', request.id);

      td[1].textContent = request.onlineUrl;
      td[2].textContent = new Date(request.creationTime);
      td[3].textContent = request.status;
      td[4].textContent = request.requestOrigin;

      const row = document.importNode(template.content, true);
      requestQueueTable.appendChild(row);
    }
    savePageRequests = requests;
  }

  /**
   * Fills the event logs section.
   * @param {!Array<string>} logs A list of log strings.
   */
  function fillEventLog(logs) {
    const element = $('logs');
    element.textContent = '';
    for (const log of logs) {
      const logItem = document.createElement('li');
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
    const errorText = error && error.message ? error.message : error;

    $('prefetch-actions-info').textContent = 'Error: ' + errorText;
  }

  /**
   * Downloads all the stored page and request queue information into a file.
   * Also translates all the fields representing datetime into human-readable
   * date strings.
   * TODO(chili): Create a CSV writer that can abstract out the line joining.
   */
  function dumpAsJson() {
    const json = JSON.stringify(
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
   * Sets all checkboxes with a specific name to the same checked status as the
   * provided source checkbox.
   * @param {HTMLElement} source The checkbox controlling the checked
   *     status.
   * @param {string} checkboxesName The name identifying the checkboxes to set.
   */
  function toggleAllCheckboxes(source, checkboxesName) {
    const checkboxes = document.getElementsByName(checkboxesName);
    for (const checkbox of checkboxes) {
      checkbox.checked = source.checked;
    }
  }

  /**
   * Return the item ids for the selected checkboxes with a given name.
   * @param {string} checkboxesName The name identifying the checkboxes to
   *     query.
   * @return {!Array<string>} An array of selected ids.
   */
  function getSelectedIdsFor(checkboxesName) {
    const checkboxes = document.querySelectorAll(
        `input[type="checkbox"][name="${checkboxesName}"]:checked`);
    return Array.from(checkboxes).map(c => c.value);
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

    const incognito = loadTimeData.getBoolean('isIncognito');
    ['delete-selected-pages', 'delete-selected-requests', 'log-model-on',
     'log-model-off', 'log-request-on', 'log-request-off', 'refresh']
        .forEach(el => $(el).disabled = incognito);

    $('delete-selected-pages').onclick = function() {
      const pageIds = getSelectedIdsFor('stored');
      browserProxy.deleteSelectedPages(pageIds).then(pagesDeleted);
    };
    $('delete-selected-requests').onclick = function() {
      const requestIds = getSelectedIdsFor('requests');
      browserProxy.deleteSelectedRequests(requestIds).then(requestsDeleted);
    };
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
      const saveUrls = $('url').value.split(',');
      let counter = saveUrls.length;
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
    $('toggle-all-stored').onclick = function() {
      toggleAllCheckboxes($('toggle-all-stored'), 'stored');
    };
    $('toggle-all-requests').onclick = function() {
      toggleAllCheckboxes($('toggle-all-requests'), 'requests');
    };
    if (!incognito) {
      refreshAll();
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', offlineInternals.initialize);
