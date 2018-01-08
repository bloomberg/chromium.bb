// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displays a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */

cr.exportPath('settings');

/**
 * The name of the event fired from this element when the "Edit" option is
 * tapped.
 * @type {string}
 */
settings.EDIT_STARTUP_URL_EVENT = 'edit-startup-url';

Polymer({
  is: 'settings-startup-url-entry',

  behaviors: [FocusRowBehavior],

  properties: {
    editable: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {!StartupPageInfo} */
    model: Object,
  },

  /**
   * @param {string} url Location of an image to get a set of icons for.
   * @return {string} A set of icon URLs.
   * @private
   */
  getIconSet_: function(url) {
    return cr.icon.getFavicon(url);
  },

  /** @private */
  onRemoveTap_: function() {
    this.$$('dialog[is=cr-action-menu]').close();
    settings.StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditTap_: function(e) {
    e.preventDefault();
    this.$$('dialog[is=cr-action-menu]').close();
    this.fire(settings.EDIT_STARTUP_URL_EVENT, {
      model: this.model,
      anchor: this.$$('#dots'),
    });
  },

  /** @private */
  onDotsTap_: function() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    actionMenu.showAt(assert(this.$$('#dots')));
  },
});
