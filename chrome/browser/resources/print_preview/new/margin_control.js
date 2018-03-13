// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * Radius of the margin control in pixels. Padding of control + 1 for border.
 * @type {number}
 */
const RADIUS_PX = 9;

Polymer({
  is: 'print-preview-margin-control',

  properties: {
    side: {
      type: String,
      reflectToAttribute: true,
    },

    invisible: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private {number} */
    positionInPts_: {
      type: Number,
      notify: true,
      value: 0,
    },

    /** @type {number} */
    scaleTransform: {
      type: Number,
      notify: true,
    },

    /** @type {!print_preview.Coordinate2d} */
    translateTransform: {
      type: Object,
      notify: true,
    },

    /** @type {!print_preview.Size} */
    pageSize: {
      type: Object,
      notify: true,
    },

    /** @type {?print_preview.Size} */
    clipSize: {
      type: Object,
      notify: true,
      observer: 'onClipSizeChange_',
    },
  },

  observers:
      ['updatePosition_(positionInPts_, scaleTransform, translateTransform, ' +
       'pageSize, side)'],

  /** @param {number} position The new position for the margin control. */
  setPositionInPts(position) {
    this.positionInPts_ = position;
  },

  /** @private */
  updatePosition_: function() {
    const orientationEnum = print_preview.ticket_items.CustomMarginsOrientation;
    let x = this.translateTransform.x;
    let y = this.translateTransform.y;
    let width = null;
    let height = null;
    if (this.side == orientationEnum.TOP) {
      y = this.scaleTransform * this.positionInPts_ +
          this.translateTransform.y - RADIUS_PX;
      width = this.scaleTransform * this.pageSize.width;
    } else if (this.side == orientationEnum.RIGHT) {
      x = this.scaleTransform * (this.pageSize.width - this.positionInPts_) +
          this.translateTransform.x - RADIUS_PX;
      height = this.scaleTransform * this.pageSize.height;
    } else if (this.side == orientationEnum.BOTTOM) {
      y = this.scaleTransform * (this.pageSize.height - this.positionInPts_) +
          this.translateTransform.y - RADIUS_PX;
      width = this.scaleTransform * this.pageSize.width;
    } else {
      x = this.scaleTransform * this.positionInPts_ +
          this.translateTransform.x - RADIUS_PX;
      height = this.scaleTransform * this.pageSize.height;
    }
    window.requestAnimationFrame(() => {
      this.style.left = Math.round(x) + 'px';
      this.style.top = Math.round(y) + 'px';
      if (width != null) {
        this.style.width = Math.round(width) + 'px';
      }
      if (height != null) {
        this.style.height = Math.round(height) + 'px';
      }
    });
    this.onClipSizeChange_();
  },

  /** @private */
  onClipSizeChange_: function() {
    if (!this.clipSize)
      return;
    window.requestAnimationFrame(() => {
      const offsetLeft = this.offsetLeft;
      const offsetTop = this.offsetTop;
      this.style.clip = 'rect(' + (-offsetTop) + 'px, ' +
          (this.clipSize.width - offsetLeft) + 'px, ' +
          (this.clipSize.height - offsetTop) + 'px, ' + (-offsetLeft) + 'px)';
    });
  },
});
})();
