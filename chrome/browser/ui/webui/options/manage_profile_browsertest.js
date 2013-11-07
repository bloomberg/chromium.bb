// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// None of these tests is relevant for Chrome OS.
GEN('#if !defined(OS_CHROMEOS)');

GEN('#include "base/command_line.h"');
GEN('#include "chrome/common/chrome_switches.h"');

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

  /** @override */
  testGenPreamble: function() {
    GEN('CommandLine::ForCurrentProcess()->' +
        'AppendSwitch(switches::kEnableManagedUsers);');
  },

  /**
   * Returns a test profile-info object with configurable "managed" status.
   * @param {boolean} managed If true, the test profile will be marked as
   *     managed.
   * @return {Object} A test profile-info object.
   */
  testProfileInfo_: function(managed) {
    return {
      name: 'Test Profile',
      iconURL: 'chrome://path/to/icon/image',
      filePath: '/path/to/profile/data/on/disk',
      isCurrentProfile: true,
      isManaged: managed
    };
  },

  /**
   * Overrides WebUI methods that provide profile info, making them return a
   * test profile-info object.
   * @param {boolean} managed Whether the test profile should be marked managed.
   */
  setProfileManaged_: function(managed) {
    // Override the BrowserOptions method to return the fake info.
    BrowserOptions.getCurrentProfile = function() {
      return this.testProfileInfo_(managed);
    }.bind(this);
    // Set the profile info in the overlay.
    ManageProfileOverlay.setProfileInfo(this.testProfileInfo_(managed),
                                        'manage');
  },
};

TEST_F('ManageProfileUITest', 'NewProfileDefaultsFocus', function() {
  var self = this;

  function checkFocus(pageName, expectedFocus, initialFocus) {
    OptionsPage.showPageByName(pageName);
    initialFocus.focus();
    expectEquals(initialFocus, document.activeElement, pageName);

    ManageProfileOverlay.receiveNewProfileDefaults(
        self.testProfileInfo_(false));
    expectEquals(expectedFocus, document.activeElement, pageName);
    OptionsPage.closeOverlay();
  }

  // Receiving new profile defaults sets focus to the name field if the create
  // overlay is open, and should not change focus at all otherwise.
  checkFocus('manageProfile',
             $('manage-profile-cancel'),
             $('manage-profile-cancel'));
  checkFocus('createProfile',
             $('create-profile-name'),
             $('create-profile-cancel'));
  checkFocus('managedUserLearnMore',
             $('managed-user-learn-more-done'),
             $('managed-user-learn-more-done'));
  checkFocus('managedUserLearnMore',
             document.querySelector('#managed-user-learn-more-text a'),
             document.querySelector('#managed-user-learn-more-text a'));
});

// The default options should be reset each time the creation overlay is shown.
TEST_F('ManageProfileUITest', 'DefaultCreateOptions', function() {
  OptionsPage.showPageByName('createProfile');
  var shortcutsAllowed = loadTimeData.getBoolean('profileShortcutsEnabled');
  var createShortcut = $('create-shortcut');
  var createManaged = $('create-profile-managed');
  assertEquals(shortcutsAllowed, createShortcut.checked);
  assertFalse(createManaged.checked);

  createShortcut.checked = !shortcutsAllowed;
  createManaged.checked = true;
  OptionsPage.closeOverlay();
  OptionsPage.showPageByName('createProfile');
  assertEquals(shortcutsAllowed, createShortcut.checked);
  assertFalse(createManaged.checked);
});

// Creating managed users should be disallowed when they are not enabled.
TEST_F('ManageProfileUITest', 'CreateManagedUserAllowed', function() {
  var container = $('create-profile-managed-container');

  ManageProfileOverlay.getInstance().initializePage();
  assertFalse(container.hidden);

  loadTimeData.overrideValues({'managedUsersEnabled': false});
  ManageProfileOverlay.getInstance().initializePage();
  assertTrue(container.hidden);
});

// The checkbox label should change depending on whether the user is signed in.
TEST_F('ManageProfileUITest', 'CreateManagedUserText', function() {
  var signedInText =  $('create-profile-managed-signed-in');
  var notSignedInText = $('create-profile-managed-not-signed-in');

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
  assertFalse($('create-profile-managed').checked);
  assertTrue($('create-profile-managed').disabled);
});

// Managed users should not be able to edit their profile names.
TEST_F('ManageProfileUITest', 'EditManagedUserNameAllowed', function() {
  var nameField = $('manage-profile-name');

  this.setProfileManaged_(false);
  ManageProfileOverlay.showManageDialog();
  assertFalse(nameField.disabled);

  this.setProfileManaged_(true);
  ManageProfileOverlay.showManageDialog();
  assertTrue(nameField.disabled);
});

// Setting profile information should allow the confirmation to be shown.
TEST_F('ManageProfileUITest', 'ShowCreateConfirmation', function() {
  var testProfile = this.testProfileInfo_(true);
  testProfile.custodianEmail = 'foo@bar.example.com';
  ManagedUserCreateConfirmOverlay.setProfileInfo(testProfile);
  assertTrue(ManagedUserCreateConfirmOverlay.getInstance().canShowPage());
  OptionsPage.showPageByName('managedUserCreateConfirm', false);
  assertEquals('managedUserCreateConfirm',
               OptionsPage.getTopmostVisiblePage().name);
});

// Trying to show a confirmation dialog with no profile information should fall
// back to the default (main) settings page.
TEST_F('ManageProfileUITest', 'NoEmptyConfirmation', function() {
  assertEquals('manageProfile', OptionsPage.getTopmostVisiblePage().name);
  assertFalse(ManagedUserCreateConfirmOverlay.getInstance().canShowPage());
  OptionsPage.showPageByName('managedUserCreateConfirm', true);
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);
});

// A confirmation dialog should be shown after creating a new managed user.
TEST_F('ManageProfileUITest', 'ShowCreateConfirmationOnSuccess', function() {
  OptionsPage.showPageByName('createProfile');
  assertEquals('createProfile', OptionsPage.getTopmostVisiblePage().name);
  CreateProfileOverlay.onSuccess(this.testProfileInfo_(false));
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);

  OptionsPage.showPageByName('createProfile');
  assertEquals('createProfile', OptionsPage.getTopmostVisiblePage().name);
  CreateProfileOverlay.onSuccess(this.testProfileInfo_(true));
  assertEquals('managedUserCreateConfirm',
               OptionsPage.getTopmostVisiblePage().name);
});

// An error should be shown if creating a new managed user fails.
TEST_F('ManageProfileUITest', 'NoCreateConfirmationOnError', function() {
  OptionsPage.showPageByName('createProfile');
  assertEquals('createProfile', OptionsPage.getTopmostVisiblePage().name);
  var errorBubble = $('create-profile-error-bubble');
  assertTrue(errorBubble.hidden);

  CreateProfileOverlay.onError('An Error Message!');
  assertEquals('createProfile', OptionsPage.getTopmostVisiblePage().name);
  assertFalse(errorBubble.hidden);
});

// The name and email sould be inserted into the confirmation dialog.
TEST_F('ManageProfileUITest', 'CreateConfirmationText', function () {
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
    assertEquals('managedUserCreateConfirm',
                 OptionsPage.getTopmostVisiblePage().name);

    // Check for the presence of the name and email in the UI, without depending
    // on the details of the messsages.
    assertNotEquals(-1,
        $('managed-user-created-title').textContent.indexOf(expectedText));
    assertNotEquals(-1,
        $('managed-user-created-switch').textContent.indexOf(expectedText));
    var message = $('managed-user-created-text');
    assertNotEquals(-1, message.textContent.indexOf(expectedText));
    assertNotEquals(-1, message.textContent.indexOf(custodianEmail));

    // The name should be properly HTML-escaped.
    assertNotEquals(-1, message.innerHTML.indexOf(expectedHtml));

    OptionsPage.closeOverlay();
    assertEquals('settings', OptionsPage.getTopmostVisiblePage().name, name);
  }

  // Show and configure the create-profile dialog.
  OptionsPage.showPageByName('createProfile');
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  assertEquals('createProfile', OptionsPage.getTopmostVisiblePage().name);

  checkDialog('OneWord');
  checkDialog('Multiple Words');
  checkDialog('It\'s "<HTML> injection" & more!',
              'It\'s "<HTML> injection" & more!',
              // The innerHTML getter doesn't escape quotation marks,
              // independent of whether they were escaped in the setter.
              'It\'s "&lt;HTML&gt; injection" &amp; more!');

  // Test elision. MAX_LENGTH = 50, minus 3 for the ellipsis.
  var name47Characters = '01234567890123456789012345678901234567890123456';
  var name60Characters = name47Characters + '0123456789012';
  checkDialog(name60Characters, name47Characters + '...');

  // Test both elision and HTML escaping. The allowed string length is the
  // visible length, not the length including the entity names.
  name47Characters = name47Characters.replace('0', '&').replace('1', '>');
  name60Characters = name60Characters.replace('0', '&').replace('1', '>');
  var escaped = name47Characters.replace('&', '&amp;').replace('>', '&gt;');
  checkDialog(name60Characters, name47Characters + '...', escaped + '...');
});

// An additional warning should be shown when deleting a managed user.
TEST_F('ManageProfileUITest', 'DeleteManagedUserWarning', function() {
  var addendum = $('delete-managed-profile-addendum');

  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(true));
  assertFalse(addendum.hidden);

  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertTrue(addendum.hidden);
});

// The policy prohibiting managed users should update the UI dynamically.
TEST_F('ManageProfileUITest', 'PolicyDynamicRefresh', function() {
  ManageProfileOverlay.getInstance().initializePage();

  var custodianEmail = 'chrome.playpen.test@gmail.com';
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateManagedUsersAllowed(true);
  var checkbox = $('create-profile-managed');
  var link = $('create-profile-managed-not-signed-in-link');
  var indicator = $('create-profile-managed-indicator');

  assertFalse(checkbox.disabled, 'allowed and signed in');
  assertFalse(link.hidden, 'allowed and signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               'allowed and signed in');

  CreateProfileOverlay.updateSignedInStatus('');
  CreateProfileOverlay.updateManagedUsersAllowed(true);
  assertTrue(checkbox.disabled, 'allowed, not signed in');
  assertFalse(link.hidden, 'allowed, not signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               'allowed, not signed in');

  CreateProfileOverlay.updateSignedInStatus('');
  CreateProfileOverlay.updateManagedUsersAllowed(false);
  assertTrue(checkbox.disabled, 'disallowed, not signed in');
  assertTrue(link.hidden, 'disallowed, not signed in');
  assertEquals('inline-block', window.getComputedStyle(indicator, null).display,
               'disallowed, not signed in');
  assertEquals('policy', indicator.getAttribute('controlled-by'));

  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateManagedUsersAllowed(false);
  assertTrue(checkbox.disabled, 'disallowed, signed in');
  assertTrue(link.hidden, 'disallowed, signed in');
  assertEquals('inline-block', window.getComputedStyle(indicator, null).display,
               'disallowed, signed in');
  assertEquals('policy', indicator.getAttribute('controlled-by'));

  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateManagedUsersAllowed(true);
  assertFalse(checkbox.disabled, 're-allowed and signed in');
  assertFalse(link.hidden, 're-allowed and signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               're-allowed and signed in');
});

// The managed user checkbox should correctly update its state during profile
// creation and afterwards.
TEST_F('ManageProfileUITest', 'CreateInProgress', function() {
  ManageProfileOverlay.getInstance().initializePage();

  var custodianEmail = 'chrome.playpen.test@gmail.com';
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateManagedUsersAllowed(true);
  var checkbox = $('create-profile-managed');
  var link = $('create-profile-managed-not-signed-in-link');
  var indicator = $('create-profile-managed-indicator');

  assertFalse(checkbox.disabled, 'allowed and signed in');
  assertFalse(link.hidden, 'allowed and signed in');
  assertEquals('none', window.getComputedStyle(indicator, null).display,
               'allowed and signed in');
  assertFalse(indicator.hasAttribute('controlled-by'));

  CreateProfileOverlay.updateCreateInProgress(true);
  assertTrue(checkbox.disabled, 'creation in progress');

  // A no-op update to the sign-in status should not change the UI.
  CreateProfileOverlay.updateSignedInStatus(custodianEmail);
  CreateProfileOverlay.updateManagedUsersAllowed(true);
  assertTrue(checkbox.disabled, 'creation in progress');

  CreateProfileOverlay.updateCreateInProgress(false);
  assertFalse(checkbox.disabled, 'creation finished');
});

// Managed users shouldn't be able to open the delete or create dialogs.
TEST_F('ManageProfileUITest', 'ManagedShowDeleteAndCreate', function() {
  this.setProfileManaged_(false);

  ManageProfileOverlay.showCreateDialog();
  assertEquals('createProfile', OptionsPage.getTopmostVisiblePage().name);
  OptionsPage.closeOverlay();
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);
  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertEquals('manageProfile', OptionsPage.getTopmostVisiblePage().name);
  assertFalse($('manage-profile-overlay-delete').hidden);
  OptionsPage.closeOverlay();
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);

  this.setProfileManaged_(true);
  ManageProfileOverlay.showCreateDialog();
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);
  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);
});

// Only non-managed users should be able to delete profiles.
TEST_F('ManageProfileUITest', 'ManagedDelete', function() {
  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  assertEquals('manageProfile', OptionsPage.getTopmostVisiblePage().name);
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

  this.setProfileManaged_(false);
  var messages = clickAndListen();
  assertEquals(1, messages.length);
  assertEquals('deleteProfile', messages[0]);
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);

  ManageProfileOverlay.showDeleteDialog(this.testProfileInfo_(false));
  this.setProfileManaged_(true);
  messages = clickAndListen();
  assertEquals(0, messages.length);
  assertEquals('settings', OptionsPage.getTopmostVisiblePage().name);
});

GEN('#endif  // OS_CHROMEOS');
