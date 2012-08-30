// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Updates the Authentication Status section.
 * @param {Object} authStatus Dictionary containing auth status.
 */
function updateAuthStatus(authStatus) {
  $('has-refresh-token').textContent = authStatus['has-refresh-token'];
  $('has-access-token').textContent = authStatus['has-access-token'];
}

/**
 * Updates the GCache Contents section.
 * @param {Array} gcacheContents List of dictionaries describing metadata
 * of files and directories under the GCache directory.
 * @param {Object} gcacheSummary Dictionary of summary of GCache.
 */
function updateGCacheContents(gcacheContents, gcacheSummary) {
  var tbody = $('gcache-contents');
  for (var i = 0; i < gcacheContents.length; i++) {
    var entry = gcacheContents[i];
    var tr = document.createElement('tr');

    // Add some suffix based on the type.
    var path = entry.path;
    if (entry.is_directory)
      path += '/';
    else if (entry.is_symbolic_link)
      path += '@';

    tr.appendChild(createElementFromText('td', path));
    tr.appendChild(createElementFromText('td', entry.size));
    tr.appendChild(createElementFromText('td', entry.last_modified));
    tbody.appendChild(tr);
  }

  $('gcache-summary-total-size').textContent = gcacheSummary['total_size'];
}

/**
 * Updates the File System Contents section. The function is called from the
 * C++ side repeatedly with contents of a directory.
 * @param {string} directoryContentsAsText Pre-formatted string representation
 * of contents a directory in the file system.
 */
function updateFileSystemContents(directoryContentsAsText) {
  var div = $('file-system-contents');
  div.appendChild(createElementFromText('pre', directoryContentsAsText));
}

/**
 * Updates the Cache Contents section.
 * @param {Object} cacheEntry Dictionary describing a cache entry.
 * The function is called from the C++ side repeatedly.
 */
function updateCacheContents(cacheEntry) {
  var tr = document.createElement('tr');
  tr.appendChild(createElementFromText('td', cacheEntry.resource_id));
  tr.appendChild(createElementFromText('td', cacheEntry.md5));
  tr.appendChild(createElementFromText('td', cacheEntry.is_present));
  tr.appendChild(createElementFromText('td', cacheEntry.is_pinned));
  tr.appendChild(createElementFromText('td', cacheEntry.is_dirty));
  tr.appendChild(createElementFromText('td', cacheEntry.is_mounted));
  tr.appendChild(createElementFromText('td', cacheEntry.is_persistent));

  $('cache-contents').appendChild(tr);
}

/**
 * Updates the Local Storage summary.
 * @param {Object} localStorageSummary Dictionary describing the status of local
 * stogage.
 */
function updateLocalStorageUsage(localStorageSummary) {
  var freeSpaceInMB = localStorageSummary.free_space / (1 << 20);
  $('local-storage-freespace').innerText = freeSpaceInMB;
}

/**
 * Updates the summary about in-flight operations.
 * @param {Array} inFlightOperations List of dictionaries describing the status
 * of in-flight operations.
 */
function updateInFlightOperations(inFlightOperations) {
  var container = $('in-flight-operations-contents');

  // Reset the table.
  var existingNodes = container.childNodes;
  for (var i = 0; i < existingNodes.length; i++) {
    var node = existingNodes[i];
    if (node.className == 'in-flight-operation')
      container.removeChild(node);
  }

  // Add in-flight operations.
  for (var i = 0; i < inFlightOperations.length; i++) {
    var operation = inFlightOperations[i];
    var tr = document.createElement('tr');
    tr.className = 'in-flight-operation';
    tr.appendChild(createElementFromText('td', operation.operation_id));
    tr.appendChild(createElementFromText('td', operation.operation_type));
    tr.appendChild(createElementFromText('td', operation.file_path));
    tr.appendChild(createElementFromText('td', operation.transfer_state));
    tr.appendChild(createElementFromText('td', operation.start_time));
    var progress = operation.progress_current + '/' + operation.progress_total;
    if (operation.progress_total > 0) {
      progress += ' (' +
          (operation.progress_current / operation.progress_total * 100) + '%)';
    }
    tr.appendChild(createElementFromText('td', progress));

    container.appendChild(tr);
  }
}

/**
 * Creates an element named |elementName| containing the content |text|.
 * @param {string} elementName Name of the new element to be created.
 * @param {string} text Text to be contained in the new element.
 * @return {HTMLElement} The newly created HTML element.
 */
function createElementFromText(elementName, text) {
  var element = document.createElement(elementName);
  element.appendChild(document.createTextNode(text));
  return element;
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('pageLoaded');
  window.setInterval(function() {
      chrome.send('periodicUpdate');
    }, 1000);
});
