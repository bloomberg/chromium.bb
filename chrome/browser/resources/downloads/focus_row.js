// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * Provides an implementation for a single column grid.
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  function FocusRow() {}

  /**
   * Decorates |focusRow| so that it can be treated as a FocusRow.
   * @param {Element} focusRow The element that has all the columns represented
   *     by |itemView|.
   * @param {!downloads.ItemView} itemView The item view this row cares about.
   * @param {Node} boundary Focus events are ignored outside of this node.
   */
  FocusRow.decorate = function(focusRow, itemView, boundary) {
    focusRow.__proto__ = FocusRow.prototype;
    focusRow.decorate(boundary);
    focusRow.addFocusableElements_();
  };

  /**
   * Determines if element should be focusable.
   * @param {Element} element
   * @return {boolean}
   */
  FocusRow.shouldFocus = function(element) {
    if (!element)
      return false;

    // Hidden elements are not focusable.
    var style = window.getComputedStyle(element);
    if (style.visibility == 'hidden' || style.display == 'none')
      return false;

    // Verify all ancestors are focusable.
    return !element.parentElement ||
           FocusRow.shouldFocus(element.parentElement);
  };

  FocusRow.prototype = {
    __proto__: cr.ui.FocusRow.prototype,

    /** @override */
    getEquivalentElement: function(element) {
      if (this.focusableElements.indexOf(element) > -1)
        return assert(element);

      // All elements default to another element with the same type.
      var columnType = element.getAttribute('column-type');
      var equivalent = this.querySelector('[column-type=' + columnType + ']');

      if (this.focusableElements.indexOf(equivalent) < 0) {
        equivalent = null;
        var equivalentTypes =
            ['show', 'retry', 'pause', 'resume', 'remove', 'cancel'];
        if (equivalentTypes.indexOf(columnType) != -1) {
          var allTypes = equivalentTypes.map(function(type) {
            return '[column-type=' + type + ']:not([hidden])';
          }).join(', ');
          equivalent = this.querySelector(allTypes);
        }
      }

      // Return the first focusable element if no equivalent element is found.
      return assert(equivalent || this.focusableElements[0]);
    },

    /** @private */
    addFocusableElements_: function() {
      var possiblyFocusableElements = this.querySelectorAll('[column-type]');
      for (var i = 0; i < possiblyFocusableElements.length; ++i) {
        var possiblyFocusableElement = possiblyFocusableElements[i];
        if (FocusRow.shouldFocus(possiblyFocusableElement))
          this.addFocusableElement(possiblyFocusableElement);
      }
    },
  };

  return {FocusRow: FocusRow};
});
