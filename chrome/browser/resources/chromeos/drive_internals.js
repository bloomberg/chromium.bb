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
 */
function updateGCacheContents(gcacheContents) {
  var tbody = $('gcache-contents');
  // Add a header row.
  var tr = document.createElement('tr');
  tr.appendChild(createElementFromText('th', 'Path'));
  tr.appendChild(createElementFromText('th', 'Size'));
  tr.appendChild(createElementFromText('th', 'Last Modified'));
  tbody.appendChild(tr);

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
});
