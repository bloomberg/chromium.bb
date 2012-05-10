// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // VirtualKeyboardManager class:

  /**
   * Virtual keyboard management page.
   * @constructor
   */
  function VirtualKeyboardManager() {
    this.activeNavTab = null;
    OptionsPage.call(this,
                     'virtualKeyboards',
                     // The templateData.virtualKeyboardPageTabTitle is added
                     // in OptionsPageUIHandler::RegisterTitle().
                     templateData.virtualKeyboardPageTabTitle,
                     'virtual-keyboard-manager');
  }

  cr.addSingletonGetter(VirtualKeyboardManager);

  VirtualKeyboardManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The virtual keyboards list.
     * @type {ItemList}
     * @private
     */
    virtualKeyboardsList_: null,

    /** @inheritDoc */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      this.createVirtualKeyboardsList_();
      $('virtual-keyboard-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /** @inheritDoc */
    didShowPage: function() {
      chrome.send('updateVirtualKeyboardList');
    },

    /**
     * Creates, decorates and initializes the keyboards list.
     * @private
     */
    createVirtualKeyboardsList_: function() {
      this.virtualKeyboardsList_ = $('virtual-keyboard-per-layout-list');
      options.VirtualKeyboardsList.decorate(this.virtualKeyboardsList_);
      this.virtualKeyboardsList_.autoExpands = true;
    },
  };

  /**
   * Sets the list of virtual keyboards shown in the view. This function is
   * called by C++ code (e.g. chrome/browser/ui/webui/options/chromeos/).
   * @param {Object} list A list of layouts with their registered virtual
   *     keyboards.
   */
  VirtualKeyboardManager.updateVirtualKeyboardList = function(list) {
    // See virtual_keyboard_list.js for an example of the format the list should
    // take.
    var filteredList = list.filter(function(element, index, array) {
        // Don't show a layout which is supported by only one virtual keyboard
        // extension.
        return element.supportedKeyboards.length > 1;
      });

    // Sort the drop-down menu items by name.
    filteredList.forEach(function(e) {
        e.supportedKeyboards.sort(function(e1, e2) {
            return e1.name > e2.name;
          });
      });

    // Sort the list by layout name.
    $('virtual-keyboard-per-layout-list').setVirtualKeyboardList(
        filteredList.sort(function(e1, e2) {
            return e1.layoutName > e2.layoutName;
          }));
  };

  // Export
  return {
    VirtualKeyboardManager: VirtualKeyboardManager,
  };
});
