// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Custom version of chrome://resources/css/tree.css.js, adding support for
 * inverted arrow icons.
 */
(function() {
  /**
   * @type {number}
   * @const
   */
  var WIDTH = 14;

  /**
   * @type {number}
   * @const
   */
  var HEIGHT = WIDTH / 2 + 2;

  /**
   * @type {number}
   * @const
   */
  var MARGIN = 1;

  /**
   * @param {string} name CSS canvas identifier.
   * @param {string} backgroundColor Background color.
   * @param {string} strokeColor Outline color.
   */
  function prepareTriangle(name, backgroundColor, strokeColor) {
    var ctx = document.getCSSCanvasContext('2d',
                                           name,
                                           WIDTH + MARGIN * 2,
                                           HEIGHT + MARGIN * 2);

    ctx.fillStyle = backgroundColor;
    ctx.strokeStyle = strokeColor;
    ctx.translate(MARGIN, MARGIN);

    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(0, 2);
    ctx.lineTo(WIDTH / 2, HEIGHT);
    ctx.lineTo(WIDTH, 2);
    ctx.lineTo(WIDTH, 0);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();
  }

  prepareTriangle(
      'tree-triangle', 'rgba(122, 122, 122, 0.6)', 'rgba(0, 0, 0, 0)');
  prepareTriangle('tree-triangle-inverted', '#ffffff', '#ffffff');
})();
