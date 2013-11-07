// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function highlightIfChanged(node, oldVal, newVal) {
  function clearHighlight() {
    this.removeAttribute('highlighted');
  }

  var oldStr = oldVal.toString();
  var newStr = newVal.toString();
  if (oldStr != '' && oldStr != newStr) {
    // Note the addListener function does not end up creating duplicate
    // listeners.  There can be only one listener per event at a time.
    // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
    node.addEventListener('webkitAnimationEnd', clearHighlight, false);
    node.setAttribute('highlighted');
  }
}

(function() {
// Contains the latest snapshot of sync about info.
chrome.sync.aboutInfo = {};

// TODO(akalin): Make aboutInfo have key names likeThis and not
// like_this.
function refreshAboutInfo(aboutInfo) {
  chrome.sync.aboutInfo = aboutInfo;
  var aboutInfoDiv = $('aboutInfo');
  jstProcess(new JsEvalContext(aboutInfo), aboutInfoDiv);
}

function onLoad() {
  $('status-data').hidden = true;
  chrome.sync.getAboutInfo(refreshAboutInfo);

  chrome.sync.onServiceStateChanged.addListener(function() {
    chrome.sync.getAboutInfo(refreshAboutInfo);
  });

  var dumpStatusButton = $('dump-status');
  dumpStatusButton.addEventListener('click', function(event) {
    var aboutInfo = chrome.sync.aboutInfo;
    if (!$('include-ids').checked) {
      aboutInfo.details = chrome.sync.aboutInfo.details.filter(function(el) {
        return !el.is_sensitive;
      });
    }
    var data = '';
    data += new Date().toString() + '\n';
    data += '======\n';
    data += 'Status\n';
    data += '======\n';
    data += JSON.stringify(aboutInfo, null, 2) + '\n';

    $('status-text').value = data;
    $('status-data').hidden = false;
  });

  var importStatusButton = $('import-status');
  importStatusButton.addEventListener('click', function(event) {
    $('status-data').hidden = false;
    if ($('status-text').value.length == 0) {
      $('status-text').value = 'Paste sync status dump here then click import.';
      return;
    }

    // First remove any characters before the '{'.
    var data = $('status-text').value;
    var firstBrace = data.indexOf('{');
    if (firstBrace < 0) {
      $('status-text').value = 'Invalid sync status dump.';
      return;
    }
    data = data.substr(firstBrace);

    // Remove listeners to prevent sync events from overwriting imported data.
    chrome.sync.onServiceStateChanged.removeListeners();

    var aboutInfo = JSON.parse(data);
    refreshAboutInfo(aboutInfo);
  });
}

document.addEventListener('DOMContentLoaded', onLoad, false);
})();
