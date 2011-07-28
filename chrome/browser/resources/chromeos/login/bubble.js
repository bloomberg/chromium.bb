// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bubble implementation.
 */

// TODO(xiyuan): Move this into shared.
cr.define('cr.ui', function() {
  /**
   * Creates a bubble div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var Bubble = cr.ui.define('div');

  Bubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Anchor element
    anchor_: undefined,

    /** @inheritDoc */
    decorate: function() {
      this.ownerDocument.addEventListener('click',
                                          this.handleClick_.bind(this));
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {string} text Text message to show in bubble.
     * @public
     */
    showTextForElement: function(el, text) {
      // Just in cse previous fade out animation is not finished.
      this.removeEventListener('webkitTransitionEnd',
                               this.handleFadedEnd_.bind(this));

      const ARROW_OFFSET = 14;
      const VERTICAL_PADDING = 5;

      var elementOrigin = Oobe.getOffset(el);
      var anchorX = elementOrigin.left + el.offsetWidth / 2 - ARROW_OFFSET;
      var anchorY = elementOrigin.top + el.offsetHeight + VERTICAL_PADDING;

      this.style.left = anchorX + 'px';
      this.style.top = anchorY + 'px';

      this.anchor_ = el;
      this.textContent = text;
      this.hidden = false;
      this.classList.remove('faded');
    },

    /**
     * Hides the bubble.
     */
    hide: function() {
      if (!this.classList.contains('faded')) {
        this.classList.add('faded');
        this.addEventListener('webkitTransitionEnd',
                              this.handleFadedEnd_.bind(this));
      }
    },

    /**
     * Handler for faded transition end.
     * @private
     */
    handleFadedEnd_: function(e) {
      this.removeEventListener('webkitTransitionEnd',
                               this.handleFadedEnd_.bind(this));
      this.hidden = true;
    },

    /**
     * Handler of click event.
     * @private
     */
    handleClick_: function(e) {
      // Ignore clicks on anchor element.
      if (e.target == this.anchor_)
        return;

      if (!this.hidden)
        this.hide();
    }
  };

  return {
    Bubble: Bubble
  };
});
