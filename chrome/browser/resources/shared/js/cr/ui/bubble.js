// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: event_tracker.js

cr.define('cr.ui', function() {

  /**
   * The arrow location specifies how the arrow and bubble are positioned in
   * relation to the anchor node.
   * @enum
   */
  var ArrowLocation = {
    // The arrow is positioned at the top and the start of the bubble. In left
    // to right mode this is the top left. The entire bubble is positioned below
    // the anchor node.
    TOP_START: 'top-start',
    // The arrow is positioned at the top and the end of the bubble. In left to
    // right mode this is the top right. The entire bubble is positioned below
    // the anchor node.
    TOP_END: 'top-end',
    // The arrow is positioned at the bottom and the start of the bubble. In
    // left to right mode this is the bottom left. The entire bubble is
    // positioned above the anchor node.
    BOTTOM_START: 'bottom-start',
    // The arrow is positioned at the bottom and the end of the bubble. In
    // left to right mode this is the bottom right. The entire bubble is
    // positioned above the anchor node.
    BOTTOM_END: 'bottom-end'
  };

  /**
   * The bubble alignment specifies the horizontal position of the bubble in
   * relation to the anchor node.
   * @enum
   */
  var BubbleAlignment = {
    // The bubble is positioned so that the tip of the arrow points to the
    // middle of the anchor node.
    ARROW_TO_MID_ANCHOR: 'arrow-to-mid-anchor',
    // The bubble is positioned so that the edge nearest to the arrow is lined
    // up with the edge of the anchor node.
    BUBBLE_EDGE_TO_ANCHOR_EDGE: 'bubble-edge-anchor-edge'
  };

  /**
   * The horizontal distance between the tip of the arrow and the start or the
   * end of the bubble (as specified by the arrow location).
   * @const
   */
  var ARROW_OFFSET_X = 30;

  /**
   * The vertical distance between the tip of the arrow and the bottom or top of
   * the bubble (as specified by the arrow location). Note, if you change this
   * then you should also change the "top" and "bottom" values for .bubble-arrow
   * in bubble.css.
   * @const
   */
  var ARROW_OFFSET_Y = 8;

  /**
   * Bubble is a free-floating informational bubble with a triangular arrow
   * that points at a place of interest on the page.
   */
  var Bubble = cr.ui.define('div');

  Bubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'bubble';
      this.innerHTML =
          '<div class="bubble-contents"></div>' +
          '<div class="bubble-close"></div>' +
          '<div class="bubble-shadow"></div>' +
          '<div class="bubble-arrow"></div>';

      this.hidden = true;
      this.handleCloseEvent = this.hide;
      this.deactivateToDismissDelay_ = 0;
      this.bubbleAlignment = BubbleAlignment.ARROW_TO_MID_ANCHOR;
    },

    /**
     * Sets the child node of the bubble.
     * @param {node} An HTML element
     */
    set content(node) {
      var bubbleContent = this.querySelector('.bubble-contents');
      bubbleContent.innerHTML = '';
      bubbleContent.appendChild(node);
    },

    /**
     * Handles close event which is triggered when the close button
     * is clicked. By default is set to this.hide.
     * @param {function} A function with no parameters
     */
    set handleCloseEvent(func) {
      this.handleCloseEvent_ = func;
    },

    /**
     * Sets the anchor node, i.e. the node that this bubble points at.
     * @param {HTMLElement} node The new anchor node.
     */
    set anchorNode(node) {
      this.anchorNode_ = node;

      if (!this.hidden)
        this.reposition();
    },

    /**
     * Sets the arrow location.
     * @param {cr.ui.ArrowLocation} arrowLocation The new arrow location.
     */
    setArrowLocation: function(arrowLocation) {
      this.isRight_ = arrowLocation == ArrowLocation.TOP_END ||
                      arrowLocation == ArrowLocation.BOTTOM_END;
      if (document.documentElement.dir == 'rtl')
        this.isRight_ = !this.isRight_;
      this.isTop_ = arrowLocation == ArrowLocation.TOP_START ||
                    arrowLocation == ArrowLocation.TOP_END;

      var bubbleArrow = this.querySelector('.bubble-arrow');
      bubbleArrow.setAttribute('is-right', this.isRight_);
      bubbleArrow.setAttribute('is-top', this.isTop_);

      if (!this.hidden)
        this.reposition();
    },

    /**
     * Sets the bubble alignment.
     * @param {cr.ui.BubbleAlignment} alignment The new bubble alignment.
     */
    set bubbleAlignment(alignment) {
      this.bubbleAlignment_ = alignment;
    },

    /**
     * Sets the delay before the user is allowed to click outside the bubble
     * to dismiss it. Using a delay makes it less likely that the user will
     * unintentionally dismiss the bubble.
     * @param {number} delay The delay in milliseconds.
     */
    set deactivateToDismissDelay(delay) {
      this.deactivateToDismissDelay_ = delay;
    },

    /**
     * Hides or shows the close button.
     * @param {Boolean} isVisible True if the close button should be visible.
     */
    setCloseButtonVisible: function(isVisible) {
      this.querySelector('.bubble-close').hidden = !isVisible;
    },

    /**
     * Updates the position of the bubble. This is automatically called when
     * the window is resized, but should also be called any time the layout
     * may have changed.
     */
    reposition: function() {
      var clientRect = this.anchorNode_.getBoundingClientRect();

      var left;
      if (this.bubbleAlignment_ ==
          BubbleAlignment.BUBBLE_EDGE_TO_ANCHOR_EDGE) {
        left = this.isRight_ ? clientRect.right - this.clientWidth :
            clientRect.left;
      } else {
        var anchorMid = (clientRect.left + clientRect.right) / 2;
        left = this.isRight_ ? anchorMid - this.clientWidth + ARROW_OFFSET_X :
            anchorMid - ARROW_OFFSET_X;
      }
      var top = this.isTop_ ? clientRect.bottom + ARROW_OFFSET_Y :
          clientRect.top - this.clientHeight - ARROW_OFFSET_Y;

      this.style.left = left + 'px';
      this.style.top = top + 'px';
    },

    /**
     * Starts showing the bubble. The bubble will show until the user clicks
     * away or presses Escape.
     */
    show: function() {
      if (!this.hidden)
        return;

      document.body.appendChild(this);
      this.hidden = false;
      this.reposition();
      this.showTime_ = Date.now();

      this.eventTracker_ = new EventTracker;
      this.eventTracker_.add(window, 'resize', this.reposition.bind(this));

      var doc = this.ownerDocument;
      this.eventTracker_.add(doc, 'keydown', this, true);
      this.eventTracker_.add(doc, 'mousedown', this, true);
    },

    /**
     * Hides the bubble from view.
     */
    hide: function() {
      this.hidden = true;
      this.eventTracker_.removeAll();
      this.parentNode.removeChild(this);
    },

    /**
     * Handles keydown and mousedown events, dismissing the bubble if
     * necessary.
     * @param {Event} e The event.
     */
    handleEvent: function(e) {
      switch (e.type) {
        case 'keydown': {
          if (e.keyCode == 27)  // Esc
            this.hide();
          break;
        }
        case 'mousedown': {
          if (e.target == this.querySelector('.bubble-close')) {
            this.handleCloseEvent_();
          } else if (!this.contains(e.target)) {
            if (Date.now() - this.showTime_ < this.deactivateToDismissDelay_)
              return;
            this.hide();
          } else {
            return;
          }
          break;
        }
      }
    },
  };

  return {
    ArrowLocation: ArrowLocation,
    Bubble: Bubble,
    BubbleAlignment: BubbleAlignment
  };
});
