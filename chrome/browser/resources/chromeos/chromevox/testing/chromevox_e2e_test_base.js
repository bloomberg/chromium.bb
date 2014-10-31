// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
    'chrome/browser/resources/chromeos/chromevox/testing/common.js']);

/**
 * Base test fixture for ChromeVox end to end tests.
 *
 * These tests run against production ChromeVox inside of the extension's
 * background page context.
 * @constructor
 */
function ChromeVoxE2ETest() {}

ChromeVoxE2ETest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * @override
   * No UI in the background context.
   */
  runAccessibilityChecks: false,

  /** @override */
  isAsync: true,

  /** @override */
  browsePreload: null,

  /** @override */
  testGenCppIncludes: function() {
    GEN_BLOCK(function() {/*!
#include "ash/accessibility_delegate.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/common/extensions/extension_constants.h"
    */});
  },

  /** @override */
  testGenPreamble: function() {
    GEN_BLOCK(function() {/*!
  if (chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
    chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(false,
        ui::A11Y_NOTIFICATION_NONE);
  }

  base::Closure load_cb =
      base::Bind(&chromeos::AccessibilityManager::EnableSpokenFeedback,
          base::Unretained(chromeos::AccessibilityManager::Get()),
          true,
          ui::A11Y_NOTIFICATION_NONE);
  WaitForExtension(extension_misc::kChromeVoxExtensionId, load_cb);
    */});
  },

  /**
   * Run a test with the specified HTML snippet loaded.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function()} callback Called once the document is ready.
   */
  runWithDocument: function(doc, callback) {
    var docString = TestUtils.extractHtmlFromCommentEncodedString(doc);
    var url = 'data:text/html,<!doctype html>' +
        docString +
        '<!-- chromevox_next_test -->';
    var createParams = {
      active: true,
      url: url
    };
    chrome.tabs.create(createParams, function(tab) {
      chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
        if (tabId == tab.id && changeInfo.status == 'complete') {
          callback();
        }
      });
    });
  }
};

/**
 * Similar to |TEST_F|. Generates a test for the given |testFixture|,
 * |testName|, and |testFunction|.
 * Used this variant when an |isAsync| fixture wants to temporarily mix in an
 * sync test.
 * @param {string} testFixture Fixture name.
 * @param {string} testName Test name.
 * @param {function} testFunction The test impl.
 */
function SYNC_TEST_F(testFixture, testName, testFunction) {
  var wrappedTestFunction = function() {
    testFunction.call(this);
    testDone([true, '']);
  };
  TEST_F(testFixture, testName, wrappedTestFunction);
}
