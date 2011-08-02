// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
function makeLogEntryNode(entry) {
  var timeNode = document.createElement('td');
  timeNode.textContent = entry.date;

  var submoduleNode = document.createElement('td');
  submoduleNode.textContent = entry.submodule;

  var eventNode = document.createElement('td');
  eventNode.textContent = entry.event;

  var details = document.createElement('pre');
  details.textContent = JSON.stringify(entry.details, null, 2);
  var detailsNode = document.createElement('td');
  detailsNode.appendChild(details);

  var node = document.createElement('tr');
  node.appendChild(timeNode);
  node.appendChild(submoduleNode);
  node.appendChild(eventNode);
  node.appendChild(detailsNode);

  return node;
}

var syncEvents = document.getElementById('sync-events');

var entries = chrome.sync.log.entries;
for (var i = 0; i < entries.length; ++i) {
  syncEvents.appendChild(makeLogEntryNode(entries[i]));
}

chrome.sync.log.addEventListener('append', function(event) {
  syncEvents.appendChild(makeLogEntryNode(event.detail));
});
})();
