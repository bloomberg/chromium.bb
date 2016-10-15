// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-action-menu',
  extends: 'dialog',

  /**
   * List of all options in this action menu.
   * @private {?NodeList<!Element>}
   */
  options_: null,

  /**
   * Index of the currently focused item.
   * @private {number}
   */
  focusedIndex_: -1,

  /**
   * Reference to the bound window's resize listener, such that it can be
   * removed on detach.
   * @private {?Function}
   */
  onWindowResize_: null,

  hostAttributes: {
    tabindex: 0,
  },

  listeners: {
    'keydown': 'onKeyDown_',
    'tap': 'onTap_',
  },

  /** override */
  attached: function() {
    this.options_ = this.querySelectorAll('.dropdown-item');
  },

  /** override */
  detached: function() {
    this.removeResizeListener_();
  },

  /** @private */
  removeResizeListener_: function() {
    window.removeEventListener('resize', this.onWindowResize_);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTap_: function(e) {
    if (e.target == this) {
      this.close();
      e.stopPropagation();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    if (e.key == 'Tab') {
      this.close();
      return;
    }

    if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp')
      return;

    var nextOption = this.getNextOption_(e.key == 'ArrowDown' ? 1 : - 1);
    if (nextOption)
      nextOption.focus();

    e.preventDefault();
  },

  /**
   * @param {number} step -1 for getting previous option (up), 1 for getting
   *     next option (down).
   * @return {?Element} The next focusable option, taking into account
   *     disabled/hidden attributes, or null if no focusable option exists.
   * @private
   */
  getNextOption_: function(step) {
    // Using a counter to ensure no infinite loop occurs if all elements are
    // hidden/disabled.
    var counter = 0;
    var nextOption = null;
    var numOptions = this.options_.length;

    do {
      this.focusedIndex_ =
          (numOptions + this.focusedIndex_ + step) % numOptions;
      nextOption = this.options_[this.focusedIndex_];
      if (nextOption.disabled || nextOption.hidden)
        nextOption = null;
      counter++;
    } while (!nextOption && counter < numOptions);

    return nextOption;
  },

  /** @override */
  close: function() {
    // Removing 'resize' listener when dialog is closed.
    this.removeResizeListener_();
    HTMLDialogElement.prototype.close.call(this);
    window.dispatchEvent(new CustomEvent('resize'));
  },

  /**
   * Shows the menu anchored to the given element.
   * @param {!Element} anchorElement
   */
  showAt: function(anchorElement) {
    var rect = anchorElement.getBoundingClientRect();

    // Ensure that the correct item is focused when the dialog is shown, by
    // setting the 'autofocus' attribute.
    this.focusedIndex_ = -1;
    var nextOption = this.getNextOption_(1);

    /** @suppress {checkTypes} */
    (function(options) {
      options.forEach(function(option) {
        option.removeAttribute('autofocus');
      });
    })(this.options_);

    if (nextOption)
      nextOption.setAttribute('autofocus', true);

    this.onWindowResize_ = this.onWindowResize_ || function() {
      if (this.open)
        this.close();
    }.bind(this);
    window.addEventListener('resize', this.onWindowResize_);

    this.showModal();

    if (new settings.DirectionDelegateImpl().isRtl()) {
      var right = window.innerWidth - rect.left - this.offsetWidth;
      this.style.right = right + 'px';
    } else {
      var left = rect.right - this.offsetWidth;
      this.style.left = left + 'px';
    }

    // Attempt to show the menu starting from the top of the rectangle and
    // extending downwards. If that does not fit within the window, fallback to
    // starting from the bottom and extending upwards.
    var top = rect.top + this.offsetHeight <= window.innerHeight ?
        rect.top :
        rect.bottom - this.offsetHeight - Math.max(
            rect.bottom - window.innerHeight, 0);

    this.style.top = top + 'px';
  },
});
