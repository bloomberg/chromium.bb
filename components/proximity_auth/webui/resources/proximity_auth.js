// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(tengs): This page is just a placeholder. The real page still needs to be
// built.

function LSystem(rules, startState, iterations) {
  symbols = startState.split('').map(function(character) {
    return { c: character, i: 0 };
  });

  while (true) {
    var index = -1;
    for (var i = 0; i < symbols.length; ++i) {
      var symbol = symbols[i];
      if (symbol.i < iterations && rules[symbol.c] != null) {
        index = i;
        break;
      }
    }
    if (index == -1)
      break;

    var symbol = symbols[index];
    var newSymbols = rules[symbol.c].apply(symbol).split('').map(function(c) {
      return { c: c, i: symbol.i + 1 };
    });

    Array.prototype.splice.apply(symbols, [index, 1].concat(newSymbols));
  }

  return symbols.map(function(symbol) {
    return symbol.c;
  });
};

var rules = {
  'X': function(x) { return 'X+YF'; },
  'Y': function(y) { return 'FX-Y'; },
}
var startState = 'FX+FX+';
var iterations = 10;

function draw() {
  var canvas = document.getElementById('canvas');
  canvas.width = canvas.offsetWidth;
  canvas.height = canvas.offsetHeight;

  var canvasWidth = canvas.offsetWidth;
  var canvasHeight = canvas.offsetHeight;
  var context = canvas.getContext('2d');

  context.lineWidth = 2;
  var segmentWidth = 0.015 * canvasHeight;
  var pos = {
    x: 0.5 * canvasWidth,
    y: 0.25 * canvasHeight,
  };
  var dir = { x: 1, y: 0, };

  var commands = LSystem(rules, startState, iterations);
  var drawCommand = function() {
    var command = commands.shift();
    if (command === 'F') {
      context.beginPath();
      context.moveTo(pos.x, pos.y);

      pos = {
        x: pos.x + dir.x * segmentWidth,
        y: pos.y + dir.y * segmentWidth,
      };

      context.lineTo(pos.x, pos.y);
      var r = Math.round(pos.x / canvasWidth * 256);
      var b = Math.round(pos.y / canvasHeight * 256);
      var g = 180;
      context.strokeStyle = 'rgb(' + r + ',' + g + ',' + b + ')';
      context.stroke();
    } else if (command === '+') {
      dir = { x: -dir.y, y: dir.x }
    } else if (command === '-') {
      dir = { x: dir.y, y: -dir.x }
    }
  }

  window.setInterval(drawCommand, 10);
}

document.addEventListener("DOMContentLoaded", draw);
