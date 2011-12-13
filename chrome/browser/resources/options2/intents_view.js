// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // IntentsView class:

  /**
   * Encapsulated handling of the Intents data page.
   * @constructor
   */
  function IntentsView(model) {
    OptionsPage.call(this, 'intents',
                     templateData.intentsViewPageTabTitle,
                     'intents-view-page');
  }

  cr.addSingletonGetter(IntentsView);

  IntentsView.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var intentsList = $('intents-list');
      options.IntentsList.decorate(intentsList);
      window.addEventListener('resize', this.handleResize_.bind(this));

      this.addEventListener('visibleChange', this.handleVisibleChange_);
    },

    initialized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @param {Event} e Property change event.
     * @private
     */
    handleVisibleChange_: function(e) {
      if (!this.visible)
        return;

      // Resize the intents list whenever the options page becomes visible.
      this.handleResize_(null);
      if (!this.initialized_) {
        this.initialized_ = true;
        chrome.send('loadIntents');
      } else {
        $('intents-list').redraw();
      }
    },

    /**
     * Handler for when the window changes size. Resizes the intents list to
     * match the window height.
     * @param {?Event} e Window resize event, or null if called directly.
     * @private
     */
    handleResize_: function(e) {
      if (!this.visible)
        return;
      var intentsList = $('intents-list');
      // 25 pixels from the window bottom seems like a visually pleasing amount.
      var height = window.innerHeight - intentsList.offsetTop - 25;
      intentsList.style.height = height + 'px';
    },
  };

  // IntentsViewHandler callbacks.
  IntentsView.loadChildren = function(args) {
    $('intents-list').loadChildren(args[0], args[1]);
  };

  // Export
  return {
    IntentsView: IntentsView
  };

});
