// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a split pane control.
 *
 * The split pane is a hbox (display: -webkit-box) with at least two elements.
 * The second element is the splitter. The width of the first element determines
 * the position of the splitter.
 *
 * <div class=split-pane>
 *   <div class=left>...</div>
 *   <div class=splitter></div>
 *   ...
 * </div>
 *
 */

cr.define('cr.ui', function() {
  // TODO(arv): Currently this only supports horizontal layout.
  // TODO(arv): This ignores min-width and max-width of the elements to the
  // right of the splitter.

  /**
   * Returns the computed style width of an element.
   * @param {!Element} el The element to get the width of.
   * @return {number} The width in pixels.
   */
  function getComputedWidth(el) {
    return parseInt(el.ownerDocument.defaultView.getComputedStyle(el).width,
                    10);
  }

  /**
   * Creates a new split pane element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var SplitPane = cr.ui.define('div');

  SplitPane.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Initializes the element.
     */
    decorate: function() {
      this.addEventListener('mousedown', cr.bind(this.handleMouseDown_, this),
                            true);
    },

    /**
     * Starts the dragging of the splitter.
     * @param {!Event} e The mouse event that started the drag.
     */
    startDrag: function(e) {
      if (!this.boundHandleMouseMove_) {
        this.boundHandleMouseMove_ = cr.bind(this.handleMouseMove_, this);
        this.boundHandleMouseUp_ = cr.bind(this.handleMouseUp_, this);
      }

      var doc = this.ownerDocument;

      // Use capturing events on the document to get events when the mouse
      // leaves the document.
      doc.addEventListener('mousemove',this.boundHandleMouseMove_, true);
      doc.addEventListener('mouseup', this.boundHandleMouseUp_, true);

      // Use the computed width style as the base so that we can ignore what
      // box sizing the element has.
      var leftComponent = this.firstElementChild;
      var computedWidth = getComputedWidth(leftComponent);
      this.startX_ = e.screenX;
      this.startWidth_ = computedWidth;
    },

    /**
     * Ends the dragging of the splitter. This fires a "resize" event if the
     * size changed.
     */
    endDrag: function() {
      var doc = this.ownerDocument;
      doc.removeEventListener('mousemove', this.boundHandleMouseMove_, true);
      doc.removeEventListener('mouseup', this.boundHandleMouseUp_, true);

      // Check if the size changed.
      var leftComponent = this.firstElementChild;
      var computedWidth = getComputedWidth(leftComponent);
      if (this.startWidth_ != computedWidth)
        cr.dispatchSimpleEvent(this, 'resize');
    },

    /**
     * Handles the mouse down event which starts the dragging of the splitter.
     * @param {!Event} e The mouse event.
     * @private
     */
    handleMouseDown_: function(e) {
      var splitter = this.splitter;
      if (splitter && splitter.contains(e.target)) {
        this.startDrag(e);
        // Default action is to start selection and to move focus.
        e.preventDefault();
      }
    },

    /**
     * Handles the mouse move event which moves the splitter as the user moves
     * the mouse.
     * @param {!Event} e The mouse event.
     * @private
     */
    handleMouseMove_: function(e) {
      var leftComponent = this.firstElementChild;
      var rtl = this.ownerDocument.defaultView.getComputedStyle(this).
          direction == 'rtl';
      var dirMultiplier = rtl ? -1 : 1;
      leftComponent.style.width = this.startWidth_ +
          dirMultiplier * (e.screenX - this.startX_) + 'px';
    },

    /**
     * Handles the mouse up event which ends the dragging of the splitter.
     * @param {!Event} e The mouse event.
     * @private
     */
    handleMouseUp_: function(e) {
      this.endDrag();
    },

    /**
     * The element used as the splitter.
     * @type {HTMLElement}
     */
    get splitter() {
      return this.children[1];
    }
  };

  return {
    SplitPane: SplitPane
  }
});
