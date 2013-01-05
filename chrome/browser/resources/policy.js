// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */
var policyDataFormat = {
  // Whether any of the policies in 'policies' have a value.
  'anyPoliciesSet': true,

  'policies': [
    {
      'level': 'managed',
      'name': 'AllowXYZ',
      'set': true,
      'scope': 'Machine',
      'status': 'ok',
      'value': true
    }
  ],
  'status': {
    'deviceFetchInterval': '8min',
    'deviceId': 'D2AC39A2-3C8FC-E2C0-E45D2DC3782C',
    'deviceLastFetchTime': '9:50 PM',
    'devicePolicyDomain': 'google.com',
    'deviceStatusMessage': 'OK',
    'displayDeviceStatus': true,
    'displayStatusSection': true,
    'displayUserStatus': true,
    'user': 'simo@google.com',
    'userFetchInterval': '8min',
    'userId': 'D2AC39A2-3C8FC-E2C0-E45D2DC3782C',
    'userLastFetchTime': '9:50 PM',
    'userStatusMessage': 'OK'
  }
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
     * Takes the |policyData| argument and populates the page with this data. It
     * expects an object structure like the policyDataFormat above.
     * @param {Object} policyData Detailed info about policies.
     */
    renderTemplate: function(policyData) {
      this.noActivePolicies_ = !policyData.anyPoliciesSet;

      if (this.noActivePolicies_)
        $('no-policies').hidden = false;
      if (policyData.status.displayStatusSection)
        $('status-section').hidden = false;

      // This is the javascript code that processes the template:
      var input = new JsEvalContext(policyData);
      var output = $('data-template');
      jstProcess(input, output);

      var toggles = document.querySelectorAll('.policy-set * .toggler');
      for (var i = 0; i < toggles.length; i++) {
        toggles[i].hidden = true;
        toggles[i].onclick = function() {
          Policy.getInstance().toggleCellExpand_(this);
        };
      }

      var containers = document.querySelectorAll('.text-container');
      for (var i = 0; i < containers.length; i++)
        this.initTextContainer_(containers[i]);
    },

    /**
     * Filters the table of policies by name.
     * @param {string} term The search string.
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
        row.hidden =
            !(cellContents.toLowerCase().indexOf(this.searchTerm_) >= 0);
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
      for (var i = 0; i < tableRows.length; i++)
        tableRows[i].hidden = !($('toggle-unsent-policies').checked);

      // Filter table again in case a search was active.
      this.filterTable(this.searchTerm_);
    },

    /**
     * Expands or collapses a table cell that has overflowing text.
     * @param {Object} toggler The toggler that was clicked on.
     * @private
     */
    toggleCellExpand_: function(toggler) {
      var textContainer = toggler.parentElement;
      textContainer.collapsed = !textContainer.collapsed;

      if (textContainer.collapsed)
        this.collapseCell_(textContainer);
      else
        this.expandCell_(textContainer);
    },

    /**
     * Collapses all expanded table cells and updates the visibility of the
     * toggles accordingly. Should be called before the policy information in
     * the table is updated.
     */
    collapseExpandedCells: function() {
      var textContainers = document.querySelectorAll('.text-expanded');
      for (var i = 0; i < textContainers.length; i++)
        this.collapseCell_(textContainers[i]);
    },

    /**
     * Expands a table cell so that all the text it contains is visible.
     * @param {Object} textContainer The cell's div element that contains the
     * text.
     * @private
     */
    expandCell_: function(textContainer) {
      textContainer.classList.remove('text-collapsed');
      textContainer.classList.add('text-expanded');
      textContainer.querySelector('.expand').hidden = true;
      textContainer.querySelector('.collapse').hidden = false;
    },

    /**
     * Collapses a table cell so that overflowing text is hidden.
     * @param {Object} textContainer The cell's div element that contains the
     * text.
     * @private
     */
    collapseCell_: function(textContainer) {
      textContainer.classList.remove('text-expanded');
      textContainer.classList.add('text-collapsed');
      textContainer.querySelector('.expand').hidden = false;
      textContainer.querySelector('.collapse').hidden = true;
    },

    /**
     * Initializes a text container, showing the expand toggle if necessary.
     * @param {Object} textContainer The text container element.
     */
    initTextContainer_: function(textContainer) {
      textContainer.collapsed = true;
      var textValue = textContainer.querySelector('.text-value');

      // If the text is wider than the text container, the expand toggler should
      // appear.
      if (textContainer.offsetWidth < textValue.offsetWidth ||
          textContainer.offsetHeight < textValue.offsetHeight) {
        this.collapseCell_(textContainer);
      }
    }
  };

  /**
   * Asks the C++ PolicyUIHandler to get details about policies and status
   * information. The PolicyUIHandler should reply to returnData() (below).
   */
  Policy.requestData = function() {
    chrome.send('requestData');
  };

  /**
   * Called by the C++ PolicyUIHandler when it has the requested data.
   * @param {Object} policyData The policy information in the format described
   * by the policyDataFormat.
   */
  Policy.returnData = function(policyData) {
    var policy = Policy.getInstance();
    policy.collapseExpandedCells();
    policy.renderTemplate(policyData);
    policy.updatePolicyVisibility();
  };

  /**
   * Called by the C++ PolicyUIHandler when a requested policy refresh has
   * completed.
   */
  Policy.refreshDone = function() {
    $('fetch-policies-button').disabled = false;
  };

  /**
   * Asks the C++ PolicyUIHandler to re-fetch policy information.
   */
  Policy.triggerPolicyFetch = function() {
    chrome.send('fetchPolicy');
  };

  /**
   * Determines whether a policy should be visible or not.
   * @param {Object} policy An entry in the 'policies' array given by the above
   * PolicyDataFormat.
   */
  Policy.shouldDisplayPolicy = function(policy) {
    return $('toggle-unsent-policies').checked || policy.set;
  };

  /**
   * Initializes the page and loads the list of policies and the policy
   * status data.
   */
  Policy.initialize = function() {
    Policy.requestData();

    // Set HTML event handlers.
    $('fetch-policies-button').onclick = function(event) {
      this.disabled = true;
      Policy.triggerPolicyFetch();
    };

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
