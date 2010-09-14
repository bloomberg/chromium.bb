// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the sync options page.
   * @constructor
   */
  function SyncOptions() {
    OptionsPage.call(this, 'sync', templateData.syncPage, 'syncPage');
  }

  cr.addSingletonGetter(SyncOptions);

  SyncOptions.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      this.registerEventHandlers();
    },

    /**
     * Registers event handler on all the data type checkboxes so that the set
     * of data types to sync get updated as the user checks/unchecks them.
     */
     registerEventHandlers: function() {
      checks = $('syncPage').getElementsByTagName('input');
      for (var i = 0; i < checks.length; i++) {
        checks[i].onclick = function(event) {
          chrome.send('updatePreferredDataTypes');
        };
      }
    }
  };

  /**
   * Toggles the visibility of the data type checkboxes based on whether they
   * are enabled on not.
   * @param {Object} dict A mapping from data type to a boolean indicating
   *     whether it is enabled.
   */
  SyncOptions.setRegisteredDataTypes = function(dict) {
    for (var type in dict) {
      if (type.match(/Registered$/) && !dict[type]) {
        node = $(type.replace(/([a-z]+)Registered$/i, '$1').toLowerCase()
                 + '-check');
        if (node) {
          node.parentNode.style.display = 'none';
        }
      }
    }
  };

  // Export
  return {
    SyncOptions: SyncOptions
  };

});

