// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Nav dot
 * This is the class for the navigation controls that appear along the bottom
 * of the NTP.
 */

cr.define('ntp', function() {
  'use strict';

  /**
   * Creates a new navigation dot.
   * @param {TilePage} page The associated TilePage.
   * @param {string} title The title of the navigation dot.
   * @constructor
   * @extends {HTMLLIElement}
   */
  function NavDot(page, title) {
    var dot = cr.doc.createElement('li');
    dot.__proto__ = NavDot.prototype;
    dot.initialize(page, title);

    return dot;
  }

  NavDot.prototype = {
    __proto__: HTMLLIElement.prototype,

    initialize: function(page, title) {
      this.className = 'dot';
      this.page_ = page;
      this.textContent = title;

      this.addEventListener('keydown', this.onKeyDown_);
      this.addEventListener('click', this.onClick_);
    },

    /**
     * @return {TilePage} The associated TilePage.
     */
    get page() {
      return this.page_;
    },

    /**
     * Removes the dot from the page.
     */
    remove: function() {
      this.parentNode.removeChild(this);
    },

    /**
     * Navigates the card slider to the page for this dot.
     */
    switchToPage: function() {
      ntp.getCardSlider().selectCardByValue(this.page_, true);
    },

    /**
     * Handler for keydown event on the dot.
     * @param {Event} e The KeyboardEvent.
     */
    onKeyDown_: function(e) {
      if (e.keyIdentifier == 'Enter') {
        this.onClick_(e);
        e.stopPropagation();
      }
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

  };

  return {
    NavDot: NavDot,
  };
});
