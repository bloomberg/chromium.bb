// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function() {
    // Test management (backed by a json file) api enums.

    // The enum should be declared on the API object.
    chrome.test.assertTrue(
        'LaunchType' in chrome.management,
        '"LaunchType" is not present on chrome.management.');
    // The object should have entries for each enum entry. Note that we don't
    // test all entries here because we don't want to update this test if the
    // management api changes.
    chrome.test.assertTrue(
        'OPEN_AS_REGULAR_TAB' in chrome.management.LaunchType,
        '"OPEN_AS_REGULAR_TAB" is not present on management.LaunchType');
    // The value of the enum should be its string value.
    chrome.test.assertEq(chrome.management.LaunchType.OPEN_AS_REGULAR_TAB,
                         'OPEN_AS_REGULAR_TAB');
    // There should be more than one value for the enum.
    chrome.test.assertTrue(
        Object.keys(chrome.management.LaunchType).length > 1);

    // Perform an analogous test for the notifications api (backed by an idl).
    chrome.test.assertTrue(
        'PermissionLevel' in chrome.notifications,
        '"PermissionLevel" is not present on chrome.notifications.');
    chrome.test.assertTrue(
        'granted' in chrome.notifications.PermissionLevel,
        '"granted" is not present on notifications.PermissionLevel');
    chrome.test.assertEq(chrome.notifications.PermissionLevel.granted,
                         'granted');
    chrome.test.assertTrue(
        Object.keys(chrome.notifications.PermissionLevel).length > 1);

    chrome.test.succeed();
  }
]);
