// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */
var policyDataformat = {
  'policies': [
    {
      'level': 'managed',
      'name': 'AllowXYZ',
      'set': true,
      'sourceType': 'Device',
      'status': 'ok',
      'value': true,
    },
  ],
  'anyPoliciesSet': true
};

cr.define('policies', function() {

  function Policy() {
  }

  cr.addSingletonGetter(Policy);

  Policy.prototype = {

    /**
     * True if none of the received policies are actually set, false otherwise.
     * @type {boolean}
     */
    noActivePolicies_: false,

    /**
     * The current search term for filtering of the policy table.
     * @type {string}
     * @private
     */
    searchTerm_: '',

    /**
     * Takes the |policyData| input argument which represents data about the
     * policies supported by the device/client and populates the html jstemplate
     * with that data. It expects an object structure like the above.
     * @param {Object} policyData Detailed info about policies
     */
    renderTemplate: function(policyData) {
      this.noActivePolicies_ = !policyData.anyPoliciesSet;

      // This is the javascript code that processes the template:
      var input = new JsEvalContext(policyData);
      var output = $('policiesTemplate');
      jstProcess(input, output);
    },

    /**
     * Filters the table of policies by name.
     * @param {string} term The search string
     */
    filterTable: function(term) {
      this.searchTerm_ = term.toLowerCase();
      var table = $('policy-table');
      var showUnsent = $('toggle-unsent-policies').checked;
      for (var r = 1; r < table.rows.length; r++) {
        var row = table.rows[r];

        // Don't change visibility of policies that aren't set if the checkbox
        // isn't checked.
        if (!showUnsent && row.className == 'policy-unset')
          continue;

        var nameCell = row.querySelector('.policy-name');
        var cellContents = nameCell.textContent;
        if (cellContents.toLowerCase().indexOf(this.searchTerm_) >= 0)
          row.style.display = 'table-row';
        else
          row.style.display = 'none';
      }
    },

    /**
     * Updates the visibility of the policies depending on the state of the
     * 'toggle-unsent-policies' checkbox.
     */
    updatePolicyVisibility: function() {
      if ($('toggle-unsent-policies').checked)
          $('policies').style.display = '';
      else if (this.noActivePolicies_)
          $('policies').style.display = 'none';

      var tableRows = document.getElementsByClassName('policy-unset');
      for (var i = 0; i < tableRows.length; i++) {
        if ($('toggle-unsent-policies').checked)
          tableRows[i].style.visibility = 'visible';
        else
          tableRows[i].style.visibility = 'hidden';
      }

      // Filter table again in case a search was active.
      this.filterTable(this.searchTerm_);
    }
  };

  /**
   * Asks the C++ PolicyUIHandler to get details about policies. The
   * PolicyDOMHandler should reply to returnPolicyData() (below).
   */
  Policy.requestPolicyData = function() {
    chrome.send('requestPolicyData');
  };

  /**
   * Called by the C++ PolicyUIHandler when it has the requested policy data.
   */
  Policy.returnPolicyData = function(policyData) {
    Policy.getInstance().renderTemplate(policyData);
  };

  /**
   * Determines whether a policy should be visible or not.
   * @param {policy} policy information in the format given by above the
   * PolicyDataFormat
   */
  Policy.shouldDisplayPolicy = function(policy) {
    return $('toggle-unsent-policies').checked || policy.set;
  };

  /**
   * Initializes the page and loads the list of policies.
   */
  Policy.initialize = function() {
    i18nTemplate.process(document, templateData);
    Policy.requestPolicyData();

    // Set HTML event handlers.
    $('toggle-unsent-policies').onchange = function(event) {
      Policy.getInstance().updatePolicyVisibility();
    };

    $('search-field').onsearch = function(event) {
      Policy.getInstance().filterTable(this.value);
    };
  };

  // Export
  return {
    Policy: Policy
  };
});

var Policy = policies.Policy;

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', policies.Policy.initialize);
