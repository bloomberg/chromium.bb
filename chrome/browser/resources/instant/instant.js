// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redefine '$' here rather than including 'cr.js', since this is
// the only function needed.  This allows this file to be loaded
// in a browser directly for layout and some testing purposes.
var $ = function(id) { return document.getElementById(id); };

/**
 * WebUI for configuring instant.* preference values used by
 * Chrome's instant search system.
 */
var instantConfig = (function() {
  'use strict';

  /** List of fields used to dynamically build form. **/
  var FIELDS = [
    {
      key: 'instant.animation_scale_factor',
      label: 'Slow down animations by a factor of',
      type: 'float',
      units: 'no units, range 1 to 10',
      default: 1
    },
    {
      key: 'instant.experimental_zero_suggest_url_prefix',
      label: 'Prefix URL for the (experimental) ZeroSuggest provider',
      type: 'string',
      units: '',
      default: ''
    }
  ];

  /**
   * Returns a DOM element of the given type and class name.
   */
  function createElementWithClass(elementType, className) {
    var element = document.createElement(elementType);
    element.className = className;
    return element;
  }

  /**
   * Dynamically builds web-form based on FIELDS list.
   * @return {string} The form's HTML.
   */
  function buildForm() {
    var buf = [];

    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];

      var row = createElementWithClass('div', 'row');
      row.id = '';

      var label = createElementWithClass('label', 'row-label');
      label.setAttribute('for', field.key);
      label.textContent = field.label;
      row.appendChild(label);

      var input = createElementWithClass('input', 'row-input');
      input.id = field.key;
      input.title = "Default Value: " + field.default;
      if (field.type == 'float') {
        input.size = 3;
        input.type = 'number';
        input.min = field.min || 0;
        if (field.max) input.max = field.max;
        if (field.step) input.step = field.step;
      } else {
        input.size = 40;
      }
      row.appendChild(input);

      var units = createElementWithClass('div', 'row-units');
      if (field.units)
        units.innerHTML = field.units;
      row.appendChild(units);

      $('instant-form').appendChild(row);
    }
  }

  /**
   * Initialize the form by adding 'onChange' listeners to all fields.
   */
  function initForm() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      $(field.key).onchange = (function(key) {
        setPreferenceValue(key, $(key).value);
      }).bind(null, field.key);
    }
  }

  /**
   * Request a preference setting's value.
   * This method is asynchronous; the result is provided by a call to
   * getPreferenceValueResult.
   * @param {string} prefName The name of the preference value being requested.
   */
  function getPreferenceValue(prefName) {
    chrome.send('getPreferenceValue', [prefName]);
  }

  /**
   * Handle callback from call to getPreferenceValue.
   * @param {string} prefName The name of the requested preference value.
   * @param {value} value The current value associated with prefName.
   */
  function getPreferenceValueResult(prefName, value) {
    $(prefName).value = value;
  }

  /**
   * Set a preference setting's value.
   * @param {string} prefName The name of the preference value being set.
   * @param {value} value The value to be associated with prefName.
   */
  function setPreferenceValue(prefName, value) {
    chrome.send('setPreferenceValue', [prefName, value]);
  }

  /**
   * Handle processing of "Reset" button.
   * Causes off form values to be updated based on current preference values.
   */
  function onReset() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      $(field.key).value = field.default;
      setPreferenceValue(field.key, field.default);
    }
    return false;
  }

  /**
   * Saves data back into Chrome preferences.
   */
  function onSave() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      var value = $(field.key).value;
      setPreferenceValue(
          field.key, (field.type == 'float') ? parseFloat(value) : value);
    }
    return false;
  }


  function loadForm() {
    for (var i = 0; i < FIELDS.length; i++)
      getPreferenceValue(FIELDS[i].key);
  }

  /**
   * Build and initialize the configuration form.
   */
  function initialize() {
    buildForm();
    loadForm();
    initForm();

    $('reset-button').onclick = onReset.bind(this);
    $('save-button').onclick = onSave.bind(this);
  }

  return {
    initialize: initialize,
    getPreferenceValueResult: getPreferenceValueResult
  };
})();

document.addEventListener('DOMContentLoaded', instantConfig.initialize);
