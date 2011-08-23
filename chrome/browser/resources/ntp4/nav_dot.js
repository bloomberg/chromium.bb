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
   * @param {string} title The title of the navigation dot.
   * @param {bool} titleIsEditable If true, the title can be changed.
   * @param {bool} animate If true, animates into existence.
   * @constructor
   * @extends {HTMLLIElement}
   */
  function NavDot(page, title, titleIsEditable, animate) {
    var dot = cr.doc.createElement('li');
    dot.__proto__ = NavDot.prototype;
    dot.initialize(page, title, titleIsEditable, animate);

    return dot;
  }

  NavDot.prototype = {
    __proto__: HTMLLIElement.prototype,

    initialize: function(page, title, titleIsEditable, animate) {
      this.className = 'dot';
      this.setAttribute('tabindex', 0);
      this.setAttribute('role', 'button');

      this.page_ = page;
      this.title_ = title;
      this.titleIsEditable_ = titleIsEditable;

      var selectionBar = this.ownerDocument.createElement('div');
      selectionBar.className = 'selection-bar';
      this.appendChild(selectionBar);

      // TODO(estade): should there be some limit to the number of characters?
      this.input_ = this.ownerDocument.createElement('input');
      this.input_.setAttribute('spellcheck', false);
      this.input_.value = title;
      // Take the input out of the tab-traversal focus order.
      this.input_.disabled = true;
      this.appendChild(this.input_);

      this.addEventListener('click', this.onClick_);
      this.addEventListener('dblclick', this.onDoubleClick_);
      this.dragWrapper_ = new DragWrapper(this, this);
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

      chrome.send('navigationDotUsed');
      e.stopPropagation();
    },

    /**
     * Double clicks allow the user to edit the page title.
     * @param {Event} e The click event.
     * @private
     */
    onDoubleClick_: function(e) {
      if (this.titleIsEditable_) {
        this.input_.disabled = false;
        this.input_.focus();
        this.input_.select();
      }
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
          this.input_.value = this.title_;
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
      this.title_ = this.input_.value;
      ntp4.saveAppPageName(this.page_, this.title_);
      this.input_.disabled = true;
    },

    shouldAcceptDrag: function(e) {
      return this.page_.shouldAcceptDrag(e);
    },

    /**
     * A drag has entered the navigation dot. If the user hovers long enough,
     * we will navigate to the relevant page.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDragEnter: function(e) {
      var self = this;
      function navPageClearTimeout() {
        self.switchToPage();
        self.dragNavTimeout = null;
      }
      this.dragNavTimeout = window.setTimeout(navPageClearTimeout, 500);

      this.doDragOver(e);
    },

    /**
     * A dragged element has moved over the navigation dot. Show the correct
     * indicator and prevent default handling so the <input> won't act as a drag
     * target.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDragOver: function(e) {
      e.preventDefault();

      if (!this.dragWrapper_.isCurrentDragTarget)
        e.dataTransfer.dropEffect = 'none';
      else if (ntp4.getCurrentlyDraggingTile())
        e.dataTransfer.dropEffect = 'move';
      else
        e.dataTransfer.dropEffect = 'copy';
    },

    /**
     * A dragged element has been dropped on the navigation dot. Tell the page
     * to append it.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDrop: function(e) {
      e.stopPropagation();
      var tile = ntp4.getCurrentlyDraggingTile();
      if (tile && tile.tilePage != this.page_)
        this.page_.appendDraggingTile();
      // TODO(estade): handle non-tile drags.

      this.cancelDelayedSwitch_();
    },

    /**
     * The drag has left the navigation dot.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDragLeave: function(e) {
      this.cancelDelayedSwitch_();
    },

    /**
     * Cancels the timer for page switching.
     * @private
     */
    cancelDelayedSwitch_: function() {
      if (this.dragNavTimeout) {
        window.clearTimeout(this.dragNavTimeout);
        this.dragNavTimeout = null;
      }
    },

    /**
     * A transition has ended.
     * @param {Event} e The transition end event.
     * @private
     */
    onTransitionEnd_: function(e) {
      if (e.propertyName === 'max-width' && this.classList.contains('small'))
        this.parentNode.removeChild(this);
    },
  };

  return {
    NavDot: NavDot,
  };
});
