// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  if (document.location != 'chrome://settings-frame/options_settings_app.html')
    return;

  OptionsPage.setIsSettingsApp();

  // Override the offset in the options page.
  OptionsPage.setHorizontalOffset(38);

  document.addEventListener('DOMContentLoaded', function() {
    // Hide everything by default.
    var sections = document.querySelectorAll('section');
    for (var i = 0; i < sections.length; i++)
      sections[i].hidden = true;

    var whitelistedSections = [
      'advanced-settings',
      'handlers-section',
      'languages-section',
      'media-galleries-section',
      'network-section',
      'notifications-section',
      'privacy-section',
      'sync-section',
      'sync-users-section'
    ];

    for (var i = 0; i < whitelistedSections.length; i++)
      $(whitelistedSections[i]).hidden = false;

    // Hide irrelevant parts of privacy section.
    var hiddenPrivacyNodeList = document.querySelectorAll(
        '#privacy-section > div > .checkbox');
    for (var i = 0; i < hiddenPrivacyNodeList.length; i++)
      hiddenPrivacyNodeList[i].hidden = true;

    document.querySelector(
        '#privacy-section > div > #privacy-explanation').
            hidden = true;

    // Hide Import bookmarks and settings button.
    $('import-data').hidden = true;

    // Hide create / edit / delete profile buttons.
    $('profiles-create').hidden = true;
    $('profiles-delete').hidden = true;
    $('profiles-manage').hidden = true;

    // Remove the 'X'es on profiles in the profile list.
    $('profiles-list').canDeleteItems = false;
  });

  loadTimeData.overrideValues(loadTimeData.getValue('settingsApp'));
}());
