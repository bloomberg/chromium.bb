// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'supervised-user-learn-more' is a page that contains
 * information about what a supervised user is, what happens when a supervised
 * user is created, and a link to the help center for more information.
 */
Polymer({
  is: 'supervised-user-learn-more',

  /**
   * Handler for the 'Done' button click event.
   * @param {!Event} event
   * @private
   */
  onDoneTap_: function(event) {
    // Event is caught by user-manager-pages.
    this.fire('change-page', {page: 'create-user-page'});
  }
});
