// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {testing.Test}
 */
function OptionsBrowsertestBase() {}

OptionsBrowsertestBase.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    var requiredOwnedAriaRoleMissingSelectors = [
      '#address-list',
      '#creditcard-list',
      '#home-page-overlay > .autocomplete-suggestions',
      '#language-options-list',
      '#manage-profile-icon-grid',
      '#create-profile-icon-grid',
      '#saved-passwords-list',
      '#password-exceptions-list',
      '#extension-keyword-list',
      '#startup-overlay > .autocomplete-suggestions',
      '#content-settings-exceptions-area > .content-area > *',
      '#cookies-list',
      '#handlers-list',
      '#ignored-handlers-list',
      '#supervised-user-list',
      '#select-avatar-grid',
      '#language-dictionary-overlay-word-list',
      // Selectors below only affect ChromeOS tests.
      '#bluetooth-unpaired-devices-list',
      '#ignored-host-list',
      '#remembered-network-list',
      '#bluetooth-paired-devices-list',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/559265
    this.accessibilityAuditConfig.ignoreSelectors(
        'requiredOwnedAriaRoleMissing',
        requiredOwnedAriaRoleMissingSelectors);

    var tabIndexGreaterThanZeroSelectors = [
      '#user-image-grid',
      '#discard-photo',
      '#take-photo',
      '#flip-photo',
      '#change-picture-overlay-confirm',
    ];

    // Enable when failure is resolved.
    // AX_FOCUS_03: http://crbug.com/560910
    this.accessibilityAuditConfig.ignoreSelectors(
        'tabIndexGreaterThanZero',
        tabIndexGreaterThanZeroSelectors);

    // Enable when audit has improved performance.
    // AX_HTML_02:
    // https://github.com/GoogleChrome/accessibility-developer-tools/issues/263
    this.accessibilityAuditConfig.auditRulesToIgnore.push(
        'duplicateId');

    // Enable when failure is resolved.
    // AX_ARIA_02: http://crbug.com/591547
    this.accessibilityAuditConfig.ignoreSelectors(
         'nonExistentAriaRelatedElement', '#input');

    // Enable when failure is resolved.
    // AX_ARIA_04: http://crbug.com/591550
    this.accessibilityAuditConfig.ignoreSelectors(
         'badAriaAttributeValue', '#input');

  },
};
