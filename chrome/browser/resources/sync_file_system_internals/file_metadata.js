// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor File Metadata per Extension ID.
 */
var FileMetadata = (function() {
'use strict';

var FileMetadata = {};

/**
 * Get File Metadata from handler.
 */
function getFileMetadata() {
  chrome.send('getFileMetadata');
}

/**
 * Renders result of getFileMetadata as a table.
 */
FileMetadata.onGetFileMetadata = function(fileMetadataMap) {
  var itemContainer = $('file-metadata-entries');
  itemContainer.textContent = '';

  for (var i = 0; i < fileMetadataMap.length; i++) {
    var metadatEntry = fileMetadataMap[i];
    var tr = document.createElement('tr');
    tr.appendChild(createElementFromText('td', metadatEntry.origin));
    tr.appendChild(createElementFromText('td', metadatEntry.status));
    tr.appendChild(createElementFromText('td', metadatEntry.type));
    tr.appendChild(createElementFromText('td', metadatEntry.title));
    tr.appendChild(createElementFromText('td', metadatEntry.details));
    itemContainer.appendChild(tr);
  }
}

// TODO(calvinlo): Move to helper file so it doesn't need to be duplicated.
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

function main() {
  getFileMetadata();
  $('refresh-file-metadata').addEventListener('click', getFileMetadata);
}

document.addEventListener('DOMContentLoaded', main);
return FileMetadata;
})();
