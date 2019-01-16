// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
let Point;

/**
 * @typedef {{
 *   width: number,
 *   height: number
 * }}
 */
let Size;

class Viewport {
  /**
   * @param {number} zoom
   * @return {{width: number, height: number}}
   */
  getDocumentDimensions(zoom) {}

  /**
   * @param {!Point} point
   * @return {boolean}
   */
  isPointInsidePage(point) {}

  /** @return {!Point} */
  get position() {}

  /** @return {!Size} */
  get size() {}

  /** @return {number} */
  get zoom() {}
}

/** @type {Object} */
Viewport.PAGE_SHADOW;

/** @type {number} */
Viewport.PAGE_SHADOW.top;

/** @type {number} */
Viewport.PAGE_SHADOW.left;
