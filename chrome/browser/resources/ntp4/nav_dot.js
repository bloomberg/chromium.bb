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
   * @param {TilePage} page The associated TilePage.
   * @param {bool} animate If true, animates into existence.
   * @constructor
   * @extends {HTMLLIElement}
   */
  function NavDot(page, animate) {
    var dot = cr.doc.createElement('li');
    dot.__proto__ = NavDot.prototype;
    dot.initialize(page, animate);

    return dot;
  }

  NavDot.prototype = {
    __proto__: HTMLLIElement.prototype,

    initialize: function(page, animate) {
      this.className = 'dot';
      this.setAttribute('tabindex', 0);
      this.setAttribute('role', 'button');

      this.page_ = page;

      // TODO(estade): should there be some limit to the number of characters?
      this.input_ = this.ownerDocument.createElement('input');
      this.input_.setAttribute('spellcheck', false);
      this.input_.value = page.pageName;
      this.appendChild(this.input_);

      this.addEventListener('click', this.onClick_);
      this.addEventListener('dblclick', this.onDoubleClick_);
      this.addEventListener('dragenter', this.onDragEnter_);
      this.addEventListener('dragleave', this.onDragLeave_);
      this.addEventListener('webkitTransitionEnd', this.onTransitionEnd_);

      this.input_.addEventListener('blur', this.onInputBlur_.bind(this));
      this.input_.addEventListener('mousedown',
                                   this.onInputMouseDown_.bind(this));
      this.input_.addEventListener('keydown', this.onInputKeyDown_.bind(this));

      if (animate) {
        this.classList.add('small');
        var self = this;
        window.setTimeout(function() {
          self.classList.remove('small');
        }, 0);
      }
    },

    /**
     * Removes the dot from the page after transitioning to 0 width.
     */
    animateRemove: function() {
      this.classList.add('small');
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
      // The explicit focus call is necessary because of overriding the default
      // handling in onInputMouseDown_.
      if (this.ownerDocument.activeElement != this.input_)
        this.focus();
      e.stopPropagation();
    },

    /**
     * Double clicks allow the user to edit the page title.
     * @param {Event} e The click event.
     * @private
     */
    onDoubleClick_: function(e) {
      this.input_.focus();
      this.input_.select();
    },

    /**
     * Prevent mouse down on the input from selecting it.
     * @param {Event} e The click event.
     * @private
     */
    onInputMouseDown_: function(e) {
      if (this.ownerDocument.activeElement != this.input_)
        e.preventDefault();
    },

    /**
     * Handle keypresses on the input.
     * @param {Event} e The click event.
     * @private
     */
    onInputKeyDown_: function(e) {
      switch (e.keyIdentifier) {
        case 'U+001B':  // Escape cancels edits.
          this.input_.value = this.page_.pageName;
        case 'Enter':  // Fall through.
          this.input_.blur();
          break;
      }
    },

    /**
     * When the input blurs, commit the edited changes.
     * @param {Event} e The blur event.
     * @private
     */
    onInputBlur_: function(e) {
      window.getSelection().removeAllRanges();
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

    /**
     * A transition has ended.
     * @param {Event} e The transition end event.
     */
    onTransitionEnd_: function(e) {
      if (e.propertyName === 'width' && this.classList.contains('small'))
        this.parentNode.removeChild(this);
    },
  };

  return {
    NavDot: NavDot,
  };
});
