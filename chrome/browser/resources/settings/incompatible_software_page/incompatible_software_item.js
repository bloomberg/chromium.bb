// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'incompatible-software-item' represents one item in a "list-box" of
 * incompatible software, as defined in
 * chrome/browser/conflicts/problematic_programs_updater_win.h.
 * This element contains a button that can be used to remove or update the
 * incompatible software, depending on the value of the action-type property.
 *
 * Example usage:
 *
 *   <div class="list-box">
 *     <incompatible-software-item
 *       software-name="Google Chrome"
 *       action-type="1"
 *       action-url="https://www.google.com/chrome/more-info">
 *     </incompatible-software-item>
 *   </div>
 *
 * or
 *
 *   <div class="list-box">
 *     <template is="dom-repeat" items="[[softwareList]]" as="software">
 *       <incompatible-software-item
 *         software-name="[[software.name]]"
 *         action-type="[[software.actionType]]"
 *         action-url="[[software.actionUrl]]">
 *       </incompatible-software-item>
 *     </template>
 *   </div>
 */

Polymer({
  is: 'incompatible-software-item',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The name of the software to be displayed. Also used for the UNINSTALL
     * action, where the name is passed to the startProgramUninstallation()
     * call.
     */
    softwareName: String,

    /**
     * The type of the action to be taken on this incompatible software. Must be
     * one of BlacklistMessageType in
     * chrome/browser/conflicts/proto/module_list.proto.
     * @type {!settings.ActionTypes}
     */
    actionType: Number,

    /**
     * For the actions MORE_INFO and UPGRADE, this is the URL that must be
     * opened when the action button is tapped.
     */
    actionUrl: String,
  },

  /** @private {settings.IncompatibleSoftwareBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ =
        settings.IncompatibleSoftwareBrowserProxyImpl.getInstance();
  },

  /**
   * Executes the action for this incompatible software, depending on
   * actionType.
   * @private
   */
  onActionTap_: function() {
    if (this.actionType === settings.ActionTypes.UNINSTALL) {
      this.browserProxy_.startProgramUninstallation(this.softwareName);
    } else if (
        this.actionType === settings.ActionTypes.MORE_INFO ||
        this.actionType === settings.ActionTypes.UPGRADE) {
      this.browserProxy_.openURL(this.actionUrl);
    } else {
      assertNotReached();
    }
  },

  /**
   * @return {string} The label that should be applied to the action button.
   * @private
   */
  getActionName_: function(actionType) {
    if (actionType === settings.ActionTypes.UNINSTALL)
      return this.i18n('incompatibleSoftwareRemoveButton');
    if (actionType === settings.ActionTypes.MORE_INFO)
      return this.i18n('learnMore');
    if (actionType === settings.ActionTypes.UPGRADE)
      return this.i18n('incompatibleSoftwareUpdateButton');
    assertNotReached();
  },
});
