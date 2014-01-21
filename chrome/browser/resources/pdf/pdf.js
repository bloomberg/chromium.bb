// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

var plugin;
var sizer;

function onScroll() {
  var scrollMessage = {
    type: 'scroll',
    xOffset: window.pageXOffset,
    yOffset: window.pageYOffset
  };
  plugin.postMessage(scrollMessage);
}

function handleMessage(message) {
  if (message.data['type'] == 'document_dimensions') {
    if (sizer.style.height != message.data['document_height'] + 'px') {
      sizer.style.height = message.data['document_height'] + 'px';
      sizer.style.width = message.data['document_width'] + 'px';
    }
  }
}

function load() {
  window.addEventListener('scroll',
      function() { webkitRequestAnimationFrame(onScroll); });

  // The pdf location is passed in the document url in the format:
  // http://<.../pdf.html>?<pdf location>.
  var url = window.location.search.substring(1);
  plugin = document.createElement('object');
  plugin.setAttribute('width', '100%');
  plugin.setAttribute('height', '100%');
  plugin.setAttribute('type', 'application/x-google-chrome-pdf');
  plugin.setAttribute('src', url);
  plugin.style.zIndex = '1';
  plugin.style.position = 'fixed';
  plugin.addEventListener('message', handleMessage, false);
  document.body.appendChild(plugin);

  sizer = document.createElement('div');
  sizer.style.zIndex = '0';
  sizer.style.position = 'absolute';
  sizer.style.width = '100%';
  sizer.style.height = '100%';
  document.body.appendChild(sizer);
}

load();

})();
