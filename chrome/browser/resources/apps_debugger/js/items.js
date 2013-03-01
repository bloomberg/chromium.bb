// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('apps_dev_tool', function() {
  'use strict';

  /**
   * AppsDevTool constructor.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function AppsDevTool() {}

  AppsDevTool.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      // Set up the three buttons (load unpacked, pack and update).
      $('load-unpacked').addEventListener('click',
          this.handleLoadUnpackedItem_.bind(this));
      $('pack-item').addEventListener('click',
          this.handlePackItem_.bind(this));
      $('update-items-now').addEventListener('click',
          this.handleUpdateItemNow_.bind(this));
      $('permissions-close').addEventListener('click', function() {
        AppsDevTool.showOverlay(null);
      });
      var packItemOverlay =
          apps_dev_tool.PackItemOverlay.getInstance().initializePage();
    },

    /**
     * Handles the Load Unpacked Extension button.
     * @param {!Event} e Click event.
     * @private
     */
    handleLoadUnpackedItem_: function(e) {
      chrome.developerPrivate.loadUnpacked(function(success) {
        apps_dev_tool.ItemsList.loadItemsInfo();
      });
    },

    /** Handles the Pack Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handlePackItem_: function(e) {
      AppsDevTool.showOverlay($('packItemOverlay'));
    },

    /**
     * Handles the Update Extension Now Button.
     * @param {!Event} e Click event.
     * @private
     */
    handleUpdateItemNow_: function(e) {
      chrome.developerPrivate.autoUpdate(function(response) {});
    },
  };

  /**
   * Returns the current overlay or null if one does not exist.
   * @return {Element} The overlay element.
   */
  AppsDevTool.getCurrentOverlay = function() {
    return document.querySelector('#overlay .page.showing');
  };

  /**
   * Shows |el|. If there's another overlay showing, hide it.
   * @param {HTMLElement} el The overlay page to show. If falsey, all overlays
   *     are hidden.
   */
  AppsDevTool.showOverlay = function(el) {
    var currentlyShowingOverlay = AppsDevTool.getCurrentOverlay();
    if (currentlyShowingOverlay)
      currentlyShowingOverlay.classList.remove('showing');
    if (el)
      el.classList.add('showing');
    overlay.hidden = !el;
    uber.invokeMethodOnParent(el ? 'beginInterceptingEvents' :
                                   'stopInterceptingEvents');
  };

  /**
   * Loads translated strings.
   */
  AppsDevTool.initStrings = function() {
    chrome.developerPrivate.getStrings(function(strings) {
      loadTimeData.data = strings;
      i18nTemplate.process(document, loadTimeData);
    });
  };

  return {
    AppsDevTool: AppsDevTool,
  };
});
