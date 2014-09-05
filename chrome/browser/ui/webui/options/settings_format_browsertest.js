// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for testing the formatting of settings pages.
 * @extends {testing.Test}
 * @constructor
 */
function SettingsFormatWebUITest() {}

/**
 * Map of rule exemptions grouped by test.
 * @const
 */
SettingsFormatWebUITest.Filters = {
  /**
   * Exemption for checkboxes that do not require an id or pref property.
   * Input methods use inputMethodId instead of id for unique identification.
   */
  'pref': ['language-options-input-method-template',
           'language-options-input-method-list']
};

/**
 * Collection of error messages.
 * @const
 */
SettingsFormatWebUITest.Messages = {
    MISSING_CHECK_WRAPPER: 'Element $1 should be enclosed in <div class="$2">',
    MISSING_ID_OR_PREF: 'Missing id or pref preoperty for checkbox $1.',
    MISSING_RADIO_BUTTON_NAME: 'Radio button $1 is missing the name property',
    MISSING_RADIO_BUTTON_VALUE: 'Radio button $1 is missing the value property',
};

SettingsFormatWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Navigate to browser settings.
   */
  browsePreload: 'chrome://settings-frame/',

  /**
   * List of errors generated during a test. Used instead of expect* functions
   * to suppress verbosity. The implementation of errorsToMessage in the
   * testing API generates a call stack for each error produced which greatly
   * reduces readability.
   * @type {Array.<string>}
   */
  errors: null,

  setUp: function() {
    this.errors = [];
  },

  tearDown: function() {
    assertTrue(this.errors.length == 0, '\n' + this.errors.join('\n'));
  },

  /**
   * Generates a failure message. During tear down of the test, the accumulation
   * of pending messages triggers a test failure.
   * @param {string} key Label of the message formatting string.
   * @param {!Element} element The element that triggered the failure.
   * @param {...string} args Additional arguments for formatting the message.
   */
  fail: function(key, element, args) {
    var subs = [this.getLabel(element)].concat(
        Array.prototype.slice.call(arguments, 2));
    var message = SettingsFormatWebUITest.Messages[key].replace(
        /\$\d/g,
        function(m) {
      return subs[m[1] - 1] || '$' + m[1];
    });
    assertFalse(/\$\d/.test(message), 'found unreplaced subs');
    this.errors.push(message);
  },

 /**
  * String for identifying a node within an error message.
  * @param {!Element} element The target element to identify.
  * @return {string} Name to facilitate tracking down the element.
  */
  getLabel: function(element) {
    if (element.id)
      return element.id;

    if (element.pref)
      return element.pref;

    if (element.name && element.value)
      return element.name + '-' + element.value;

    return this.getLabel(element.parentNode);
  },


  /**
   * Checks if the node is exempt from following the formatting rule.
   * @param {!Element} element The candidate element.
   * @param {Array.<string>} filters List of exemptions.
   * @return {boolean} True if the element is exempt.
   */
  isExempt: function(element, filters) {
    var target = this.getLabel(element);
    for (var i = 0; i < filters.length; i++) {
      if (filters[i] == target)
        return true;
    }
    return false;
  }
};

/**
 * Ensure that radio and checkbox buttons have consistent layout.
 */
TEST_F('SettingsFormatWebUITest', 'RadioCheckboxStyleCheck', function() {
  var settings = $('settings');
  assertTrue(settings != null, 'Unable to access settings');
  var query = 'input[type=checkbox], input[type=radio]';
  var elements = document.querySelectorAll(query);
  assertTrue(elements.length > 0);
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    if (!findAncestorByClass(element, element.type))
      this.fail('MISSING_CHECK_WRAPPER', element, element.type);
  }
});

/**
 * Each checkbox requires an id or pref property.
 */
TEST_F('SettingsFormatWebUITest', 'CheckboxIdOrPrefCheck', function() {
  var query =
      'input[type=checkbox]:not([pref]):not([id]):not(.spacer-checkbox)';
  var elements = document.querySelectorAll(query);
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    if (!this.isExempt(element, SettingsFormatWebUITest.Filters['pref']))
      this.fail('MISSING_ID_OR_PREF', element);
  }
});

/**
 * Each radio button requires name and value properties.
 */
TEST_F('SettingsFormatWebUITest', 'RadioButtonNameValueCheck', function() {
  var elements = document.querySelectorAll('input[type=radio]');
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    if (!element.name)
      this.fail('MISSING_RADIO_BUTTON_NAME', element);

    if (!element.getAttribute('value'))
      this.fail('MISSING_RADIO_BUTTON_VALUE', element);
  }
});
