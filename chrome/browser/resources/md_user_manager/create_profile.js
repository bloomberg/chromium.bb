// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'create-profile' is a page that contains controls for creating
 * a (optionally supervised) profile, including choosing a name, and an avatar.
 *
 * @element create-profile
 */
Polymer({
  is: 'create-profile',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * True if supervised user checkbox is disabled.
     * @private {boolean}
     */
    supervisedUserCheckboxDisabled_: {
      type: Boolean,
      computed:
          'isSupervisedUserCheckboxDisabled_(createInProgress_, signedIn_)'
    },

    /**
     * The current profile name.
     * @private {string}
     */
    profileName_: {
      type: String,
      value: '',
    },

    /**
     * The list of available profile icon URLs.
     * @private {!Array<string>}
     */
    availableIconUrls_: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * The currently selected profile icon URL. May be a data URL.
     * @private {string}
     */
    profileIconUrl_: {
      type: String,
      value: ''
    },

    /**
     * True if a profile is being created or imported.
     * @private {boolean}
     */
    createInProgress_: {
      type: Boolean,
      value: false
    },

    /**
     * The current error/warning message.
     * @private {string}
     */
    message_: {
      type: String,
      value: ''
    },

    /**
     * True if the new profile is a supervised profile.
     * @private {boolean}
     */
    isSupervised_: {
      type: Boolean,
      value: false
    },

    /**
     * The list of usernames and profile paths for currently signed-in users.
     * @private {!Array<SignedInUser>}
     */
    signedInUsers_: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * Index of the selected signed-in user.
     * @private {number}
     */
    selectedEmail_: {
      type: Number,
      value: 0
    },

    /** @private {!signin.ProfileBrowserProxy} */
    browserProxy_: Object
  },

  /** @override */
  created: function() {
    this.browserProxy_ = signin.ProfileBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.resetForm_();

    this.addWebUIListener(
      'create-profile-success', this.handleSuccess_.bind(this));
    this.addWebUIListener(
      'create-profile-warning', this.handleMessage_.bind(this));
    this.addWebUIListener(
      'create-profile-error', this.handleMessage_.bind(this));
    this.addWebUIListener(
      'profile-icons-received', this.handleProfileIcons_.bind(this));
    this.addWebUIListener(
      'signedin-users-received', this.handleSignedInUsers_.bind(this));

    this.browserProxy_.getAvailableIcons();
    this.browserProxy_.getSignedInUsers();
  },

  /**
   * Resets the state of the page.
   * @private
   */
  resetForm_: function() {
    this.profileName_ = '';
    this.availableIconUrls_ = [];
    this.profileIconUrl_ = '';
    this.createInProgress_ = false;
    this.message_ = '';
    this.isSupervised_ = false;
    this.signedInUsers_ = [];
    this.selectedEmail_ = 0;
  },

  /**
   * Handler for when the profile icons are pushed from the browser.
   * @param {!Array<string>} iconUrls
   * @private
   */
  handleProfileIcons_: function(iconUrls) {
    this.availableIconUrls_ = iconUrls;
    this.profileIconUrl_ = iconUrls[0];
  },

  /**
   * Updates the signed-in users.
   * @param {!Array<SignedInUser>} signedInUsers
   * @private
   */
  handleSignedInUsers_: function(signedInUsers) {
    this.signedInUsers_ = signedInUsers;
    this.signedIn_ = signedInUsers.length > 0;
  },

  /**
   * Handler for the 'Learn More' button click event.
   * @param {!Event} event
   * @private
   */
  onLearnMoreTap_: function(event) {
    // TODO(mahmadi): fire the event to show the 'learn-more-page'
  },

  /**
   * Handler for the 'Ok' button click event.
   * @param {!Event} event
   * @private
   */
  onSaveTap_: function(event) {
    this.createInProgress_ = true;
    this.browserProxy_.createProfile(
        this.profileName_, this.profileIconUrl_, this.isSupervised_,
        this.signedInUsers_[this.selectedEmail_].profilePath);
  },

  /**
   * Handler for the 'Cancel' button click event.
   * @param {!Event} event
   * @private
   */
  onCancelTap_: function(event) {
    if (this.createInProgress_) {
      this.createInProgress_ = false;
      this.browserProxy_.cancelCreateProfile();
    } else {
      this.fire('change-page', {page: 'user-pods-page'});
    }
  },

  /**
   * Handler for when the user clicks a new profile icon.
   * @param {!Event} event
   * @private
   */
  onIconTap_: function(event) {
    var element = Polymer.dom(event).rootTarget;

    if (element.nodeName == 'IMG')
      this.profileIconUrl_ = element.src;
    else if (element.dataset && element.dataset.iconUrl)
      this.profileIconUrl_ = element.dataset.iconUrl;

    // Button toggle state is controlled by the selected icon URL. Prevent
    // tap events from changing the toggle state.
    event.preventDefault();
  },

  /**
   * Handles profile create/import success message pushed by the browser.
   * @param {!ProfileInfo} profileInfo Details of the created/imported profile.
   * @private
   */
  handleSuccess_: function(profileInfo) {
    this.createInProgress_ = false;
    this.fire('change-page', {page: 'user-pods-page'});
  },

  /**
   * Handles profile create/import warning/error message pushed by the browser.
   * @param {string} message An HTML warning/error message.
   * @private
   */
  handleMessage_: function(message) {
    this.createInProgress_ = false;
    this.message_ = message;
  },

  /**
   * Computed binding determining which profile icon button is toggled on.
   * @param {string} iconUrl icon URL of a given icon button.
   * @param {string} profileIconUrl Currently selected icon URL.
   * @return {boolean}
   * @private
   */
  isActiveIcon_: function(iconUrl, profileIconUrl) {
    return iconUrl == profileIconUrl;
  },

  /**
   * Computed binding determining whether 'Ok' button is disabled.
   * @param {boolean} createInProgress Is create in progress?
   * @param {string} profileName Profile Name.
   * @param {string} message Existing warning/error message.
   * @return {boolean}
   * @private
   */
  isOkDisabled_: function(createInProgress, profileName, message) {
    // TODO(mahmadi): Figure out a way to add 'paper-input-extracted' as a
    // dependency and cast to PaperInputElement instead.
    /** @type {{validate: function():boolean}} */
    var nameInput = this.$.nameInput;
    return createInProgress || !profileName || message != '' ||
        !nameInput.validate();
  },

  /**
   * Computed binding determining whether supervised user checkbox is disabled.
   * @param {boolean} createInProgress Is create in progress?
   * @param {boolean} signedIn Are there any signed-in users?
   * @return {boolean}
   * @private
   */
  isSupervisedUserCheckboxDisabled_: function(createInProgress, signedIn) {
    return createInProgress || !signedIn;
  }
});
