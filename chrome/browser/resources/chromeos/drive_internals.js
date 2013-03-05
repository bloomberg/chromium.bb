// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Updates the Drive related Flags section.
 * @param {Array} flags List of dictionaries describing flags.
 */
function updateDriveRelatedFlags(flags) {
  var ul = $('drive-related-flags');
  updateKeyValueList(ul, flags);
}

/**
 * Updates the Drive related Preferences section.
 * @param {Array} preferences List of dictionaries describing preferences.
 */
function updateDriveRelatedPreferences(preferences) {
  var ul = $('drive-related-preferences');
  updateKeyValueList(ul, preferences);
}

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
 * Updates the summary about about resource.
 * @param {Object} aboutResource Dictionary describing about resource.
 */
function updateAboutResource(aboutResource) {
  var quotaTotalInMb = aboutResource['account-quota-total'] / (1 << 20);
  var quotaUsedInMb = aboutResource['account-quota-used'] / (1 << 20);

  $('account-quota-info').textContent =
      quotaUsedInMb + ' / ' + quotaTotalInMb + ' (MB)';
  $('account-largest-changestamp-remote').textContent =
      aboutResource['account-largest-changestamp-remote'];
  $('root-resource-id').textContent = aboutResource['root-resource-id'];
}

/**
 * Updates the summary about app list.
 * @param {Object} appList Dictionary describing app list.
 */
function updateAppList(appList) {
  $('app-list-etag').textContent = appList['etag'];

  var itemContainer = $('app-list-items');
  for (var i = 0; i < appList['items'].length; i++) {
    var app = appList['items'][i];
    var tr = document.createElement('tr');
    tr.className = 'installed-app';
    tr.appendChild(createElementFromText('td', app.name));
    tr.appendChild(createElementFromText('td', app.application_id));
    tr.appendChild(createElementFromText('td', app.object_type));
    tr.appendChild(createElementFromText('td', app.supports_create));

    itemContainer.appendChild(tr);
  }
}

/**
 * Updates the local cache information about account metadata.
 * @param {Object} localMetadata Dictionary describing account metadata.
 */
function updateLocalMetadata(localMetadata) {
  $('account-largest-changestamp-local').textContent =
      localMetadata['account-largest-changestamp-local'];
  $('account-metadata-loaded').textContent =
      localMetadata['account-metadata-loaded'].toString() +
      (localMetadata['account-metadata-refreshing'] ? ' (refreshing)' : '');
}

/**
 * Updates the summary about delta update status.
 * @param {Object} deltaUpdateStatus Dictionary describing delta update status.
 */
function updateDeltaUpdateStatus(deltaUpdateStatus) {
  $('push-notification-enabled').textContent =
        deltaUpdateStatus['push-notification-enabled'];
  $('polling-interval-sec').textContent =
        deltaUpdateStatus['polling-interval-sec'];
  $('last-update-check-time').textContent =
        deltaUpdateStatus['last-update-check-time'];
  $('last-update-check-error').textContent =
        deltaUpdateStatus['last-update-check-error'];
}

/**
 * Updates the event log section.
 * @param {Array} log Array of events.
 */
function updateEventLog(log) {
  var ul = $('event-log');
  updateKeyValueList(ul, log);
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

/**
 * Updates <ul> element with the given key-value list.
 * @param {HTMLElement} ul <ul> element to be modified.
 * @param {Array} list List of dictionaries containing 'key' and 'value'.
 */
function updateKeyValueList(ul, list) {
  for (var i = 0; i < list.length; i++) {
    var flag = list[i];
    var text = flag.key;
    if (list.value != '')
      text += ': ' + flag.value;

    var li = createElementFromText('li', text);
    ul.appendChild(li);
  }
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('pageLoaded');

  // Update the table of contents.
  var toc = $('toc');
  var sections = document.getElementsByTagName('h2');
  for (var i = 0; i < sections.length; i++) {
    var section = sections[i];
    var a = createElementFromText('a', section.textContent);
    a.href = '#' + section.id;
    var li = document.createElement('li');
    li.appendChild(a);
    toc.appendChild(li);
  }

  $('button-clear-access-token').addEventListener('click', function() {
    chrome.send('clearAccessToken');
  });

  $('button-clear-refresh-token').addEventListener('click', function() {
    chrome.send('clearRefreshToken');
  });

  window.setInterval(function() {
      chrome.send('periodicUpdate');
    }, 1000);
});
