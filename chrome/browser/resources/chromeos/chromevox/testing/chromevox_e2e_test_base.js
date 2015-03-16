// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
    'chrome/browser/resources/chromeos/chromevox/testing/common.js',
    'chrome/browser/resources/chromeos/chromevox/testing/callback_helper.js']);

/**
 * Base test fixture for ChromeVox end to end tests.
 *
 * These tests run against production ChromeVox inside of the extension's
 * background page context.
 * @constructor
 */
function ChromeVoxE2ETest() {
  this.callbackHelper_ = new CallbackHelper(this);
}

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
  base::Closure load_cb =
      base::Bind(&chromeos::AccessibilityManager::EnableSpokenFeedback,
          base::Unretained(chromeos::AccessibilityManager::Get()),
          true,
          ui::A11Y_NOTIFICATION_NONE);
  WaitForExtension(extension_misc::kChromeVoxExtensionId, load_cb);
    */});
  },

  /**
   * Launch a new tab, wait until tab status complete, then run callback.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function()} callback Called once the document is ready.
   */
  runWithLoadedTab: function(doc, callback) {
    this.launchNewTabWithDoc(doc, function(tab) {
      chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
        if (tabId == tab.id && changeInfo.status == 'complete') {
          callback(tabId);
        }
      });
    });
  },

  /**
   * Launches the given document in a new tab.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function()} opt_callback Called once the document is created.
   */
  runWithTab: function(doc, opt_callback) {
    var docString = TestUtils.extractHtmlFromCommentEncodedString(doc);
    var url = 'data:text/html,<!doctype html>' +
        docString +
        '<!-- chromevox_next_test -->';
    var createParams = {
      active: true,
      url: url
    };
    chrome.tabs.create(createParams, opt_callback);
  },

  /**
   * Send a key to the page.
   * @param {number} tabId Of the page.
   * @param {string} key Name of the key (e.g. Down).
   * @param {string} elementQueryString
   */
  sendKeyToElement: function(tabId, key, elementQueryString) {
    var code = TestUtils.extractHtmlFromCommentEncodedString(function() {/*!
      var target = document.body.querySelector('$1');
      target.focus();
      var evt = document.createEvent('KeyboardEvent');
      evt.initKeyboardEvent('keydown', true, true, window, '$0', 0, false,
          false, false, false);
      document.activeElement.dispatchEvent(evt);
    */}, [key, elementQueryString]);

    chrome.tabs.executeScript(tabId, {code: code});
  },

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture.
   * @return {Function}
   */
  newCallback: function(opt_callback) {
    return this.callbackHelper_.wrap(opt_callback);
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
