// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Override some methods of policy.Page.

/** @override */
policy.Page.setPolicyValues = function(values) {
  var page = this.getInstance();
  var table = page.policyTables['chrome'];
  table.setPolicyValues(values.chromePolicies || {});
  if (values.hasOwnProperty('extensionPolicies')) {
    for (var extensionId in values.extensionPolicies) {
      table = page.policyTables['extension-' + extensionId];
      if (table) {
        table.setPolicyValues(values.extensionPolicies[extensionId]);
      }
    }
  } else {
    for (var extension in page.policyTables) {
      if (extension == 'chrome') {
        continue;
      }
      table = page.policyTables[extension];
      table.setPolicyValues({});
    }
  }
};

/** @override */
policy.Page.prototype.initialize = function() {
  cr.ui.FocusOutlineManager.forDocument(document);

  this.mainSection = $('main-section');
  this.policyTables = {};

  // Place the initial focus on the session choice input field.
  $('session-name-field').select();

  $('filter').onsearch = (event) => {
    for (policyTable in this.policyTables) {
      this.policyTables[policyTable].setFilterPattern($('filter').value);
    }
  };

  $('session-choice').onsubmit = () => {
    var session = $('session-name-field').value;
    chrome.send('loadSession', [session]);
    $('session-name-field').value = '';
    // Return false in order to prevent the browser from reloading the whole
    // page.
    return false;
  };

  $('show-unset').onchange = () => {
    for (policyTable in this.policyTables) {
      this.policyTables[policyTable].filter();
    }
  };

  // Notify the browser that the page has loaded, causing it to send the
  // list of all known policies and the values from the default session.
  chrome.send('initialized');
};

/**
 * Extracts current policy values to send to backend for logging.
 * @return {Object} The dictionary containing policy values.
 */
policy.Page.prototype.getDictionary = function() {
  var result = {chromePolicies: {}, extensionPolicies: {}};
  for (var id in this.policyTables) {
    if (id == 'chrome') {
      result.chromePolicies = this.policyTables[id].getDictionary();
    } else {
      const PREFIX_LENGTH = 'extension-'.length;
      var extensionId = id.substr(PREFIX_LENGTH);
      result.extensionPolicies[extensionId] =
          this.policyTables[id].getDictionary();
    }
  }
  return result;
};

// Specify necessary columns.
policy.Page.prototype.tableHeadings = ['Name', 'Value', 'Status'];

// Override policy.Policy methods.

/** @override */
policy.Policy.prototype.decorate = function() {
  this.updateToggleExpandedValueText_();
  this.querySelector('.edit-button')
      .addEventListener('click', this.onValueEditing_.bind(this));
  this.querySelector('.value-edit-form').onsubmit =
      this.submitEditedValue_.bind(this);
  this.querySelector('.toggle-expanded-value')
      .addEventListener('click', this.toggleExpandedValue_.bind(this));
};

/** @override */
policy.Policy.prototype.initialize = function(name, value, unknown) {
  this.name = name;
  this.unset = !value;
  this.unknown = unknown;
  this.querySelector('.name').textContent = name;
  if (value) {
    this.setValue_(value.value);
  }
  this.setStatus_(value);
};

/**
 * Set the status column.
 * @param {Object} value Dictionary with information about the policy value.
 * @private
 */
policy.Policy.prototype.setStatus_ = function(value) {
  var status;
  if (this.unknown) {
    status = loadTimeData.getString('unknown');
  } else if (!value) {
    status = loadTimeData.getString('unset');
  } else if (value.error) {
    status = value.error;
  } else {
    status = loadTimeData.getString('ok');
  }
  this.querySelector('.status').textContent = status;
};

/**
 * Set the policy value.
 * @param {Object} value Dictionary with information about the policy value.
 * @private
 */
policy.Policy.prototype.setValue_ = function(value) {
  this.unset = !value;
  this.value = value;
  this.querySelector('.value').textContent = value || '';
  this.querySelector('.expanded-value').textContent = value || '';
  this.querySelector('.value-edit-field').value = value || '';
};

/** @override */
policy.Policy.prototype.getValueWidth_ = function(valueContainer) {
  return valueContainer.querySelector('.value').offsetWidth +
      valueContainer.querySelector('.edit-button').offsetWidth;
};

/**
 * Start editing value.
 * @private
 */
policy.Policy.prototype.onValueEditing_ = function() {
  this.classList.add('value-editing-on');
  this.classList.remove('has-overflowed-value');
  this.querySelector('.value-edit-field').select();
};

/**
 * Update the policy to its new edited value.
 * @private
 */
policy.Policy.prototype.submitEditedValue_ = function() {
  var newValue = this.querySelector('.value-edit-field').value;
  this.setValue_(newValue);
  this.setStatus_(newValue);
  this.classList.remove('value-editing-on');
  this.querySelector('.value-container').valueWidth = undefined;
  this.checkOverflow();
  var showUnset = $('show-unset').checked;
  this.hidden = this.unset && !showUnset ||
      this.name.toLowerCase().indexOf(this.parentNode.filterPattern_) == -1;
  chrome.send('updateSession', [policy.Page.getInstance().getDictionary()]);
  return false;
};

// Override policy.PolicyTable methods.

/**
 * Get policy values stored in this table.
 * @returns {Object} Dictionary with policy values.
 */
policy.PolicyTable.prototype.getDictionary = function() {
  var result = {};
  var policies = this.getElementsByTagName('tbody');
  for (var i = 0; i < policies.length; i++) {
    var policy = policies[i];
    if (policy.unset) {
      continue;
    }
    result[policy.name] = {value: policy.value};
  }
  return result;
};

// Add error showing function.

/**
 * Shows an error message to the user.
 * @param {String} message_name Identifier for the error message.
 */
policy.showErrorMessage = function(message_name) {
  // TODO(urusant): improve error showing.
  alert(loadTimeData.getString(message_name));
  console.log(loadTimeData.getString(message_name));
};

// Call the main inttialization function when the page finishes loading.
document.addEventListener(
    'DOMContentLoaded',
    policy.Page.getInstance().initialize.bind(policy.Page.getInstance()));
