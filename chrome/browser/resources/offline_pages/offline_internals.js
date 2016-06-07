// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('offlineInternals', function() {
  'use strict';

  /**
   * @typedef {{
   *   onlineUrl: string,
   *   creationTime: number,
   *   status: number,
   *   id: string,
   *   namespace: string,
   *   size: string,
   *   filePath: string,
   *   lastAccessTime: number,
   *   accessCount: number
   *  }}
   */
  var OfflinePageItem;

  /**
   * @typedef {{
   *   status: string,
   *   onlineUrl: string,
   *   creationTime: number,
   *   id: string,
   *   namespace: string,
   *   attemptCount: number
   * }}
   */
  var SavePageRequest;

  /**
   * Clear the specified table.
   * @param {string} tableId id of the table to clear.
   */
  function clearTable(tableId) {
    $(tableId).textContent = '';
  }

  /**
   * Fill stored pages table.
   * @param {HTMLElement} element A HTML element.
   * @param {!Array<OfflinePageItem>} pages An array object representing
   *     stored offline pages.
   */
  function fillStoredPages(element, pages) {
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
      cell.textContent = pages[i].onlineUrl;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = pages[i].namespace;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = pages[i].size;
      row.appendChild(cell);

      element.appendChild(row);
    }
  }

  /**
   * Fill requests table.
   * @param {HTMLElement} element A HTML element.
   * @param {!Array<SavePageRequest>} requests An array object representing
   *     the request queue.
   */
  function fillRequestQueue(element, requests) {
    for (var i = 0; i < requests.length; i++) {
      var row = document.createElement('tr');

      var cell = document.createElement('td');
      cell.textContent = requests[i].onlineUrl;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = new Date(requests[i].creationTime);
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = requests[i].status;
      row.appendChild(cell);

      element.appendChild(row);
    }
  }

  /**
   * Refresh all displayed information.
   */
  function refreshAll() {
    cr.sendWithPromise('getOfflineInternalsInfo').then(setOfflineInternalsInfo);
  }

  /**
   * Delete all pages in the offline store.
   */
  function deleteAllPages() {
    cr.sendWithPromise('deleteAllPages').then(pagesDeleted);
  }

  /**
   * Callback when pages are deleted.
   * @param {string} deletePageStatus The status of delete page call.
   */
  function pagesDeleted(deletePageStatus) {
    // TODO(chili): decide what to do here. Perhaps a refresh of just
    // the stored pages table?
  }

  /**
   * Callback when information is loaded.
   * @param {{AllPages: !Array<OfflinePageItem>,
   *          Queue: !Array<SavePageRequest>}} info An object containing both
   *     stored pages and request queue status.
   */
  function setOfflineInternalsInfo(info) {
    clearTable('stored-pages');
    clearTable('request-queue');

    fillStoredPages($('stored-pages'), info.AllPages);
    fillRequestQueue($('request-queue'), info.Queue);
  }

  /**
   * Delete selected pages from the offline store.
   */
  function deleteSelectedPages() {
    var checkboxes = document.getElementsByName('stored');
    var selectedIds = [];
    for (var checkbox of checkboxes) {
      if (checkbox.checked)
        selectedIds.push(checkbox.value);
    }

    cr.sendWithPromise('deleteSelectedPages', selectedIds).then(pagesDeleted);
  }

  function initialize() {
    $('clear-all').onclick = deleteAllPages;
    $('clear-selected').onclick = deleteSelectedPages;
    $('refresh').onclick = refreshAll;
    refreshAll();
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', offlineInternals.initialize);
