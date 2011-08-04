// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function load(source) {
  var editorWindow =
        document.getElementsByClassName('editor-frame')[0].contentWindow;

  // ImageEditor makes extensive use of Function.prototype.bind.
  // Add it if it does not exist (as in Safari 5).
  if (!editorWindow.Function.prototype.bind) {
    editorWindow.Function.prototype.bind = bindToObject;
  }

  editorWindow.ImageUtil.trace.bindToDOM(
      document.getElementsByClassName('debug-output')[0]);

  editorWindow.ImageEditor.open(save, close, source);
}

function save(blob) {
  console.log('Saving ' + blob.size + ' bytes');
}

function close() {
  document.body.innerHTML = 'Editor closed, hit reload';
}

function getUrlField() {
  return document.getElementsByClassName('image-url')[0];
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

function bindToObject(thisObject) {
  var func = this;
  var args = Array.prototype.slice.call(arguments, 1);
  function bound() {
   return func.apply(
       thisObject, args.concat(Array.prototype.slice.call(arguments, 0)));
  }
  bound.toString = function() { return "bound: " + func; };
  return bound;
};

