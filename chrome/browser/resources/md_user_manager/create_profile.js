// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'create-profile' is a page that contains controls for creating
 * a (optionally supervised) profile, including choosing a name, and an avatar.
 */
(function() {
/**
 * Sentinel signed-in user's index value.
 * @const {number}
 */
var NO_USER_SELECTED = -1;

Polymer({
  is: 'create-profile',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior
  ],

  properties: {
    /**
     * The current profile name.
     * @private {string}
     */
    profileName_: {
      type: String,
      value: ''
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
     * @private {!Array<!SignedInUser>}
     */
    signedInUsers_: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * Index of the selected signed-in user.
     * @private {number}
     */
    signedInUserIndex_: {
      type: Number,
      value: NO_USER_SELECTED
    },

    /**
     * Sentinel signed-in user's index value.
     * @private {number}
     */
    sentinelSignedInUserIndex_: {
      type: Number,
      value: NO_USER_SELECTED,
      readOnly: true
    },

    /** @private {!signin.ProfileBrowserProxy} */
    browserProxy_: Object
  },

  listeners: {
    'tap': 'onTap_',
    'importUserPopup.import': 'onImportUserPopupImport_'
  },

  /** @override */
  created: function() {
    this.browserProxy_ = signin.ProfileBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
      'create-profile-success', this.handleSuccess_.bind(this));
    this.addWebUIListener(
      'create-profile-warning', this.handleMessage_.bind(this));
    this.addWebUIListener(
      'create-profile-error', this.handleMessage_.bind(this));
    this.addWebUIListener(
      'profile-icons-received', this.handleProfileIcons_.bind(this));
    this.addWebUIListener(
      'profile-defaults-received', this.handleProfileDefaults_.bind(this));
    this.addWebUIListener(
      'signedin-users-received', this.handleSignedInUsers_.bind(this));

    this.browserProxy_.getAvailableIcons();
    this.browserProxy_.getSignedInUsers();
  },

  /**
   * Handles tap events from:
   * - links within dynamic warning/error messages pushed from the browser.
   * - the 'noSignedInUserMessage' i18n string.
   * @param {!Event} event
   * @private
   */
  onTap_: function(event) {
    var element = Polymer.dom(event).rootTarget;

    if (element.id == 'supervised-user-import-existing') {
      this.onImportUserTap_(event);
      event.preventDefault();
    } else if (element.id == 'sign-in-to-chrome') {
      this.browserProxy_.openUrlInLastActiveProfileBrowser(element.href);
      event.preventDefault();
    }
    // TODO(mahmadi): handle tap event on '#reauth' to re-auth the custodian.
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
   * Handler for when the profile defaults are pushed from the browser.
   * @param {!ProfileInfo} profileInfo Default Info for the new profile.
   * @private
   */
  handleProfileDefaults_: function(profileInfo) {
    this.profileName_ = profileInfo.name;
  },

  /**
   * Handler for when signed-in users are pushed from the browser.
   * @param {!Array<!SignedInUser>} signedInUsers
   * @private
   */
  handleSignedInUsers_: function(signedInUsers) {
    this.signedInUsers_ = signedInUsers;
  },

  /**
   * Returns the currently selected signed-in user.
   * @return {(!SignedInUser|undefined)}
   * @private
   */
  signedInUser_: function(signedInUserIndex) {
    return this.signedInUsers_[signedInUserIndex];
  },

  /**
   * Handler for the 'Learn More' link tap event.
   * @param {!Event} event
   * @private
   */
  onLearnMoreTap_: function(event) {
    this.fire('change-page', {page: 'supervised-learn-more-page'});
  },

  /**
   * Handler for the 'Import Supervised User' link tap event.
   * @param {!Event} event
   * @private
   */
  onImportUserTap_: function(event) {
    if (this.signedInUserIndex_ == NO_USER_SELECTED) {
      // A custodian must be selected.
      this.handleMessage_(this.i18n('custodianAccountNotSelectedError'));
    } else {
      var signedInUser = this.signedInUser_(this.signedInUserIndex_);
      this.createInProgress_ = true;
      this.browserProxy_.getExistingSupervisedUsers(signedInUser.profilePath)
          .then(this.showImportSupervisedUserPopup_.bind(this),
                this.handleMessage_.bind(this));
    }
  },

  /**
   * Handler for the 'Save' button tap event.
   * @param {!Event} event
   * @private
   */
  onSaveTap_: function(event) {
    if (!this.isSupervised_) {
      // The new profile is not supervised. Go ahead and create it.
      this.createProfile_();
    } else if (this.signedInUserIndex_ == NO_USER_SELECTED) {
      // If the new profile is supervised, a custodian must be selected.
      this.handleMessage_(this.i18n('custodianAccountNotSelectedError'));
    } else {
      var signedInUser = this.signedInUser_(this.signedInUserIndex_);
      this.createInProgress_ = true;
      this.browserProxy_.getExistingSupervisedUsers(signedInUser.profilePath)
          .then(this.createProfileIfValidSupervisedUser_.bind(this),
                this.handleMessage_.bind(this));
    }
  },

  /**
   * Displays the import supervised user popup.
   * @param {!Array<!SupervisedUser>} supervisedUsers The list of existing
   *     supervised users.
   * @private
   */
  showImportSupervisedUserPopup_: function(supervisedUsers) {
    this.createInProgress_ = false;
    this.$.importUserPopup.show(this.signedInUser_(this.signedInUserIndex_),
                                supervisedUsers);
  },

  /**
   * Checks if the entered name matches name of an existing supervised user.
   * If yes, the user is prompted to import the existing supervised user.
   * If no, the new supervised profile gets created.
   * @param {!Array<!SupervisedUser>} supervisedUsers The list of existing
   *     supervised users.
   * @private
   */
  createProfileIfValidSupervisedUser_: function(supervisedUsers) {
    for (var i = 0; i < supervisedUsers.length; ++i) {
      if (supervisedUsers[i].name != this.profileName_)
        continue;
      // Check if another supervised user also exists with that name.
      var nameIsUnique = true;
      // Handling the case when multiple supervised users with the same
      // name exist, but not all of them are on the device.
      // If at least one is not imported, we want to offer that
      // option to the user. This could happen due to a bug that allowed
      // creating SUs with the same name (https://crbug.com/557445).
      var allOnCurrentDevice = supervisedUsers[i].onCurrentDevice;
      for (var j = i + 1; j < supervisedUsers.length; ++j) {
        if (supervisedUsers[j].name == this.profileName_) {
          nameIsUnique = false;
          allOnCurrentDevice = allOnCurrentDevice &&
             supervisedUsers[j].onCurrentDevice;
        }
      }

      var opts = {
        'substitutions':
          [HTMLEscape(elide(this.profileName_, /* maxLength */ 50))],
        'attrs': {
          'id': function(node, value) {
            return node.tagName == 'A';
          },
          'is': function(node, value) {
            return node.tagName == 'A' && value == 'action-link';
          },
          'role': function(node, value) {
            return node.tagName == 'A' && value == 'link';
          },
          'tabindex': function(node, value) {
            return node.tagName == 'A';
          }
        }
      };

      this.handleMessage_(allOnCurrentDevice ?
          this.i18n('managedProfilesExistingLocalSupervisedUser') :
          this.i18nAdvanced('manageProfilesExistingSupervisedUser', opts));
      return;
    }
    // No existing supervised user's name matches the entered profile name.
    // Continue with creating the new supervised profile.
    this.createProfile_();
  },

  /**
   * Creates the new profile.
   * @private
   */
  createProfile_: function() {
    var custodianProfilePath = '';
    if (this.signedInUserIndex_ != NO_USER_SELECTED) {
      custodianProfilePath =
          this.signedInUser_(this.signedInUserIndex_).profilePath;
    }
    this.createInProgress_ = true;
    this.browserProxy_.createProfile(
        this.profileName_, this.profileIconUrl_, this.isSupervised_, '',
        custodianProfilePath);
  },

  /**
   * Handler for the 'import' event fired by #importUserPopup once a supervised
   * user is selected to be imported and the popup closes.
   * @param {!{detail: {supervisedUser: !SupervisedUser,
   *                    signedInUser: !SignedInUser}}} event
   * @private
   */
  onImportUserPopupImport_: function(event) {
    var supervisedUser = event.detail.supervisedUser;
    var signedInUser = event.detail.signedInUser;
    this.createInProgress_ = true;
    this.browserProxy_.createProfile(
        supervisedUser.name, supervisedUser.iconURL, true, supervisedUser.id,
        signedInUser.profilePath);
  },

  /**
   * Handler for the 'Cancel' button tap event.
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
    if (profileInfo.showConfirmation) {
      this.fire('change-page', {page: 'supervised-create-confirm-page',
                                data: profileInfo});
    } else {
      this.fire('change-page', {page: 'user-pods-page'});
    }
  },

  /**
   * Handles profile create/import warning/error message pushed by the browser.
   * @param {*} message An HTML warning/error message.
   * @private
   */
  handleMessage_: function(message) {
    this.createInProgress_ = false;
    this.message_ = '' + message;
  },

  /**
   * Returns a translated message that contains link elements with the 'id'
   * attribute.
   * @param {string} id The ID of the string to translate.
   * @private
   */
  i18nAllowIDAttr_: function(id) {
    var opts = {
      'attrs': {
        'id' : function(node, value) {
          return node.tagName == 'A';
        }
      }
    };

    return this.i18nAdvanced(id, opts);
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
   * Computed binding determining whether 'Save' button is disabled.
   * @param {boolean} createInProgress Is create in progress?
   * @param {string} profileName Profile Name.
   * @return {boolean}
   * @private
   */
  isSaveDisabled_: function(createInProgress, profileName) {
    // TODO(mahmadi): Figure out a way to add 'paper-input-extracted' as a
    // dependency and cast to PaperInputElement instead.
    /** @type {{validate: function():boolean}} */
    var nameInput = this.$.nameInput;
    return createInProgress || !profileName || !nameInput.validate();
  },

  /**
   * Returns True if the import supervised user link should be hidden.
   * @param {boolean} createInProgress True if create/import is in progress
   * @param {number} signedInUserIndex Index of the selected signed-in user.
   * @return {boolean}
   * @private
   */
  isImportUserLinkHidden_: function(createInProgress, signedInUserIndex) {
    return createInProgress || !this.signedInUser_(signedInUserIndex);
  },

  /**
   * Computed binding that returns True if there are any signed-in users.
   * @param {!Array<!SignedInUser>} signedInUsers signed-in users.
   * @return {boolean}
   * @private
   */
  isSignedIn_: function(signedInUsers) {
    return signedInUsers.length > 0;
  }
});
}());
