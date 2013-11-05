// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';
var toggleDisplay = function() {
  var detailsNode = this.parentNode.getElementsByClassName('details')[0];
  if (detailsNode.classList.contains('hidden')) {
    detailsNode.classList.remove('hidden');
  } else {
    detailsNode.classList.add('hidden');
  }
}

var makeLogEntryNode = function(entry) {
  var timeNode = document.createElement('td');
  timeNode.textContent = entry.date;

  var submoduleNode = document.createElement('td');
  submoduleNode.textContent = entry.submodule;

  var eventNode = document.createElement('td');
  eventNode.textContent = entry.event;

  var details = document.createElement('pre');
  details.textContent = JSON.stringify(entry.details, null, 2);
  details.className = 'details';
  details.classList.add('details');
  details.classList.add('hidden');
  var detailsToggleButton = document.createElement('button');
  detailsToggleButton.addEventListener('click', toggleDisplay, false);
  detailsToggleButton.textContent = 'Show/Hide Details';
  var detailsNode = document.createElement('td');
  detailsNode.appendChild(detailsToggleButton);
  detailsNode.appendChild(details);

  var node = document.createElement('tr');
  node.appendChild(timeNode);
  node.appendChild(submoduleNode);
  node.appendChild(eventNode);
  node.appendChild(detailsNode);

  return node;
}

var syncEvents = $('sync-events');

var entries = chrome.sync.log.entries;
for (var i = 0; i < entries.length; ++i) {
  syncEvents.appendChild(makeLogEntryNode(entries[i]));
}

chrome.sync.log.addEventListener('append', function(event) {
  syncEvents.appendChild(makeLogEntryNode(event.detail));
});
})();
