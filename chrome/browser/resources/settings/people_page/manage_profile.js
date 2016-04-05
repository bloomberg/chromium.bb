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

  behaviors: [WebUIListenerBehavior],

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
     * The available icons for selection.
     * @type {!Array<string>}
     */
    availableIconUrls: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * @private {!settings.ManageProfileBrowserProxyImpl}
     */
    browserProxy_: {
      type: Object,
      value: function() {
        return settings.ManageProfileBrowserProxyImpl.getInstance();
      },
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('available-icons-changed', function(iconUrls) {
      this.availableIconUrls = iconUrls;
    }.bind(this));

    this.browserProxy_.getAvailableIcons();
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @private
   * @param {!Event} event
   */
  onProfileNameChanged_: function(event) {
    if (event.target.invalid)
      return;

    this.browserProxy_.setProfileIconAndName(this.profileIconUrl,
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

    this.browserProxy_.setProfileIconAndName(iconUrl, this.profileName);

    // Button toggle state is controlled by the selected icon URL. Prevent
    // tap events from changing the toggle state.
    event.preventDefault();
  },

  /**
   * Computed binding determining which profile icon button is toggled on.
   * @private
   * @param {string} iconUrl
   * @param {string} profileIconUrl
   * @return {boolean}
   */
  isActiveIcon_: function(iconUrl, profileIconUrl) {
    return iconUrl == profileIconUrl;
  },
});
