// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-profile' is the settings subpage containing controls to
 * edit a profile's name, icon, and desktop shortcut.
 *
 * @group Chrome Settings Elements
 * @element settings-manage-profile
 */
Polymer({
  is: 'settings-manage-profile',

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
     * The available icons for selection. Populated by SyncPrivateApi.
     * @type {!Array<!string>}
     */
    availableIconUrls: {
      type: Array,
      value: function() { return []; },
    },
  },

  /** @override */
  created: function() {
    settings.SyncPrivateApi.getAvailableIcons(
        this.handleAvailableIcons_.bind(this));
  },

  /**
   * Handler for when the available icons are pushed from SyncPrivateApi.
   * @private
   * @param {!Array<!string>} iconUrls
   */
  handleAvailableIcons_: function(iconUrls) {
    this.availableIconUrls = iconUrls;
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @private
   * @param {!Event} event
   */
  onProfileNameChanged_: function(event) {
    settings.SyncPrivateApi.setProfileIconAndName(this.profileIconUrl,
                                                  event.target.value);
  },

  /**
   * Handler for when the user clicks a new profile icon.
   * @private
   * @param {!Event} event
   */
  onIconTap_: function(event) {
    var element = Polymer.dom(event).rootTarget;

    var iconUrl;
    if (element.nodeName == 'IMG')
      iconUrl = element.src;
    else if (element.dataset && element.dataset.iconUrl)
      iconUrl = element.dataset.iconUrl;

    if (!iconUrl)
      return;

    settings.SyncPrivateApi.setProfileIconAndName(iconUrl, this.profileName);

    // Button toggle state is controlled by the selected icon URL. Prevent
    // tap events from changing the toggle state.
    event.preventDefault();
  },

  /**
   * Computed binding determining which profile icon button is toggled on.
   * @private
   * @param {!string} iconUrl
   * @param {!string} paramIconUrl
   * @return {boolean}
   */
  isActiveIcon_: function(iconUrl, profileIconUrl) {
    return iconUrl == profileIconUrl;
  },
});
