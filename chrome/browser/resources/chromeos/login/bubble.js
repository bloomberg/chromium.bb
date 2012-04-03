// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

  /**
   * Bubble attachment side.
   * @enum {string}
   */
  Bubble.Attachment = {
    RIGHT: 'bubble-right',
    LEFT: 'bubble-left',
    TOP: 'bubble-top',
    BOTTOM: 'bubble-bottom'
  };

  Bubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Anchor element
    anchor_: undefined,

    /** @inheritDoc */
    decorate: function() {
      this.ownerDocument.addEventListener('click',
                                          this.handleDocClick_.bind(this));
      this.ownerDocument.addEventListener('keydown',
                                          this.handleDocKeyDown_.bind(this));
      this.addEventListener('webkitTransitionEnd',
                            this.handleTransitionEnd_.bind(this));
    },

    /**
     * Sets the attachment of the bubble.
     * @param {!Attachment} attachment Bubble attachment.
     */
    setAttachment_: function (attachment) {
      for (var k in Bubble.Attachment) {
        var v = Bubble.Attachment[k];
        this.classList[v == attachment ? 'add' : 'remove'](v);
      }
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!Object} pos Bubble position (left, top, right, bottom in px).
     * @param {HTMLElement} content Content to show in bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     specified position it should be displayed).
     * @private
     */
    showContentAt_: function(pos, content, attachment) {
      this.style.top = this.style.left = this.style.right = this.style.bottom =
          'auto';
      for (var k in pos) {
        if (typeof pos[k] == 'number')
          this.style[k] = pos[k] + 'px';
      }
      this.innerHTML = '';
      this.appendChild(content);
      this.setAttachment_(attachment);
      this.hidden = false;
      this.classList.remove('faded');
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {HTMLElement} content Content to show in bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {number=} opt_offset Offset of the bubble attachment point from
     *     left (for vertical attachment) or top (for horizontal attachment)
     *     side of the element. If not specified, the bubble is positioned to
     *     be aligned with the left/top side of the element but not farther than
     *     half of its weight/height.
     * @param {number=} opt_padding Optional padding of the bubble.
     * @public
     */
    showContentForElement: function(el, content, attachment,
                                    opt_offset, opt_padding) {
      const ARROW_OFFSET = 25;
      const DEFAULT_PADDING = 18;

      if (typeof opt_padding == 'undefined')
        opt_padding = DEFAULT_PADDING;

      var origin = cr.ui.login.DisplayManager.getPosition(el);
      var offset = typeof opt_offset == 'undefined' ?
          [ Math.min(ARROW_OFFSET, el.offsetWidth/2),
            Math.min(ARROW_OFFSET, el.offsetHeight/2) ] :
          [ opt_offset, opt_offset ];

      var pos = {};
      if (isRTL()) {
        switch (attachment) {
          case Bubble.Attachment.TOP:
            pos.right = origin.right + offset[0] - ARROW_OFFSET;
            pos.bottom = origin.bottom + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.RIGHT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.right = origin.right + el.offsetWidth + opt_padding;
            break;
          case Bubble.Attachment.BOTTOM:
            pos.right = origin.right + offset[0] - ARROW_OFFSET;
            pos.top = origin.top + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.LEFT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.left = origin.left + el.offsetWidth + opt_padding;
            break;
        }
      } else {
        switch (attachment) {
          case Bubble.Attachment.TOP:
            pos.left = origin.left + offset[0] - ARROW_OFFSET;
            pos.bottom = origin.bottom + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.RIGHT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.left = origin.left + el.offsetWidth + opt_padding;
            break;
          case Bubble.Attachment.BOTTOM:
            pos.left = origin.left + offset[0] - ARROW_OFFSET;
            pos.top = origin.top + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.LEFT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.right = origin.right + el.offsetWidth + opt_padding;
            break;
        }
      }

      this.anchor_ = el;
      this.showContentAt_(pos, content, attachment);
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {string} text Text content to show in bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {number=} opt_offset Offset of the bubble attachment point from
     *     left (for vertical attachment) or top (for horizontal attachment)
     *     side of the element. If not specified, the bubble is positioned to
     *     be aligned with the left/top side of the element but not farther than
     *     half of its weight/height.
     * @param {number=} opt_padding Optional padding of the bubble.
     * @public
     */
    showTextForElement: function(el, text, attachment,
                                 opt_offset, opt_padding) {
      var span = this.ownerDocument.createElement('span');
      span.textContent = text;
      this.showContentForElement(el, span, attachment, opt_offset, opt_padding);
    },

    /**
     * Hides the bubble.
     */
    hide: function() {
      if (!this.classList.contains('faded'))
        this.classList.add('faded');
    },

    /**
     * Hides the bubble anchored to the given element (if any).
     * @param {!Object} el Anchor element.
     */
    hideForElement: function(el) {
      if (!this.hidden && this.anchor_ == el)
        this.hide();
    },

    /**
     * Handler for faded transition end.
     * @private
     */
    handleTransitionEnd_: function(e) {
      if (this.classList.contains('faded'))
        this.hidden = true;
    },

    /**
     * Handler of document click event.
     * @private
     */
    handleDocClick_: function(e) {
      // Ignore clicks on anchor element.
      if (e.target == this.anchor_)
        return;

      if (!this.hidden)
        this.hide();
    },

    /**
     * Handle of document keydown event.
     * @private
     */
    handleDocKeyDown_: function(e) {
      if (!this.hidden)
        this.hide();
    }
  };

  return {
    Bubble: Bubble
  };
});
