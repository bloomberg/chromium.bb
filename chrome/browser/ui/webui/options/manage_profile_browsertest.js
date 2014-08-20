// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// None of these tests is relevant for Chrome OS.
GEN('#if !defined(OS_CHROMEOS)');

/**
 * TestFixture for ManageProfileOverlay and CreateProfileOverlay WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ManageProfileUITest() {}

ManageProfileUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame/manageProfile',

  /**
   * No need to run these for every OptionsPage test, since they'll cover the
   * whole consolidated page each time.
   * @override
   */
  runAccessibilityChecks: false,

  /**
   * Some default profile infos.
   */
  defaultIconURLs: [],
  defaultNames: [],

  /**
   * Returns a test profile-info object with configurable "supervised" status.
   * @param {boolean} supervised If true, the test profile will be marked as
   *     supervised.
   * @return {Object} A test profile-info object.
   */
  testProfileInfo_: function(supervised) {
    return {
      name: 'Test Profile',
      iconURL: 'chrome://path/to/icon/image',
      filePath: '/path/to/profile/data/on/disk',
      isCurrentProfile: true,
      isSupervised: supervised
    };
  },

  /**
   * Overrides WebUI methods that provide profile info, making them return a
   * test profile-info object.
   * @param {boolean} supervised Whether the test profile should be marked
   *     as supervised.
   * @param {string} mode The mode of the overlay (either 'manage' or 'create').
   */
  setProfileSupervised_: function(supervised, mode) {
    // Override the BrowserOptions method to return the fake info.
    BrowserOptions.getCurrentProfile = function() {
      return this.testProfileInfo_(supervised);
    }.bind(this);
    // Set the profile info in the overlay.
    ManageProfileOverlay.setProfileInfo(this.testProfileInfo_(supervised),
                                        mode);
  },

  /**
   * Set some default profile infos (icon URLs and names).
   * @param {boolean} supervised Whether the test profile should be marked as
   *     supervised.
   * @param {string} mode The mode of the overlay (either 'manage' or 'create').
   */
  initDefaultProfiles_: function(mode) {
    PageManager.showPageByName(mode + 'Profile');

    var defaultProfile = {
      name: 'Default Name',
      iconURL: '/default/path',
    };
    this.defaultIconURLs = ['/some/path',
                            defaultProfile.iconURL,
                            '/another/path',
                            '/one/more/path'];
    this.defaultNames = ['Some Name', defaultProfile.name, '', 'Another Name'];
    ManageProfileOverlay.receiveDefaultProfileIconsAndNames(
        mode, this.defaultIconURLs, this.defaultNames);
    ManageProfileOverlay.receiveNewProfileDefaults(defaultProfile);

    // Make sure the correct item in the icon grid was selected.
    var gridEl = $(mode + '-profile-icon-grid');
    expectEquals(defaultProfile.iconURL, gridEl.selectedItem);
  },
};

// Receiving the new profile defaults in the manage-user overlay shouldn't mess
// up the focus in a visible higher-level overlay.
TEST_F('ManageProfileUITest', 'NewProfileDefaultsFocus', function() {
  var self = this;

  function checkFocus(pageName, expectedFocus, initialFocus) {
    PageManager.showPageByName(pageName);
    initialFocus.focus();
    expectEquals(initialFocus, document.activeElement, pageName);

    ManageProfileOverlay.receiveNewProfileDefaults(
        self.testProfileInfo_(false));
    expectEquals(expectedFocus, document.activeElement, pageName);
    PageManager.closeOverlay();
  }

  // Receiving new profile defaults sets focus to the name field if the create
  // overlay is open, and should not change focus at all otherwise.
  checkFocus('manageProfile',
             $('manage-profile-cancel'),
             $('manage-profile-cancel'));
  checkFocus('createProfile',
             $('create-profile-name'),
             $('create-profile-cancel'));
  checkFocus('supervisedUserLearnMore',
             $('supervised-user-learn-more-done'),
             $('supervised-user-learn-more-done'));
  checkFocus('supervisedUserLearnMore',
             document.querySelector('#supervised-user-learn-more-text a'),
             document.querySelector('#supervised-user-learn-more-text a'));
});

// The default options should be reset each time the creation overlay is shown.
TEST_F('ManageProfileUITest', 'DefaultCreateOptions', function() {
  PageManager.showPageByName('createProfile');
  var shortcutsAllowed = loadTimeData.getBoolean('profileShortcutsEnabled');
  var createShortcut = $('create-shortcut');
  var createSupervised = $('create-profile-supervised');
  assertEquals(shortcutsAllowed, createShortcut.checked);
  assertFalse(createSupervised.checked);

  createShortcut.checked = !shortcutsAllowed;
  createSupervised.checked = true;
  PageManager.closeOverlay();
  PageManager.showPageByName('createProfile');
  assertEquals(shortcutsAllowed, createShortcut.checked);
  assertFalse(createSupervised.checked);
});

// The checkbox label should change depending on whether the user is signed in.
TEST_F('ManageProfileUITest', 'CreateSupervisedUserText', function() {
  var signedInText = $('create-profile-supervised-signed-in');
  var notSignedInText = $('create-profile-supervised-not-signed-in');

  ManageProfileOverlay.getInstance().initializePage();

  var custodianEmail = 'chrome.playpen.test@gmail.com';
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  assertEquals(custodianEmail,
               CreateProfileOverlay.getInstance().signedInEmail_);
  assertFalse(signedInText.hidden);
  assertTrue(notSignedInText.hidden);
  // Make sure the email is in the string somewhere, without depending on the
  // exact details of the message.
  assertNotEquals(-1, signedInText.textContent.indexOf(custodianEmail));

  CreateProfileOverlay.updateSignedInStatus('');
  assertEquals('', CreateProfileOverlay.getInstance().signedInEmail_);
  assertTrue(signedInText.hidden);
  assertFalse(notSignedInText.hidden);
  assertFalse($('create-profile-supervised').checked);
  assertTrue($('create-profile-supervised').disabled);
});

function ManageProfileUITestAsync() {}

ManageProfileUITestAsync.prototype = {
  __proto__: ManageProfileUITest.prototype,

  isAsync: true,
};

// The import link should show up if the user tries to create a profile with the
// same name as an existing supervised user profile.
TEST_F('ManageProfileUITestAsync', 'CreateExistingSupervisedUser', function() {
  // Initialize the list of existing supervised users.
  var supervisedUsers = [
    {
      id: 'supervisedUser1',
      name: 'Rosalie',
      iconURL: 'chrome://path/to/icon/image',
      onCurrentDevice: false,
      needAvatar: false
    },
    {
      id: 'supervisedUser2',
      name: 'Fritz',
      iconURL: 'chrome://path/to/icon/image',
      onCurrentDevice: false,
      needAvatar: true
    },
    {
      id: 'supervisedUser3',
      name: 'Test',
      iconURL: 'chrome://path/to/icon/image',
      onCurrentDevice: true,
      needAvatar: false
    },
    {
      id: 'supervisedUser4',
      name: 'SameName',
      iconURL: 'chrome://path/to/icon/image',
      onCurrentDevice: false,
      needAvatar: false
    }];
  var promise = Promise.resolve(supervisedUsers);
  options.SupervisedUserListData.getInstance().promise_ = promise;

  // Initialize the ManageProfileOverlay.
  ManageProfileOverlay.getInstance().initializePage();
  var custodianEmail = 'chrome.playpen.test@gmail.com';
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  assertEquals(custodianEmail,
               CreateProfileOverlay.getInstance().signedInEmail_);
  this.setProfileSupervised_(false, 'create');

  // Also add the names 'Test' and 'SameName' to |existingProfileNames_| to
  // simulate that profiles with those names exist on the device.
  ManageProfileOverlay.getInstance().existingProfileNames_.Test = true;
  ManageProfileOverlay.getInstance().existingProfileNames_.SameName = true;

  // Initially, the ok button should be enabled and the import link should not
  // exist.
  assertFalse($('create-profile-ok').disabled);
  assertTrue($('supervised-user-import-existing') == null);

  // Now try to create profiles with the names of existing supervised users.
  $('create-profile-supervised').checked = true;
  var nameField = $('create-profile-name');
  // A profile which already has an avatar.
  nameField.value = 'Rosalie';
  ManageProfileOverlay.getInstance().onNameChanged_('create');
  // Need to wait until the promise resolves.
  promise.then(function() {
    assertTrue($('create-profile-ok').disabled);
    assertFalse($('supervised-user-import-existing') == null);

    // A profile which doesn't have an avatar yet.
    nameField.value = 'Fritz';
    ManageProfileOverlay.getInstance().onNameChanged_('create');
    return options.SupervisedUserListData.getInstance().promise_;
  }).then(function() {
    assertTrue($('create-profile-ok').disabled);
    assertFalse($('supervised-user-import-existing') == null);

    // A profile which already exists on the device.
    nameField.value = 'Test';
    ManageProfileOverlay.getInstance().onNameChanged_('create');
    return options.SupervisedUserListData.getInstance().promise_;
  }).then(function() {
    assertTrue($('create-profile-ok').disabled);
    assertTrue($('supervised-user-import-existing') == null);

    // A profile which does not exist on the device, but there is a profile with
    // the same name already on the device.
    nameField.value = 'SameName';
    ManageProfileOverlay.getInstance().onNameChanged_('create');
    return options.SupervisedUserListData.getInstance().promise_;
  }).then(function() {
    assertTrue($('create-profile-ok').disabled);
    assertFalse($('supervised-user-import-existing') == null);

    // A profile which does not exist yet.
    nameField.value = 'NewProfileName';
    ManageProfileOverlay.getInstance().onNameChanged_('create');
    return options.SupervisedUserListData.getInstance().promise_;
  }).then(function() {
    assertFalse($('create-profile-ok').disabled);
    assertTrue($('supervised-user-import-existing') == null);
    testDone();
  });
});

// Supervised users should not be able to edit their profile names, and the
// initial focus should be adjusted accordingly.
TEST_F('ManageProfileUITest', 'EditSupervisedUserNameAllowed', function() {
  var nameField = $('manage-profile-name');

  this.setProfileSupervised_(false, 'manage');
  ManageProfileOverlay.showManageDialog();
  expectFalse(nameField.disabled);
  expectEquals(nameField, document.activeElement);

  PageManager.closeOverlay();

  this.setProfileSupervised_(true, 'manage');
  ManageProfileOverlay.showManageDialog();
  expectTrue(nameField.disabled);
  expectEquals($('manage-profile-ok'), document.activeElement);
});

// Setting profile information should allow the confirmation to be shown.
TEST_F('ManageProfileUITest', 'ShowCreateConfirmation', function() {
  var testProfile = this.testProfileInfo_(true);
  testProfile.custodianEmail = 'foo@bar.example.com';
  SupervisedUserCreateConfirmOverlay.setProfileInfo(testProfile);
  assertTrue(SupervisedUserCreateConfirmOverlay.getInstance().canShowPage());
  PageManager.showPageByName('supervisedUserCreateConfirm', false);
  assertEquals('supervisedUserCreateConfirm',
               PageManager.getTopmostVisiblePage().name);
});

// Trying to show a confirmation dialog with no profile information should fall
// back to the default (main) settings page.
TEST_F('ManageProfileUITest', 'NoEmptyConfirmation', function() {
  assertEquals('manageProfile', PageManager.getTopmostVisiblePage().name);
  assertFalse(SupervisedUserCreateConfirmOverlay.getInstance().canShowPage());
  PageManager.showPageByName('supervisedUserCreateConfirm', true);
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);
});

// A confirmation dialog should be shown after creating a new supervised user.
TEST_F('ManageProfileUITest', 'ShowCreateConfirmationOnSuccess', function() {
  PageManager.showPageByName('createProfile');
  assertEquals('createProfile', PageManager.getTopmostVisiblePage().name);
  CreateProfileOverlay.onSuccess(this.testProfileInfo_(false));
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);

  PageManager.showPageByName('createProfile');
  assertEquals('createProfile', PageManager.getTopmostVisiblePage().name);
  CreateProfileOverlay.onSuccess(this.testProfileInfo_(true));
  assertEquals('supervisedUserCreateConfirm',
               PageManager.getTopmostVisiblePage().name);
  expectEquals($('supervised-user-created-switch'), document.activeElement);
});

// An error should be shown if creating a new supervised user fails.
TEST_F('ManageProfileUITest', 'NoCreateConfirmationOnError', function() {
  PageManager.showPageByName('createProfile');
  assertEquals('createProfile', PageManager.getTopmostVisiblePage().name);
  var errorBubble = $('create-profile-error-bubble');
  assertTrue(errorBubble.hidden);

  CreateProfileOverlay.onError('An Error Message!');
  assertEquals('createProfile', PageManager.getTopmostVisiblePage().name);
  assertFalse(errorBubble.hidden);
});

// The name and email should be inserted into the confirmation dialog.
// Disbaled because of flakiness on Mac: crbug.com/405709
GEN('#if defined(OS_MACOSX)');
GEN('#define MAYBE_CreateConfirmationText \\');
GEN('    DISABLED_CreateConfirmationText');
GEN('#else');
GEN('#define MAYBE_CreateConfirmationText CreateConfirmationText');
GEN('#endif');
TEST_F('ManageProfileUITest', 'MAYBE_CreateConfirmationText', function() {
  var self = this;
  var custodianEmail = 'foo@example.com';

  // Checks the strings in the confirmation dialog. If |expectedNameText| is
  // given, it should be present in the dialog's textContent; otherwise the name
  // is expected. If |expectedNameHtml| is given, it should be present in the
  // dialog's innerHTML; otherwise the expected text is expected in the HTML
  // too.
  function checkDialog(name, expectedNameText, expectedNameHtml) {
    var expectedText = expectedNameText || name;
    var expectedHtml = expectedNameHtml || expectedText;

    // Configure the test profile and show the confirmation dialog.
    var testProfile = self.testProfileInfo_(true);
    testProfile.name = name;
    CreateProfileOverlay.onSuccess(testProfile);
    assertEquals('supervisedUserCreateConfirm',
                 PageManager.getTopmostVisiblePage().name);

    // Check for the presence of the name and email in the UI, without depending
    // on the details of the messages.
    assertNotEquals(-1,
        $('supervised-user-created-title').textContent.indexOf(expectedText));
    assertNotEquals(-1,
        $('supervised-user-created-switch').textContent.indexOf(expectedText));
    var message = $('supervised-user-created-text');
    assertNotEquals(-1, message.textContent.indexOf(expectedText));
    assertNotEquals(-1, message.textContent.indexOf(custodianEmail));

    // The name should be properly HTML-escaped.
    assertNotEquals(-1, message.innerHTML.indexOf(expectedHtml));

    PageManager.closeOverlay();
    assertEquals('settings', PageManager.getTopmostVisiblePage().name, name);
  }

  // Show and configure the create-profile dialog.
  PageManager.showPageByName('createProfile');
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  assertEquals('createProfile', PageManager.getTopmostVisiblePage().name);

  checkDialog('OneWord');
  checkDialog('Multiple Words');
  checkDialog('It\'s "<HTML> injection" & more!',
              'It\'s "<HTML> injection" & more!',
              // The innerHTML getter doesn't escape quotation marks,
              // independent of whether they were escaped in the setter.
              'It\'s "&lt;HTML&gt; injection" &amp; more!');

  // Test elision. MAX_LENGTH = 50, minus 1 for the ellipsis.
  var name49Characters = '0123456789012345678901234567890123456789012345678';
  var name50Characters = name49Characters + '9';
  var name51Characters = name50Characters + '0';
  var name60Characters = name51Characters + '123456789';
  checkDialog(name49Characters, name49Characters);
  checkDialog(name50Characters, name50Characters);
  checkDialog(name51Characters, name49Characters + '\u2026');
  checkDialog(name60Characters, name49Characters + '\u2026');

  // Test both elision and HTML escaping. The allowed string length is the
  // visible length, not the length including the entity names.
  name49Characters = name49Characters.replace('0', '&').replace('1', '>');
  name60Characters = name60Characters.replace('0', '&').replace('1', '>');
  var escaped = name49Characters.replace('&', '&amp;').replace('>', '&gt;');
  checkDialog(
      name60Characters, name49Characters + '\u2026', escaped + '\u2026');
});

// An additional warning should be shown when deleting a supervised user.
TEST_F('ManageProfileUITest', 'DeleteSupervisedUserWarning', function() {
  var addendum = $('delete-supervised-profile-addendum');

  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(true));
  assertFalse(addendum.hidden);

  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertTrue(addendum.hidden);
});

// The policy prohibiting supervised users should update the UI dynamically.
TEST_F('ManageProfileUITest', 'PolicyDynamicRefresh', function() {
  ManageProfileOverlay.getInstance().initializePage();

  var custodianEmail = 'chrome.playpen.test@gmail.com';
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateSupervisedUsersAllowed(true);
  var checkbox = $('create-profile-supervised');
  var signInPromo = $('create-profile-supervised-not-signed-in');
  var signInLink = $('create-profile-supervised-sign-in-link');
  var indicator = $('create-profile-supervised-indicator');

  assertFalse(checkbox.disabled, 'allowed and signed in');
  assertTrue(signInPromo.hidden, 'allowed and signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               'allowed and signed in');

  CreateProfileOverlay.updateSignedInStatus('');
  CreateProfileOverlay.updateSupervisedUsersAllowed(true);
  assertTrue(checkbox.disabled, 'allowed, not signed in');
  assertFalse(signInPromo.hidden, 'allowed, not signed in');
  assertTrue(signInLink.enabled, 'allowed, not signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               'allowed, not signed in');

  CreateProfileOverlay.updateSignedInStatus('');
  CreateProfileOverlay.updateSupervisedUsersAllowed(false);
  assertTrue(checkbox.disabled, 'disallowed, not signed in');
  assertFalse(signInPromo.hidden, 'disallowed, not signed in');
  assertFalse(signInLink.enabled, 'disallowed, not signed in');
  assertEquals('inline-block', window.getComputedStyle(indicator, null).display,
               'disallowed, not signed in');
  assertEquals('policy', indicator.getAttribute('controlled-by'));

  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateSupervisedUsersAllowed(false);
  assertTrue(checkbox.disabled, 'disallowed, signed in');
  assertTrue(signInPromo.hidden, 'disallowed, signed in');
  assertEquals('inline-block', window.getComputedStyle(indicator, null).display,
               'disallowed, signed in');
  assertEquals('policy', indicator.getAttribute('controlled-by'));

  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateSupervisedUsersAllowed(true);
  assertFalse(checkbox.disabled, 're-allowed and signed in');
  assertTrue(signInPromo.hidden, 're-allowed and signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               're-allowed and signed in');
});

// The supervised user checkbox should correctly update its state during profile
// creation and afterwards.
TEST_F('ManageProfileUITest', 'CreateInProgress', function() {
  ManageProfileOverlay.getInstance().initializePage();

  var custodianEmail = 'chrome.playpen.test@gmail.com';
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateSupervisedUsersAllowed(true);
  var checkbox = $('create-profile-supervised');
  var signInPromo = $('create-profile-supervised-not-signed-in');
  var indicator = $('create-profile-supervised-indicator');

  assertFalse(checkbox.disabled, 'allowed and signed in');
  assertTrue(signInPromo.hidden, 'allowed and signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               'allowed and signed in');
  assertFalse(indicator.hasAttribute('controlled-by'));

  CreateProfileOverlay.updateCreateInProgress(true);
  assertTrue(checkbox.disabled, 'creation in progress');

  // A no-op update to the sign-in status should not change the UI.
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateSupervisedUsersAllowed(true);
  assertTrue(checkbox.disabled, 'creation in progress');

  CreateProfileOverlay.updateCreateInProgress(false);
  assertFalse(checkbox.disabled, 'creation finished');
});

// Supervised users shouldn't be able to open the delete or create dialogs.
TEST_F('ManageProfileUITest', 'SupervisedShowDeleteAndCreate', function() {
  this.setProfileSupervised_(false, 'create');

  ManageProfileOverlay.showCreateDialog();
  assertEquals('createProfile', PageManager.getTopmostVisiblePage().name);
  PageManager.closeOverlay();
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);
  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertEquals('manageProfile', PageManager.getTopmostVisiblePage().name);
  assertFalse($('manage-profile-overlay-delete').hidden);
  PageManager.closeOverlay();
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);

  this.setProfileSupervised_(true, 'create');
  ManageProfileOverlay.showCreateDialog();
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);
  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);
});

// Only non-supervised users should be able to delete profiles.
TEST_F('ManageProfileUITest', 'SupervisedDelete', function() {
  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertEquals('manageProfile', PageManager.getTopmostVisiblePage().name);
  assertFalse($('manage-profile-overlay-delete').hidden);

  // Clicks the "Delete" button, after overriding chrome.send to record what
  // messages were sent.
  function clickAndListen() {
    var originalChromeSend = chrome.send;
    var chromeSendMessages = [];
    chrome.send = function(message) {
      chromeSendMessages.push(message);
    };
    $('delete-profile-ok').onclick();
    // Restore the original function so the test framework can use it.
    chrome.send = originalChromeSend;
    return chromeSendMessages;
  }

  this.setProfileSupervised_(false, 'manage');
  var messages = clickAndListen();
  assertEquals(1, messages.length);
  assertEquals('deleteProfile', messages[0]);
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);

  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  this.setProfileSupervised_(true, 'manage');
  messages = clickAndListen();
  assertEquals(0, messages.length);
  assertEquals('settings', PageManager.getTopmostVisiblePage().name);
});

// Selecting a different avatar image should update the suggested profile name.
TEST_F('ManageProfileUITest', 'Create_NameUpdateOnAvatarSelected', function() {
  var mode = 'create';
  this.initDefaultProfiles_(mode);

  var gridEl = $(mode + '-profile-icon-grid');
  var nameEl = $(mode + '-profile-name');

  // Select another icon and check that the profile name was updated.
  assertNotEquals(gridEl.selectedItem, this.defaultIconURLs[0]);
  gridEl.selectedItem = this.defaultIconURLs[0];
  expectEquals(this.defaultNames[0], nameEl.value);

  // Select icon without an associated name; the profile name shouldn't change.
  var oldName = nameEl.value;
  assertEquals('', this.defaultNames[2]);
  gridEl.selectedItem = this.defaultIconURLs[2];
  expectEquals(oldName, nameEl.value);

  // Select another icon with a name and check that the name is updated again.
  assertNotEquals('', this.defaultNames[1]);
  gridEl.selectedItem = this.defaultIconURLs[1];
  expectEquals(this.defaultNames[1], nameEl.value);

  PageManager.closeOverlay();
});

// After the user edited the profile name, selecting a different avatar image
// should not update the suggested name anymore.
TEST_F('ManageProfileUITest', 'Create_NoNameUpdateOnAvatarSelectedAfterEdit',
    function() {
  var mode = 'create';
  this.initDefaultProfiles_(mode);

  var gridEl = $(mode + '-profile-icon-grid');
  var nameEl = $(mode + '-profile-name');

  // After the user manually entered a name, it should not be changed anymore
  // (even if the entered name is another default name).
  nameEl.value = this.defaultNames[3];
  nameEl.oninput();
  gridEl.selectedItem = this.defaultIconURLs[0];
  expectEquals(this.defaultNames[3], nameEl.value);

  PageManager.closeOverlay();
});

// After the user edited the profile name, selecting a different avatar image
// should not update the suggested name anymore even if the original suggestion
// is entered again.
TEST_F('ManageProfileUITest', 'Create_NoNameUpdateOnAvatarSelectedAfterRevert',
    function() {
  var mode = 'create';
  this.initDefaultProfiles_(mode);

  var gridEl = $(mode + '-profile-icon-grid');
  var nameEl = $(mode + '-profile-name');

  // After the user manually entered a name, it should not be changed anymore,
  // even if the user then reverts to the original suggestion.
  var oldName = nameEl.value;
  nameEl.value = 'Custom Name';
  nameEl.oninput();
  nameEl.value = oldName;
  nameEl.oninput();
  // Now select another avatar and check that the name remained the same.
  assertNotEquals(gridEl.selectedItem, this.defaultIconURLs[0]);
  gridEl.selectedItem = this.defaultIconURLs[0];
  expectEquals(oldName, nameEl.value);

  PageManager.closeOverlay();
});

// In the manage dialog, the name should never be updated on avatar selection.
TEST_F('ManageProfileUITest', 'Manage_NoNameUpdateOnAvatarSelected',
    function() {
  var mode = 'manage';
  this.setProfileSupervised_(false, mode);
  PageManager.showPageByName(mode + 'Profile');

  var testProfile = this.testProfileInfo_(false);
  var iconURLs = [testProfile.iconURL, '/some/path', '/another/path'];
  var names = [testProfile.name, 'Some Name', ''];
  ManageProfileOverlay.receiveDefaultProfileIconsAndNames(
     mode, iconURLs, names);

  var gridEl = $(mode + '-profile-icon-grid');
  var nameEl = $(mode + '-profile-name');

  // Select another icon and check if the profile name was updated.
  var oldName = nameEl.value;
  gridEl.selectedItem = iconURLs[1];
  expectEquals(oldName, nameEl.value);

  PageManager.closeOverlay();
});

GEN('#endif  // OS_CHROMEOS');
