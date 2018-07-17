// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying Discover UI.
 */
{
  const DISCOVER_WELCOME_MODULE = 'discoverWelcome';

  Polymer({
    is: 'discover-ui',

    behaviors: [I18nBehavior, OobeDialogHostBehavior],

    updateLocalizedContent: function() {
      this.i18nUpdateLocale();
    },

    /*
     * Enumerates modules, attaches common event handlers.
     * @override
     */
    attached: function() {
      // Initialize modules event handlers.
      let modules = Polymer.dom(this.root).querySelectorAll('.module');
      for (let i = 0; i < modules.length; ++i) {
        let module = modules[i];
        let handlerBack = this.showModule_.bind(
            this,
            (i > 0 ? modules[i - 1].getAttribute('module') :
                     'discoverWelcome'));
        let handlerContinue;
        if (i < modules.length - 1) {
          handlerContinue = this.showModule_.bind(
              this, modules[i + 1].getAttribute('module'));
        } else {
          handlerContinue = this.end_.bind(this);
        }
        module.addEventListener('module-back', handlerBack);
        module.addEventListener('module-continue', handlerContinue);
      }
    },

    /*
     * Overridden from OobeDialogHostBehavior.
     * @override
     */
    onBeforeShow: function() {
      OobeDialogHostBehavior.onBeforeShow.call(this);
      this.$.discoverWelcome.onBeforeShow();
      this.propagateFullScreenMode('.module');

      this.$.discoverWelcome.show();
    },

    /*
     * Hides all modules.
     * @private
     */
    hideAll_: function() {
      this.$.discoverWelcome.hidden = true;
      let modules = Polymer.dom(this.root).querySelectorAll('.module');
      for (let module of modules)
        module.hidden = true;
    },

    /*
     * Shows module identified by |moduleId|.
     * @param {string} moduleId Module name (shared between C++ and JS).
     * @private
     */
    showModule_: function(moduleId) {
      let module;
      if (moduleId === DISCOVER_WELCOME_MODULE) {
        module = this.$.discoverWelcome;
      } else {
        module = Polymer.dom(this.root).querySelector(
            '.module[module="' + moduleId + '"]');
      }
      if (module) {
        this.hideAll_();
        module.hidden = false;
        module.show();
      } else {
        console.error('Module "' + moduleId + '" not found!');
      }
    },

    /*
     * @param {!Event} event The onInput event that the function is handling.
     * @private
     */
    onCardClick_: function(event) {
      let module = event.target.module;
      this.showModule_(module);
    },

    /*
     * Fires signal that Discover app has ended.
     * @private
     */
    end_: function() {
      this.fire('discover-done');
    },

    /*
     * Starts linear navigation flow.
     * @private
     */
    startLinearFlow_: function() {
      this.showModule_(
          Polymer.dom(this.root).querySelector('.module').getAttribute(
              'module'));
    },
  });
}
