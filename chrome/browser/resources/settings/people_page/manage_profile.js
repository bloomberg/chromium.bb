// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-profile' is the settings subpage containing controls to
 * edit a profile's name, icon, and desktop shortcut.
 */
Polymer({
  is: 'settings-manage-profile',

  behaviors: [WebUIListenerBehavior, settings.RouteObserverBehavior],

  properties: {
    /**
     * The currently selected profile icon URL. May be a data URL.
     */
    profileIconUrl: String,

    /**
     * The current profile name.
     */
    profileName: String,

    /**
     * True if the current profile has a shortcut.
     */
    hasProfileShortcut_: Boolean,

    /**
     * The available icons for selection.
     * @type {!Array<string>}
     */
    availableIcons: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The current sync status.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * True if the profile shortcuts feature is enabled.
     */
    isProfileShortcutSettingVisible_: Boolean,
  },

  /** @private {?settings.ManageProfileBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.ManageProfileBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    var setIcons = function(icons) {
      this.availableIcons = icons;
    }.bind(this);

    this.addWebUIListener('available-icons-changed', setIcons);
    this.browserProxy_.getAvailableIcons().then(setIcons);
  },

  /** @protected */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.Route.MANAGE_PROFILE) {
      this.$.name.value = this.profileName;

      if (loadTimeData.getBoolean('profileShortcutsEnabled')) {
        this.browserProxy_.getProfileShortcutStatus().then(function(status) {
          if (status == ProfileShortcutStatus.PROFILE_SHORTCUT_SETTING_HIDDEN) {
            this.isProfileShortcutSettingVisible_ = false;
            return;
          }

          this.isProfileShortcutSettingVisible_ = true;
          this.hasProfileShortcut_ =
              status == ProfileShortcutStatus.PROFILE_SHORTCUT_FOUND;
        }.bind(this));
      }
    }
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @param {!Event} event
   * @private
   */
  onProfileNameChanged_: function(event) {
    if (event.target.invalid)
      return;

    this.browserProxy_.setProfileName(event.target.value);
  },

  /**
   * Handler for profile name keydowns.
   * @param {!Event} event
   * @private
   */
  onProfileNameKeydown_: function(event) {
    if (event.key == 'Escape') {
      event.target.value = this.profileName;
      event.target.blur();
    }
  },

  /**
   * Handler for when an avatar is activated.
   * @param {!Event} event
   * @private
   */
  onIconActivate_: function(event) {
    // Explicitly test against undefined, because even when an element has the
    // data-is-gaia-avatar attribute, dataset.isGaiaAvatar returns an empty
    // string, which is falsy.
    var isGaiaAvatar = event.detail.item.dataset.isGaiaAvatar !== undefined;

    if (isGaiaAvatar)
      this.browserProxy_.setProfileIconToGaiaAvatar();
    else
      this.browserProxy_.setProfileIconToDefaultAvatar(event.detail.selected);
  },

  /**
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean} Whether the profile name field is disabled.
   * @private
   */
  isProfileNameDisabled_: function(syncStatus) {
    return !!syncStatus.supervisedUser && !syncStatus.childUser;
  },

  /**
   * Handler for when the profile shortcut toggle is changed.
   * @param {!Event} event
   * @private
   */
  onHasProfileShortcutChange_: function(event) {
    if (this.hasProfileShortcut_) {
      this.browserProxy_.addProfileShortcut();
    } else {
      this.browserProxy_.removeProfileShortcut();
    }
  }
});
