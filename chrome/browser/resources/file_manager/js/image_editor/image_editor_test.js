// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var editor;

function createEditor() {
  editor = new ImageEditor(document.getElementById('frame'), save, close);
  window.addEventListener('resize', resize);
  load(createTestGrid());
}

function load(source) {
  editor.onModeCancel();
  editor.getBuffer().load(source);
}

function save(canvas) {
  var blob = ImageEncoder.getBlob(canvas, 'image/jpeg');
  console.log('Blob size: ' + blob.size + ' bytes');
}

function close() {
  document.body.innerHTML = 'Editor closed, hit reload';
}

function resize() {
  var wrapper = document.getElementsByClassName('canvas-wrapper')[0];
  editor.getBuffer().resizeScreen(
      wrapper.clientWidth, wrapper.clientHeight, true);
}


function getUrlField() {
  return document.getElementById('imageUrl');
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
  var values = ImageUtil.precomputeByteFunction( function(dist) {
    return Math.round(dist/maxDist*255);
  }, maxDist);

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

// It is nice to be able to test on Safari.
// Safari 5 does not have Function.bind, so we are adding that just in case.
Function.prototype.bind = function(thisObject) {
  var func = this;
  var args = Array.prototype.slice.call(arguments, 1);
  function bound() {
   return func.apply(
       thisObject, args.concat(Array.prototype.slice.call(arguments, 0)));
  }
  bound.toString = function() { return "bound: " + func; };
  return bound;
};

