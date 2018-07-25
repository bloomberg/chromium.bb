// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const RuntimeHostPermissions = Polymer({
    is: 'extensions-runtime-host-permissions',

    properties: {
      /**
       * The underlying permissions data.
       * @type {chrome.developerPrivate.Permissions}
       */
      permissions: Object,

      /** @private */
      itemId: String,

      /** @type {!extensions.ItemDelegate} */
      delegate: Object,

      /**
       * Whether the dialog to add a new host permission is shown.
       * @private
       */
      showHostDialog_: Boolean,

      /**
       * The current site of the entry that the host dialog is editing, if the
       * dialog is open for editing.
       * @type {?string}
       * @private
       */
      hostDialogModel_: {
        type: String,
        value: null,
      },

      /**
       * The element to return focus to once the host dialog closes.
       * @type {?HTMLElement}
       * @private
       */
      hostDialogAnchorElement_: {
        type: Object,
        value: null,
      },

      /**
       * If the action menu is open, the site of the entry it is open for.
       * Otherwise null.
       * @type {?string}
       * @private
       */
      actionMenuModel_: {
        type: String,
        value: null,
      },

      /**
       * The element that triggered the action menu, so that the page will
       * return focus once the action menu (or dialog) closes.
       * @type {?HTMLElement}
       * @private
       */
      actionMenuAnchorElement_: {
        type: Object,
        value: null,
      },

      /**
       * Proxying the enum to be used easily by the html template.
       * @private
       */
      HostAccess_: {
        type: Object,
        value: chrome.developerPrivate.HostAccess,
      },
    },

    /**
     * @param {!Event} event
     * @private
     */
    onHostAccessChange_: function(event) {
      const select = /** @type {!HTMLSelectElement} */ (event.target);
      const access =
          /** @type {chrome.developerPrivate.HostAccess} */ (select.value);
      this.delegate.setItemHostAccess(this.itemId, access);
      // Force the UI to update (in order to potentially hide or show the
      // specific runtime hosts).
      // TODO(devlin): Perhaps this should be handled by the backend updating
      // and sending an onItemStateChanged event?
      this.set('permissions.hostAccess', access);
    },

    /**
     * @return {boolean}
     * @private
     */
    showSpecificSites_: function() {
      return this.permissions &&
          this.permissions.hostAccess ==
          chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES;
    },

    /**
     * @param {Event} e
     * @private
     */
    onAddHostClick_: function(e) {
      const target = /** @type {!HTMLElement} */ (e.target);
      this.doShowHostDialog_(target, null);
    },

    /**
     * @param {!HTMLElement} anchorElement The element to return focus to once
     *     the dialog closes.
     * @param {?string} currentSite The site entry currently being
     *     edited, or null if this is to add a new entry.
     * @private
     */
    doShowHostDialog_: function(anchorElement, currentSite) {
      this.hostDialogAnchorElement_ = anchorElement;
      this.hostDialogModel_ = currentSite;
      this.showHostDialog_ = true;
    },

    /** @private */
    onHostDialogClose_: function() {
      this.hostDialogModel_ = null;
      this.showHostDialog_ = false;
      cr.ui.focusWithoutInk(
          assert(this.hostDialogAnchorElement_, 'Host Anchor'));
      this.hostDialogAnchorElement_ = null;
    },

    /**
     * @param {!{
     *   model: !{item: string},
     *   target: !HTMLElement,
     * }} e
     * @private
     */
    onEditHostClick_: function(e) {
      this.actionMenuModel_ = e.model.item;
      this.actionMenuAnchorElement_ = e.target;
      const actionMenu =
          /** @type {CrActionMenuElement} */ (this.$.hostActionMenu);
      actionMenu.showAt(e.target);
    },

    /** @private */
    onActionMenuEditClick_: function() {
      // Cache the site before closing the action menu, since it's cleared.
      const site = this.actionMenuModel_;

      // Cache and reset actionMenuAnchorElement_ so focus is not returned
      // to the action menu's trigger (since the dialog will be shown next).
      // Instead, curry the element to the dialog, so once it closes, focus
      // will be returned.
      const anchorElement =
          assert(this.actionMenuAnchorElement_, 'Menu Anchor');
      this.actionMenuAnchorElement_ = null;
      this.closeActionMenu_();
      this.doShowHostDialog_(anchorElement, site);
    },

    /** @private */
    onActionMenuRemoveClick_: function() {
      this.delegate.removeRuntimeHostPermission(
          this.itemId, assert(this.actionMenuModel_, 'Action Menu Model'));
      this.closeActionMenu_();
    },

    /** @private */
    closeActionMenu_: function() {
      const menu = this.$.hostActionMenu;
      assert(menu.open);
      menu.close();
    },

    /** @private */
    onActionMenuClose_: function() {
      this.actionMenuModel_ = null;
      this.actionMenuAnchorElement_ = null;
    },
  });

  return {RuntimeHostPermissions: RuntimeHostPermissions};
});
