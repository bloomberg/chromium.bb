// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('management', function() {
  /**
   * A singleton object that handles communication between browser and WebUI.
   * @constructor
   */
  function Page() {}

  // Make Page a singleton.
  cr.addSingletonGetter(Page);

  Page.prototype = {
    /**
     * Main initialization function. Called by the browser on page load.
     */
    initialize: function() {
      cr.ui.FocusOutlineManager.forDocument(document);

      this.mainSection_ = $('main-section');

      // Notify the browser that the page has loaded, causing it to send the
      // management data.
      chrome.send('initialized');
    },

  };

  Page.showDeviceManagedStatus = function(managedString) {
    var page = this.getInstance();

    var string = document.createElement('p');
    string.textContent = managedString;
    page.mainSection_.appendChild(string);
  };

  return {Page: Page};
});

// Have the main initialization function be called when the page finishes
// loading.
document.addEventListener(
    'DOMContentLoaded',
    management.Page.prototype.initialize.bind(management.Page.getInstance()));
