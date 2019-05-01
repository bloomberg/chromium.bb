// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of helper functions when dealing with rects. */
const RectHelper = {
  /**
   * Finds the bottom of a rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {number}
   */
  bottom: (rect) => rect.top + rect.height,

  /**
   * Finds the right of a rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {number}
   */
  right: (rect) => rect.left + rect.width,

  /**
   * Increases the size of |outer| to entirely enclose |inner|, with |padding|
   * buffer on each side.
   * @param {number} padding
   * @param {chrome.accessibilityPrivate.ScreenRect=} outer
   * @param {chrome.accessibilityPrivate.ScreenRect=} inner
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   */
  expandToFitWithPadding: (padding, outer, inner) => {
    if (!outer || !inner)
      return outer;

    let newOuter = RectHelper.deepCopy(outer);

    if (newOuter.top > inner.top - padding) {
      newOuter.top = inner.top - padding;
      // The height should be the original bottom point less the new top point.
      newOuter.height = RectHelper.bottom(outer) - newOuter.top;
    }
    if (newOuter.left > inner.left - padding) {
      newOuter.left = inner.left - padding;
      // The new width should be the original right point less the new left.
      newOuter.width = RectHelper.right(outer) - newOuter.left;
    }
    if (RectHelper.bottom(newOuter) < RectHelper.bottom(inner) + padding) {
      newOuter.height = RectHelper.bottom(inner) + padding - newOuter.top;
    }
    if (RectHelper.right(newOuter) < RectHelper.right(inner) + padding) {
      newOuter.width = RectHelper.right(inner) + padding - newOuter.left;
    }

    return newOuter;
  },

  /**
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  deepCopy: (rect) => {
    const copy = (Object.assign({}, rect));
    return /** @type {!chrome.accessibilityPrivate.ScreenRect} */ (copy);
  }

};
