/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/**
 * @fileoverview Collection of functions which operate on DOM.
 */

var domUtils = window['domUtils'] || {};

/**
 * Returns pageX and pageY of the given element.
 *
 * @param {Element} element An element of which the top-left position is to be
 *     returned in the coordinate system of the document page.
 * @return {Object} A point object which has {@code x} and {@code y} fields.
 */
domUtils.pageXY = function(element) {
  var x = 0, y = 0;
  for (; element; element = element.offsetParent) {
    x += element.offsetLeft;
    y += element.offsetTop;
  }
  return {'x': x, 'y': y};
};

/**
 * Returns pageX and pageY of the given event.
 *
 * @param {Event} event An event of which the position is to be returned in
 *     the coordinate system of the document page.
 * @return {Object} A point object which has {@code x} and {@code y} fields.
 */
domUtils.pageXYOfEvent = function(event) {
  return (event.pageX != null && event.pageY != null) ?
      {'x': event.pageX, 'y': event.pageY} :
      {'x': event.clientX + document.body.scrollLeft +
            document.documentElement.scrollLeft,
       'y': event.clientY + document.body.scrollTop +
            document.documentElement.scrollTop};
};
