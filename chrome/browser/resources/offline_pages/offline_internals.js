// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   onlineUrl: string,
 *   creationTime: number,
 *   id: string,
 *   namespace: string,
 *   size: string,
 *   filePath: string,
 *   lastAccessTime: number,
 *   accessCount: number
 * }}
 */
var OfflinePage;

/**
 * @typedef {{
 *   status: string,
 *   onlineUrl: string,
 *   creationTime: number,
 *   id: string,
 *   namespace: string,
 *   lastAttempt: number
 * }}
 */
var SavePageRequest;

cr.define('offlineInternals', function() {
  'use strict';

  /** @type {!Array<OfflinePage>} */
  var offlinePages = [];

  /** @type {!Array<SavePageRequest>} */
  var savePageRequests = [];

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
      cell.textContent = pages[i].onlineUrl;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = pages[i].namespace;
      row.appendChild(cell);

      cell = document.createElement('td');
      cell.textContent = Math.round(pages[i].size / 1024);
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
   * Refresh all displayed information.
   */
  function refreshAll() {
    cr.sendWithPromise('getStoredPagesInfo').then(fillStoredPages);
    cr.sendWithPromise('getRequestQueueInfo').then(fillRequestQueue);
  }

  /**
   * Delete all pages in the offline store.
   */
  function deleteAllPages() {
    cr.sendWithPromise('deleteAllPages').then(pagesDeleted);
  }

  /**
   * Callback when pages are deleted.
   * @param {string} status The status of the request.
   */
  function pagesDeleted(status) {
    $('page-actions-info').textContent = status;
    cr.sendWithPromise('getStoredPagesInfo').then(fillStoredPages);
  }

  /**
   * Helper function to JSON-escape and add quotes around a string.
   * @param {string} strObj The obj to escape and add quotes around.
   * @return {string} The escaped string.
   */
  function escapeString(strObj) {
    // CSV single quotes are encoded as "". There can also be commas.
    return '"' + strObj.replace(/"/g, '""') + '"';
  }

  /**
   * Downloads all the stored page and request queue information into a file.
   * TODO(chili): Create a CSV writer that can abstract out the line joining.
   */
  function download() {
    var json = JSON.stringify({
      offlinePages: offlinePages,
      savePageRequests: savePageRequests
    }, null, 2);

    window.open(
        'data:application/json,' + encodeURIComponent(json),
        'dump.json');
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

    cr.sendWithPromise('deleteSelectedPages', selectedIds).then(pagesDeleted);
  }

  function initialize() {
    $('clear-all').onclick = deleteAllPages;
    $('clear-selected').onclick = deleteSelectedPages;
    $('refresh').onclick = refreshAll;
    $('download').onclick = download;
    refreshAll();
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', offlineInternals.initialize);
