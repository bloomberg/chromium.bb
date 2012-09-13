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
   * Abstract base class that provides common functionality for implementing
   * free-floating informational bubbles with a triangular arrow pointing at an
   * anchor node.
   */
  var BubbleBase = cr.ui.define('div');

  /**
   * The horizontal distance between the tip of the arrow and the reference edge
   * of the bubble (as specified by the arrow location).
   * @const
   */
  BubbleBase.ARROW_OFFSET = 30;

  BubbleBase.prototype = {
    // Set up the prototype chain
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.className = 'bubble';
      this.innerHTML =
          '<div class="bubble-content"></div>' +
          '<div class="bubble-shadow"></div>' +
          '<div class="bubble-arrow"></div>';
      this.hidden = true;
    },

    /**
     * Set the anchor node, i.e. the node that this bubble points at. Only
     * available when the bubble is not being shown.
     * @param {HTMLElement} node The new anchor node.
     */
    set anchorNode(node) {
      if (!this.hidden)
        return;

      this.anchorNode_ = node;
    },

    /**
     * Set the conent of the bubble. Only available when the bubble is not being
     * shown.
     * @param {HTMLElement} node The root node of the new content.
     */
    set content(node) {
      if (!this.hidden)
        return;

      var bubbleContent = this.querySelector('.bubble-content');
      bubbleContent.innerHTML = '';
      bubbleContent.appendChild(node);
    },

    /**
     * Set the arrow location. Only available when the bubble is not being
     * shown.
     * @param {cr.ui.ArrowLocation} location The new arrow location.
     */
    set arrowLocation(location) {
      if (!this.hidden)
        return;

      this.arrowAtRight_ = location == ArrowLocation.TOP_END ||
                           location == ArrowLocation.BOTTOM_END;
      if (document.documentElement.dir == 'rtl')
        this.arrowAtRight_ = !this.arrowAtRight_;
      this.arrowAtTop_ = location == ArrowLocation.TOP_START ||
                         location == ArrowLocation.TOP_END;
    },

    /**
     * Show the bubble.
     */
    show: function() {
      if (!this.hidden)
        return;

      document.body.appendChild(this);
      this.hidden = false;

      var doc = this.ownerDocument;
      this.eventTracker_ = new EventTracker;
      this.eventTracker_.add(doc, 'keydown', this, true);
      this.eventTracker_.add(doc, 'mousedown', this, true);
    },

    /**
     * Hide the bubble.
     */
    hide: function() {
      if (this.hidden)
        return;

      this.hidden = true;
      this.eventTracker_.removeAll();
      this.parentNode.removeChild(this);
    },

    /**
     * Handle keyboard events, dismissing the bubble if necessary.
     * @param {Event} event The event.
     */
    handleEvent: function(event) {
      // Close the bubble when the user presses <Esc>.
      if (event.type == 'keydown' && event.keyCode == 27) {
        this.hide();
        event.preventDefault();
        event.stopPropagation();
      }
    },

    /**
     * Update the arrow so that it appears at the correct position.
     * @param {Boolean} visible Whether the arrow should be visible.
     * @param {number} The horizontal distance between the tip of the arrow and
     * the reference edge of the bubble (as specified by the arrow location).
     * @private
     */
    updateArrowPosition_: function(visible, tipOffset) {
      var bubbleArrow = this.querySelector('.bubble-arrow');

      if (visible) {
        bubbleArrow.style.display = 'block';
      } else {
        bubbleArrow.style.display = 'none';
        return;
      }

      var edgeOffset = (-bubbleArrow.clientHeight / 2) + 'px';
      bubbleArrow.style.top = this.arrowAtTop_ ? edgeOffset : 'auto';
      bubbleArrow.style.bottom = this.arrowAtTop_ ? 'auto' : edgeOffset;

      edgeOffset = (tipOffset - bubbleArrow.clientHeight / 2) + 'px';
      bubbleArrow.style.left = this.arrowAtRight_ ? 'auto' : edgeOffset;
      bubbleArrow.style.right = this.arrowAtRight_ ? edgeOffset : 'auto';
    },
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
   * A bubble that remains open until the user explicitly dismisses it or clicks
   * outside the bubble after it has been shown for at least the specified
   * amount of time (making it less likely that the user will unintentionally
   * dismiss the bubble). The bubble repositions itself on layout changes.
   */
  var Bubble = cr.ui.define('div');

  Bubble.prototype = {
    // Set up the prototype chain
    __proto__: BubbleBase.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      BubbleBase.prototype.decorate.call(this);

      var close = document.createElement('div');
      close.className = 'bubble-close';
      this.appendChild(close);

      this.handleCloseEvent = this.hide;
      this.deactivateToDismissDelay_ = 0;
      this.bubbleAlignment = BubbleAlignment.ARROW_TO_MID_ANCHOR;
    },

    /**
     * Handler for close events triggered when the close button is clicked. By
     * default, set to this.hide. Only available when the bubble is not being
     * shown.
     * @param {function} handler The new handler, a function with no parameters.
     */
    set handleCloseEvent(handler) {
      if (!this.hidden)
        return;

      this.handleCloseEvent_ = handler;
    },

    /**
     * Set the bubble alignment. Only available when the bubble is not being
     * shown.
     * @param {cr.ui.BubbleAlignment} alignment The new bubble alignment.
     */
    set bubbleAlignment(alignment) {
      if (!this.hidden)
        return;

      this.bubbleAlignment_ = alignment;
    },

    /**
     * Set the delay before the user is allowed to click outside the bubble to
     * dismiss it. Using a delay makes it less likely that the user will
     * unintentionally dismiss the bubble.
     * @param {number} delay The delay in milliseconds.
     */
    set deactivateToDismissDelay(delay) {
      this.deactivateToDismissDelay_ = delay;
    },

    /**
     * Hide or show the close button.
     * @param {Boolean} isVisible True if the close button should be visible.
     */
    set closeButtonVisible(isVisible) {
      this.querySelector('.bubble-close').hidden = !isVisible;
    },

    /**
     * Update the position of the bubble. This is automatically called when the
     * window is resized, but should also be called any time the layout may have
     * changed.
     */
    reposition: function() {
      var clientRect = this.anchorNode_.getBoundingClientRect();
      var bubbleArrow = this.querySelector('.bubble-arrow');
      var arrowOffsetY = bubbleArrow.offsetHeight / 2;

      var left;
      if (this.bubbleAlignment_ ==
          BubbleAlignment.BUBBLE_EDGE_TO_ANCHOR_EDGE) {
        left = this.arrowAtRight_ ? clientRect.right - this.clientWidth :
            clientRect.left;
      } else {
        var anchorMid = (clientRect.left + clientRect.right) / 2;
        left = this.arrowAtRight_ ?
            anchorMid - this.clientWidth + BubbleBase.ARROW_OFFSET :
            anchorMid - BubbleBase.ARROW_OFFSET;
      }
      var top = this.arrowAtTop_ ? clientRect.bottom + arrowOffsetY :
          clientRect.top - this.clientHeight - arrowOffsetY;

      this.style.left = left + 'px';
      this.style.top = top + 'px';

      this.updateArrowPosition_(true, BubbleBase.ARROW_OFFSET);
    },

    /**
     * Show the bubble.
     */
    show: function() {
      if (!this.hidden)
        return;

      BubbleBase.prototype.show.call(this);

      this.reposition();
      this.showTime_ = Date.now();
      this.eventTracker_.add(window, 'resize', this.reposition.bind(this));
    },

    /**
     * Handle keyboard and mouse events, dismissing the bubble if necessary.
     * @param {Event} event The event.
     */
    handleEvent: function(event) {
      BubbleBase.prototype.handleEvent.call(this, event);

      if (event.type == 'mousedown') {
        // Dismiss the bubble when the user clicks on the close button.
        if (event.target == this.querySelector('.bubble-close')) {
          this.handleCloseEvent_();
        // Dismiss the bubble when the user clicks outside it after the
        // specified delay has passed.
        } else if (!this.contains(event.target) &&
            Date.now() - this.showTime_ >= this.deactivateToDismissDelay_) {
          this.hide();
        }
      }
    },
  };

  return {
    ArrowLocation: ArrowLocation,
    BubbleBase: BubbleBase,
    BubbleAlignment: BubbleAlignment,
    Bubble: Bubble
  };
});
