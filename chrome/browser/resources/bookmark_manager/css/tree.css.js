// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var a = 7;
  var a2 = a / 2;
  var ctx = document.getCSSCanvasContext('2d', 'triangle-filled', a2 + 2, a + 1);

  ctx.fillStyle = '#000';
  ctx.translate(.5, .5);
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(0, a);
  ctx.lineTo(a2, a2);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();

  var ctx = document.getCSSCanvasContext('2d', 'triangle-empty', a2 + 2, a + 1);

  ctx.strokeStyle = '#999';
  ctx.lineWidth  = 1.2;
  ctx.translate(.5, .5);
  ctx.fillStyle = '#000';
  ctx.beginPath();


  ctx.moveTo(0, 0);
  ctx.lineTo(0, a);
  ctx.lineTo(a2, a2);
  ctx.closePath();
  ctx.stroke();

  var ctx = document.getCSSCanvasContext('2d', 'triangle-hover', a2 + 2 + 4, a + 1 + 4);

  ctx.shadowColor = 'hsl(214,91%,89%)'
  ctx.shadowBlur = 3;
  ctx.shadowOffsetX = 1;
  ctx.shadowOffsetY = 0;

  ctx.strokeStyle = 'hsl(214,91%,79%)';
  ctx.lineWidth  = 1.2;
  ctx.translate(.5 + 2, .5 + 2);
  ctx.fillStyle = '#000';
  ctx.beginPath();

  ctx.moveTo(0, 0);
  ctx.lineTo(0, a);
  ctx.lineTo(a2, a2);
  ctx.closePath();
  ctx.stroke();
})();

// We need to generate CSS for the indentation.
(function() {
  // We need to generat the following
  //.tree-item > * > .tree-item > .tree-row {
  //  -webkit-padding-start: 20px;
  //}

  //.tree-item > * .tree-item > * .tree-item > * > .tree-item > .tree-row {
  //  -webkit-padding-start: 60px;
  //}
  var style = document.createElement('style');

  function repeat(s, n) {
    return Array(n + 1).join(s);
  }

  var s = '';
  for (var i = 1; i < 10; i++) {
    s += repeat('.tree-item > * ', i) + '.tree-item > .tree-row {\n' +
         '-webkit-padding-start:' + i * 20 + 'px\n' +
         '}\n';
  }
  style.textContent = s;
  document.documentElement.firstElementChild.appendChild(style);
})();
