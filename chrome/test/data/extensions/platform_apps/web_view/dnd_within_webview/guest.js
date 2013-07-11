// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
var $ = function(id) {
  return document.getElementById(id);
};

var dropped = false;
var waitingToPingEmbedder = false;
var dropData = 'Uninitialized';

// Also called from web_view_interactive_ui_tests.cc.
window.pingEmbedder = function() {
  if (dropped) {
    embedder.postMessage(JSON.stringify(['guest-got-drop', dropData]), '*');
  } else {
    waitingToPingEmbedder = true;
  }
};

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data)[0];
  if (data == 'create-channel') {
    embedder.postMessage(JSON.stringify(['connected']), '*');
  }
});

var doPostMessage = function(msg) {
  embedder.postMessage(JSON.stringify([msg]), '*');
};

var setUpGuest = function() {
  // Select the text to drag.
  var selection = window.getSelection();
  var range = document.createRange();
  range.selectNodeContents($('dragMe'));
  selection.removeAllRanges();
  selection.addRange(range);

  // Whole bunch of drag/drop events on destination node follows.
  // We only need to make sure we preventDefault in dragenter and
  // dragover.
  var destNode = $('dest');

  destNode.addEventListener('dragenter', function(e) {
    e.preventDefault();
  }, false);

  destNode.addEventListener('dragover', function(e) {
    e.preventDefault();
  }, false);

  destNode.addEventListener('dragleave', function(e) {
  }, false);

  destNode.addEventListener('drop', function (e) {
    dropped = true;
    dropData = e.dataTransfer.getData('Text');
    if (waitingToPingEmbedder) {
      waitingToPingEmbedder = false;
      window.pingEmbedder();
    }
  }, false);

  destNode.addEventListener('dragend', function (e) {
  }, false);
};

window.onload = function() {
  setUpGuest();
};

