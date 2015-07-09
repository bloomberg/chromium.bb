// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  /**
   * Size of additional padding in the inner scrollable section of the dropdown.
   */
  var DROPDOWN_INNER_PADDING = 12;

  /** Minimum height of toolbar dropdowns (px). */
  var MIN_DROPDOWN_HEIGHT = 300;

  Polymer({
    is: 'viewer-toolbar-dropdown',

    properties: {
      /** String to be displayed at the top of the dropdown. */
      header: String,

      /** Icon to display when the dropdown is closed. */
      closedIcon: String,

      /** Icon to display when the dropdown is open. */
      openIcon: String,

      /** True if the dropdown is currently open. */
      dropdownOpen: {
        type: Boolean,
        value: false
      },

      /** Toolbar icon currently being displayed. */
      dropdownIcon: {
        type: String,
        computed: 'computeIcon(dropdownOpen, closedIcon, openIcon)'
      },

      /** Lowest vertical point that the dropdown should occupy (px). */
      lowerBound: {
        type: Number,
        observer: 'lowerBoundChanged'
      },

      /**
       * True if the max-height CSS property for the dropdown scroll container
       * is valid. If false, the height will be updated the next time the
       * dropdown is visible.
       */
      maxHeightValid_: false
    },

    computeIcon: function(dropdownOpen, closedIcon, openIcon) {
      return dropdownOpen ? openIcon : closedIcon;
    },

    lowerBoundChanged: function() {
      this.maxHeightValid_ = false;
      if (this.dropdownOpen)
        this.updateMaxHeight();
    },

    toggleDropdown: function() {
      this.dropdownOpen = !this.dropdownOpen;
      if (this.dropdownOpen) {
        this.$.icon.classList.add('open');
        if (!this.maxHeightValid_)
          this.updateMaxHeight();
      } else {
        this.$.icon.classList.remove('open');
      }
    },

    updateMaxHeight: function() {
      var scrollContainer = this.$['scroll-container'];
      var height = this.lowerBound -
          scrollContainer.getBoundingClientRect().top -
          DROPDOWN_INNER_PADDING;
      height = Math.max(height, MIN_DROPDOWN_HEIGHT);
      scrollContainer.style.maxHeight = height + 'px';
      this.maxHeightValid_ = true;
    }
  });

})();
