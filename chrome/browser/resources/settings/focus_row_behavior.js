// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {cr.ui.FocusRowDelegate} */
class FocusRowDelegate {
  /** @param {{lastFocused: Object}} listItem */
  constructor(listItem) {
    /** @private */
    this.listItem_ = listItem;
  }

  /**
   * This function gets called when the [focus-row-control] element receives
   * the focus event.
   * @override
   * @param {!cr.ui.FocusRow} row
   * @param {!Event} e
   */
  onFocus(row, e) {
    this.listItem_.lastFocused = e.path[0];
  }

  /**
   * @override
   * @param {!cr.ui.FocusRow} row The row that detected a keydown.
   * @param {!Event} e
   * @return {boolean} Whether the event was handled.
   */
  onKeydown(row, e) {
    // Prevent iron-list from changing the focus on enter.
    if (e.key == 'Enter')
      e.stopPropagation();

    return false;
  }
}

/** @extends {cr.ui.FocusRow} */
class VirtualFocusRow extends cr.ui.FocusRow {
  /**
   * @param {!Element} root
   * @param {cr.ui.FocusRowDelegate} delegate
   */
  constructor(root, delegate) {
    super(root, /* boundary */ null, delegate);
  }
}

/**
 * Any element that is being used as an iron-list row item can extend this
 * behavior, which encapsulates focus controls of mouse and keyboards.
 * To use this behavior:
 *    - The parent element should pass a "last-focused" attribute double-bound
 *      to the row items, to track the last-focused element across rows.
 *    - There must be a container in the extending element with the
 *      [focus-row-container] attribute that contains all focusable controls.
 *    - On each of the focusable controls, there must be a [focus-row-control]
 *      attribute, and a [focus-type=] attribute unique for each control.
 *
 * @polymerBehavior
 */
const FocusRowBehavior = {
  properties: {
    /** @private {VirtualFocusRow} */
    row_: Object,

    /** @private {boolean} */
    mouseFocused_: Boolean,

    /** @type {Element} */
    lastFocused: {
      type: Object,
      notify: true,
    },

    /**
     * This is different from tabIndex, since the template only does a one-way
     * binding on both attributes, and the behavior actually make use of this
     * fact. For example, when a control within a row is focused, it will have
     * tabIndex = -1 and ironListTabIndex = 0.
     * @type {number}
     */
    ironListTabIndex: {
      type: Number,
      observer: 'ironListTabIndexChanged_',
    },
  },

  /** @private {?Element} */
  firstControl_: null,

  /** @private {!Array<!MutationObserver>} */
  controlObservers_: [],

  /** @override */
  attached: function() {
    this.classList.add('no-outline');

    Polymer.RenderStatus.afterNextRender(this, function() {
      const rowContainer = this.root.querySelector('[focus-row-container]');
      assert(!!rowContainer);
      this.row_ = new VirtualFocusRow(rowContainer, new FocusRowDelegate(this));
      this.ironListTabIndexChanged_();
      this.addItems_();

      // Adding listeners asynchronously to reduce blocking time, since this
      // behavior will be used by items in potentially long lists.
      this.listen(this, 'focus', 'onFocus_');
      this.listen(this, 'dom-change', 'addItems_');
      this.listen(this, 'mousedown', 'onMouseDown_');
      this.listen(this, 'blur', 'onBlur_');
    });
  },

  /** @override */
  detached: function() {
    this.unlisten(this, 'focus', 'onFocus_');
    this.unlisten(this, 'dom-change', 'addItems_');
    this.unlisten(this, 'mousedown', 'onMouseDown_');
    this.unlisten(this, 'blur', 'onBlur_');
    this.removeObservers_();
    if (this.row_)
      this.row_.destroy();
  },

  /** @private */
  updateFirstControl_: function() {
    const newFirstControl = this.row_.getFirstFocusable();
    if (newFirstControl === this.firstControl_)
      return;

    if (this.firstControl_)
      this.unlisten(this.firstControl_, 'keydown', 'onFirstControlKeydown_');
    this.firstControl_ = newFirstControl;
    if (this.firstControl_) {
      this.listen(
          /** @type {!Element} */ (this.firstControl_), 'keydown',
          'onFirstControlKeydown_');
    }
  },

  /** @private */
  removeObservers_: function() {
    if (this.firstControl_)
      this.unlisten(this.firstControl_, 'keydown', 'onFirstControlKeydown_');
    if (this.controlObservers_.length > 0)
      this.controlObservers_.forEach(observer => {
        observer.disconnect();
      });
    this.controlObservers_ = [];
  },

  /** @private */
  addItems_: function() {
    if (this.row_) {
      this.removeObservers_();
      this.row_.destroy();

      const controls = this.root.querySelectorAll('[focus-row-control]');

      controls.forEach(control => {
        this.row_.addItem(
            control.getAttribute('focus-type'),
            /** @type {!HTMLElement} */
            (cr.ui.FocusRow.getFocusableElement(control)));
        this.addMutationObservers_(assert(control));
      });
      this.updateFirstControl_();
    }
  },

  /**
   * @return {!MutationObserver}
   * @private
   */
  createObserver_: function() {
    return new MutationObserver(mutations => {
      const mutation = mutations[0];
      if (mutation.attributeName === 'style' && mutation.oldValue) {
        const newStyle =
            window.getComputedStyle(/** @type {!Element} */ (mutation.target));
        const oldDisplayValue = mutation.oldValue.match(/^display:(.*)(?=;)/);
        const oldVisibilityValue =
            mutation.oldValue.match(/^visibility:(.*)(?=;)/);
        // Return early if display and visibility have not changed.
        if (oldDisplayValue && newStyle.display === oldDisplayValue[1].trim() &&
            oldVisibilityValue &&
            newStyle.visibility === oldVisibilityValue[1].trim()) {
          return;
        }
      }
      this.updateFirstControl_();
    });
  },

  /**
   * The first focusable control changes if hidden, disabled, or style.display
   * changes for the control or any of its ancestors. Add mutation observers to
   * watch for these changes in order to ensure the first control keydown
   * listener is always on the correct element.
   * @param {!Element} control
   * @private
   */
  addMutationObservers_: function(control) {
    let current = control;
    while (current && current != this.root) {
      const currentObserver = this.createObserver_();
      currentObserver.observe(current, {
        attributes: true,
        attributeFilter: ['hidden', 'disabled', 'style'],
        attributeOldValue: true,
      });
      this.controlObservers_.push(currentObserver);
      current = current.parentNode;
    }
  },

  /**
   * This function gets called when the row itself receives the focus event.
   * @private
   */
  onFocus_: function() {
    if (this.mouseFocused_) {
      this.mouseFocused_ = false;  // Consume and reset flag.
      return;
    }

    if (this.lastFocused) {
      this.row_.getEquivalentElement(this.lastFocused).focus();
    } else {
      const firstFocusable = assert(this.firstControl_);
      firstFocusable.focus();
    }
  },

  /** @param {!KeyboardEvent} e */
  onFirstControlKeydown_: function(e) {
    if (e.shiftKey && e.key === 'Tab')
      this.focus();
  },

  /** @private */
  ironListTabIndexChanged_: function() {
    if (this.row_)
      this.row_.makeActive(this.ironListTabIndex == 0);
  },

  /** @private */
  onMouseDown_: function() {
    this.mouseFocused_ = true;  // Set flag to not do any control-focusing.
  },

  /** @private */
  onBlur_: function() {
    this.mouseFocused_ = false;  // Reset flag since it's not active anymore.
  }
};
