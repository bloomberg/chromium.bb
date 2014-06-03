// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../../third_party/polymer/platform/platform.js">
<include src="../../../../third_party/polymer/polymer/polymer.js">

// Defines the file-systems element.
Polymer('file-systems', {
  /**
   * Called when the element is created.
   */
  ready: function() {
  },

  /**
   * Selects an active file system from the list.
   * @param {Event} event Event.
   * @param {number} detail Detail.
   * @param {HTMLElement} sender Sender.
   */
  rowClicked: function(event, detail, sender) {
    var requestEventsNode = document.querySelector('#request-events');
    requestEventsNode.hidden = false;
    requestEventsNode.model = [];

    console.log(sender.dataset.extensionId, sender.dataset.id);
    chrome.send('selectFileSystem', [sender.dataset.extensionId,
      sender.dataset.id]);
  },

  /**
   * List of provided file system information maps.
   * @type {Array.<Object>}
   */
  model: []
});

// Defines the request-log element.
Polymer('request-events', {
  /**
   * Called when the element is created.
   */
  ready: function() {
  },

  /**
   * Formats time to a hh:mm:ss.xxxx format.
   * @param {Date} time Input time.
   * @return {string} Output string in a human-readable format.
   */
  formatTime: function(time) {
    return ('0' + time.getHours()).slice(-2) + ':' +
           ('0' + time.getMinutes()).slice(-2) + ':' +
           ('0' + time.getSeconds()).slice(-2) + '.' +
           ('000' + time.getMilliseconds()).slice(-3);
  },

  /**
   * Formats a boolean value to human-readable form.
   * @param {boolean=} opt_hasMore Input value.
   * @return {string} Output string in a human-readable format.
   */
  formatHasMore: function(opt_hasMore) {
    if (opt_hasMore == undefined)
      return '';

    return opt_hasMore ? 'HAS_MORE' : 'LAST';
  },

  /**
   * List of events.
   * @type {Array.<Object>}
   */
  model: []
});

/*
 * Updates the mounted file system list.
 * @param {Array.<Object>} fileSystems Array containing provided file system
 *     information.
 */
function updateFileSystems(fileSystems) {
  var fileSystemsNode = document.querySelector('#file-systems');
  fileSystemsNode.model = fileSystems;
}

/**
 * Called when a request is created.
 * @param {Object} event Event.
 */
function onRequestEvent(event) {
  var requestEventsNode = document.querySelector('#request-events');
  event.time = new Date(event.time);  // Convert to a real Date object.
  requestEventsNode.model.push(event);
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('updateFileSystems');

  // Refresh periodically.
  setInterval(function() {
    chrome.send('updateFileSystems');
  }, 1000);
});
