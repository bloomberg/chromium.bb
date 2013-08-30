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
      cr.ui.decorate('tabbox', cr.ui.TabBox);

      // Set up the three buttons (load unpacked, pack and update).
      document.querySelector('#apps-tab .load-unpacked').
          addEventListener('click', this.handleLoadUnpackedItem_.bind(this));
      document.querySelector('#extensions-tab .load-unpacked').
          addEventListener('click', this.handleLoadUnpackedItem_.bind(this));
      document.querySelector('#apps-tab .update-items-now').
          addEventListener('click', this.handleUpdateItemNow_.bind(this,
          document.querySelector('#apps-tab .update-items-progress')));
      document.querySelector('#extensions-tab .update-items-now').
          addEventListener('click', this.handleUpdateItemNow_.bind(this,
          document.querySelector('#extensions-tab .update-items-progress')));
      var packItemOverlay =
          apps_dev_tool.PackItemOverlay.getInstance().initializePage();

      preventDefaultOnPoundLinkClicks();  // From webui/js/util.js
    },

    /**
     * Handles the Load Unpacked Extension button.
     * @param {!Event} e Click event.
     * @private
     */
    handleLoadUnpackedItem_: function(e) {
      chrome.developerPrivate.loadUnpacked();
    },

    /**
     * Handles the Update Extension Now Button.
     * @param {!Element} tabNode Element containing the progress label.
     * @param {!Event} e Click event.
     * @private
     */
    handleUpdateItemNow_: function(progressLabelNode, e) {
      progressLabelNode.classList.add('updating');
      chrome.developerPrivate.autoUpdate(function(response) {
        // autoUpdate() will run too fast. We wait for 2 sec
        // before hiding the label so that the user can see it.
        setTimeout(function() {
          progressLabelNode.classList.remove('updating');
        }, 2000);
      });
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

      // Check managed profiles.
      chrome.developerPrivate.isProfileManaged(function(isManaged) {
        if (!isManaged)
          return;
        alertOverlay.setValues(
            loadTimeData.getString('managedProfileDialogTitle'),
            loadTimeData.getString('managedProfileDialogDescription'),
            loadTimeData.getString('managedProfileDialogCloseButton'),
            null,
            function() {
              AppsDevTool.showOverlay(null);
              window.close();
            },
            null);
        AppsDevTool.showOverlay($('alertOverlay'));
      });
    });
  };

  return {
    AppsDevTool: AppsDevTool,
  };
});
