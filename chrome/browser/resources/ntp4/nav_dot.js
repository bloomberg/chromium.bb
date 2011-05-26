// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Nav dot
 * This is the class for the navigation controls that appear along the bottom
 * of the NTP.
 */

cr.define('ntp4', function() {
  'use strict';

  /**
   * Creates a new navigation dot.
   * @constructor
   * @extends {HTMLLIElement}
   */
  function NavDot(page) {
    var dot = cr.doc.createElement('li');
    dot.__proto__ = NavDot.prototype;
    dot.initialize(page);

    return dot;
  }

  NavDot.prototype = {
    __proto__: HTMLLIElement.prototype,

    initialize: function(page) {
      this.className = 'dot';
      this.setAttribute('tabindex', 0);
      this.setAttribute('role', 'button');

      this.page_ = page;

      // TODO(estade): should there be some limit to the number of characters?
      this.span_ = this.ownerDocument.createElement('span');
      this.span_.setAttribute('spellcheck', false);
      this.span_.textContent = page.pageName;
      this.appendChild(this.span_);

      this.addEventListener('click', this.onClick_);
      this.addEventListener('dblclick', this.onDoubleClick_);
      this.addEventListener('dragenter', this.onDragEnter_);
      this.addEventListener('dragleave', this.onDragLeave_);

      this.span_.addEventListener('blur', this.onSpanBlur_.bind(this));
      this.span_.addEventListener('keydown', this.onSpanKeyDown_.bind(this));
    },

    /**
     * Navigates the card slider to the page for this dot.
     */
    switchToPage: function() {
      ntp4.getCardSlider().selectCardByValue(this.page_, true);
    },

    /**
     * Clicking causes the associated page to show.
     * @param {Event} e The click event.
     * @private
     */
    onClick_: function(e) {
      this.switchToPage();
      e.stopPropagation();
    },

    /**
     * Double clicks allow the user to edit the page title.
     * @param {Event} e The click event.
     * @private
     */
    onDoubleClick_: function(e) {
      this.span_.setAttribute('contenteditable', true);
      this.span_.focus();

      // This will select the text.
      var range = document.createRange();
      range.setStart(this.span_, 0);
      range.setEnd(this.span_, 1);
      var sel = window.getSelection();
      sel.removeAllRanges();
      sel.addRange(range);
    },

    /**
     * Handle keypresses on the span.
     * @param {Event} e The click event.
     * @private
     */
    onSpanKeyDown_: function(e) {
      switch (e.keyIdentifier) {
        case 'U+001B':  // Escape cancels edits.
          this.span_.textContent = this.page_.pageName;
        case 'Enter':  // Fall through.
          this.span_.blur();
          break;
      }
    },

    /**
     * When the span blurs, commit the edited changes.
     * @param {Event} e The blur event.
     * @private
     */
    onSpanBlur_: function(e) {
      this.span_.setAttribute('contenteditable', false);
      // TODO(estade): persist changes to textContent.
    },

    /**
      * These are equivalent to dragEnters_ and isCurrentDragTarget_ from
      * TilePage.
      * TODO(estade): thunkify the event handlers in the same manner as the
      * tile grid drag handlers.
      */
    dragEnters_: 0,
    isCurrentDragTarget_: false,

    /**
     * A drag has entered the navigation dot. If the user hovers long enough,
     * we will navigate to the relevant page.
     * @param {Event} e The MouseOver event for the drag.
     */
    onDragEnter_: function(e) {
      if (++this.dragEnters_ > 1)
        return;

      if (!this.page_.shouldAcceptDrag(e.dataTransfer))
        return;

      this.isCurrentDragTarget_ = true;

      var self = this;
      function navPageClearTimeout() {
        self.switchToPage();
        self.dragNavTimeout = null;
      }
      this.dragNavTimeout = window.setTimeout(navPageClearTimeout, 500);
    },

    /**
     * The drag has left the navigation dot.
     * @param {Event} e The MouseOver event for the drag.
     */
    onDragLeave_: function(e) {
      if (--this.dragEnters_ > 0)
        return;

      if (!this.isCurrentDragTarget_)
        return;
      this.isCurrentDragTarget_ = false;

      if (this.dragNavTimeout) {
        window.clearTimeout(this.dragNavTimeout);
        this.dragNavTimeout = null;
      }
    },
  };

  return {
    NavDot: NavDot,
  };
});
