// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-item',

  properties: {
    timeAccessed_: {
      type: String,
      value: ''
    },

    websiteTitle_: {
      type: String,
      value: ''
    },

    // Domain is the website text shown on the history-item next to the title.
    // Gives the user some idea of which history items are different pages
    // belonging to the same site, and can be used to look for more items
    // from the same site.
    websiteDomain_: {
      type: String,
      value: ''
    },

    // The website url is used to define where the link should take you if
    // you click on the title, and also to define which icon the history-item
    // should display.
    websiteUrl_: {
      type: String,
      value: '',
      observer: 'showIcon_'
    },

    // If the website is a bookmarked page starred is true.
    starred: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    // The time in seconds of when the website was accessed.
    timestamp_: {
      type: Number,
      value: 0
    },

    selected: {
      type: Boolean,
      value: false,
      notify: true
    }
  },

  /**
   * When a history-item is selected the toolbar is notified and increases
   * or decreases its count of selected items accordingly.
   * @private
   */
  onCheckboxSelected_: function() {
    this.fire('history-checkbox-select', {
      countAddition: this.$.checkbox.checked ? 1 : -1
    });
  },

  /**
   * When the url for the history-item is set, the icon associated with this
   * website is also set.
   * @private
   */
  showIcon_: function() {
    this.$['website-icon'].style.backgroundImage =
        getFaviconImageSet(this.websiteUrl_);
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details of
   * the history item and where the menu should appear.
   */
  onMenuButtonTap_: function(e) {
    var position = this.$['menu-button'].getBoundingClientRect();

    this.fire('toggle-menu', {
      x: position.left,
      y: position.top,
      accessTime: this.timestamp_
    });

    // Stops the 'tap' event from closing the menu when it opens.
    e.stopPropagation();
  }
});
