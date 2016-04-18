// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.SnippetsInternals', function() {
  'use strict';

  function initialize() {
    function submitDownload(event) {
      chrome.send('download', [$('hosts-input').value]);
      event.preventDefault();
    }

    $('submit-download').addEventListener('click', submitDownload);

    function submitClear(event) {
      chrome.send('clear');
      event.preventDefault();
    }

    $('submit-clear').addEventListener('click', submitClear);

    function submitClearDiscarded(event) {
      chrome.send('clearDiscarded');
      event.preventDefault();
    }

    $('discarded-snippets-clear').addEventListener('click',
                                                   submitClearDiscarded);

    chrome.send('loaded');
  }

  function receiveProperty(propertyId, value) {
    $(propertyId).textContent = value;
  }

  function receiveHosts(hosts) {
    displayList(hosts, 'hosts');

    $('hosts-input').value = hosts.list.map(host => host.url).join(' ');
  }

  function receiveSnippets(snippets) {
    displayList(snippets, 'snippets', 'snippet-title');
  }

  function receiveDiscardedSnippets(discardedSnippets) {
    displayList(discardedSnippets, 'discarded-snippets',
                'discarded-snippet-title');
  }

  function displayList(object, domId, titleClass) {
    jstProcess(new JsEvalContext(object), $(domId));

    var text;
    var display;

    if (object.list.length > 0) {
      text = '';
      display = 'inline';
    } else {
      text = 'The list is empty.';
      display = 'none';
    }

    if ($(domId + '-empty')) $(domId + '-empty').textContent = text;
    if ($(domId + '-clear')) $(domId + '-clear').style.display = display;

    function trigger(event) {
      // The id of the snippet is stored to 'snippet-id' attribute of the link.
      var id = event.currentTarget.getAttribute('snippet-id');
      $(id).classList.toggle('snippet-hidden');
      event.preventDefault();
    }

    var links = document.getElementsByClassName(titleClass);
    for (var link of links) {
      link.addEventListener('click', trigger);
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    receiveProperty: receiveProperty,
    receiveHosts: receiveHosts,
    receiveSnippets: receiveSnippets,
    receiveDiscardedSnippets: receiveDiscardedSnippets,
  };
});

document.addEventListener('DOMContentLoaded',
                          chrome.SnippetsInternals.initialize);
