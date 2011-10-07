// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var metadataProvider = new MetadataProvider('../metadata_dispatcher.js');

function toggleSize(check) {
  var iframe = document.querySelector('.gallery-frame');
  iframe.classList.toggle('chromebook');
}

var mockActions = [
  {
    title: 'Send',
    iconUrl: 'http://google.com/favicon.ico',
    execute: function() { alert('Sending is not supported') }
  }];


var mockDisplayStrings = {
  GALLERY_EDIT: 'Edit',
  GALLERY_SHARE: 'Share',
  GALLERY_ENTER_WHEN_DONE: 'Press Enter when done',
  GALLERY_AUTOFIX: 'Auto-fix',
  GALLERY_FIXED: 'Fixed',
  GALLERY_CROP: 'Crop',
  GALLERY_EXPOSURE: 'Brightness',
  GALLERY_BRIGHTNESS: 'Brightness',
  GALLERY_CONTRAST: 'Contrast',
  GALLERY_ROTATE_LEFT: 'Left',
  GALLERY_ROTATE_RIGHT: 'Right',
  GALLERY_UNDO: 'Undo',
  GALLERY_REDO: 'Redo'
};

function loadGallery(items) {
  if (!items) items = [createTestGrid()];

  var iframe = document.querySelector('.gallery-frame');
  var contentWindow = iframe.contentWindow;

  contentWindow.ImageUtil.trace.bindToDOM(
      document.querySelector('.debug-output'));

  contentWindow.Gallery.open(
      null,  // No local file access
      items,
      items[0],
      function() {},  // Do nothing on Close
      metadataProvider,
      mockActions,
      function(id) { return mockDisplayStrings[id] || id } );

  iframe.focus();
}

function createTestGrid() {
  var canvas = document.createElement('canvas');
  canvas.width = 1000;
  canvas.height = 1000;

  var context = canvas.getContext('2d');

  var imageData = context.getImageData(0, 0, canvas.width, canvas.height);
  fillGradient(imageData);
  context.putImageData(imageData, 0, 0);

  drawTestGrid(context);

  return canvas;
}

function fillGradient(imageData) {
  var data = imageData.data;
  var width = imageData.width;
  var height = imageData.height;

  var maxX = width - 1;
  var maxY = height - 1;
  var maxDist = maxX + maxY;
  var values = [];
  for (var i = 0; i <= maxDist; i++) {
    values.push(Math.max(0, Math.min(0xFF, Math.round(i/maxDist*255))));
  }

  var index = 0;
  for (var y = 0; y != height; y++)
    for (var x = 0; x != width; x++) {
      data[index++] = values[maxX - x + maxY - y];
      data[index++] = values[x + maxY - y];
      data[index++] = values[maxX - x + y];
      data[index++] = 0xFF;
    }
}

function drawTestGrid(context) {
  var width = context.canvas.width;
  var height = context.canvas.height;

  context.textBaseline = 'top';

  const STEP = 100;
  for (var y = 0; y < height; y+= STEP) {
    for (var x = 0; x < width; x+= STEP) {
      context.strokeRect(x + 0.5, y + 0.5, STEP, STEP);
      context.strokeText(x + ',' + y, x + 2, y);
    }
  }
}