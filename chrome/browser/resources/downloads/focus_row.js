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

  FocusRow.prototype = {
    __proto__: cr.ui.FocusRow.prototype,

    /**
     * TODO(dbeam): remove all this :not([hidden]) hackery and just create 2 new
     * methods on cr.ui.FocusRow that get possibly focusable nodes as well as
     * currently focusable nodes (taking into account visibility).
     * @override
     */
    getEquivalentElement: function(element) {
      if (this.focusableElements.indexOf(element) > -1 &&
          cr.ui.FocusRow.isFocusable(element)) {
        return assert(element);
      }

      // All elements default to another element with the same type.
      var columnType = element.getAttribute('focus-type');
      var equivalent = this.querySelector(
          '[focus-type=' + columnType + ']:not([hidden])');

      if (this.focusableElements.indexOf(equivalent) < 0) {
        equivalent = null;
        var equivalentTypes =
            ['show', 'retry', 'pause', 'resume', 'remove', 'cancel'];
        if (equivalentTypes.indexOf(columnType) != -1) {
          var allTypes = equivalentTypes.map(function(type) {
            return '[focus-type=' + type + ']:not([hidden])';
          }).join(', ');
          equivalent = this.querySelector(allTypes);
        }
      }

      // Return the first focusable element if no equivalent element is found.
      return assert(equivalent || this.focusableElements[0]);
    },

    /** @private */
    addFocusableElements_: function() {
      var possiblyFocusableElements = this.querySelectorAll('[focus-type]');
      for (var i = 0; i < possiblyFocusableElements.length; ++i) {
        var possiblyFocusableElement = possiblyFocusableElements[i];
        if (cr.ui.FocusRow.isFocusable(possiblyFocusableElement))
          this.addFocusableElement(possiblyFocusableElement);
      }
    },
  };

  return {FocusRow: FocusRow};
});
