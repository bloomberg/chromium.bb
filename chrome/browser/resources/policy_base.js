// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('policy', function() {

  /**
   * A hack to check if we are displaying the mobile version of this page by
   * checking if the first column is hidden.
   * @return {boolean} True if this is the mobile version.
   */
  const isMobilePage = function() {
    return document.defaultView
               .getComputedStyle(document.querySelector('.scope-column'))
               .display == 'none';
  };

  /**
   * A box that shows the status of cloud policy for a device, machine or user.
   * @constructor
   * @extends {HTMLFieldSetElement}
   */
  const StatusBox = cr.ui.define(function() {
    const node = $('status-box-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  StatusBox.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLFieldSetElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {},

    /**
     * Sets the text of a particular named label element in the status box
     * and updates the visibility if needed.
     * @param {string} labelName The name of the label element that is being
     *     updated.
     * @param {string} labelValue The new text content for the label.
     * @param {boolean=} needsToBeShown True if we want to show the label
     *     False otherwise.
     */
    setLabelAndShow_: function(labelName, labelValue, needsToBeShown = true) {
      const labelElement = this.querySelector(labelName);
      labelElement.textContent = labelValue || '';
      if (needsToBeShown) {
        labelElement.parentElement.hidden = false;
      }
    },
    /**
     * Populate the box with the given cloud policy status.
     * @param {string} scope The policy scope, either "device", "machine", or
     *     "user".
     * @param {Object} status Dictionary with information about the status.
     */
    initialize: function(scope, status) {
      const notSpecifiedString = loadTimeData.getString('notSpecified');
      if (scope == 'device') {
        // For device policy, set the appropriate title and populate the topmost
        // status item with the domain the device is enrolled into.
        this.querySelector('.legend').textContent =
            loadTimeData.getString('statusDevice');
        this.setLabelAndShow_(
            '.enterprise-enrollment-domain', status.enterpriseEnrollmentDomain);
        this.setLabelAndShow_(
            '.enterprise-display-domain', status.enterpriseDisplayDomain);

        // Populate the device naming information.
        // Populate the asset identifier.
        this.setLabelAndShow_(
            '.asset-id', status.assetId || notSpecifiedString);

        // Populate the device location.
        this.setLabelAndShow_(
            '.location', status.location || notSpecifiedString);

        // Populate the directory API ID.
        this.setLabelAndShow_(
            '.directory-api-id', status.directoryApiId || notSpecifiedString);
        this.setLabelAndShow_('.client-id', status.clientId);
      } else if (scope == 'machine') {
        // For machine policy, set the appropriate title and populate
        // machine enrollment status with the information that applies
        // to this machine.
        this.querySelector('.legend').textContent =
            loadTimeData.getString('statusMachine');
        this.setLabelAndShow_('.machine-enrollment-device-id', status.deviceId);
        this.setLabelAndShow_(
            '.machine-enrollment-token', status.enrollmentToken);
        this.setLabelAndShow_('.machine-enrollment-name', status.machine);
        this.setLabelAndShow_('.machine-enrollment-domain', status.domain);
      } else {
        // For user policy, set the appropriate title and populate the topmost
        // status item with the username that policies apply to.
        this.querySelector('.legend').textContent =
            loadTimeData.getString('statusUser');
        // Populate the topmost item with the username.
        this.setLabelAndShow_('.username', status.username);
        // Populate the user gaia id.
        this.setLabelAndShow_('.gaia-id', status.gaiaId || notSpecifiedString);
        this.setLabelAndShow_('.client-id', status.clientId);
      }
      this.setLabelAndShow_(
          '.time-since-last-refresh', status.timeSinceLastRefresh, false);
      this.setLabelAndShow_('.refresh-interval', status.refreshInterval, false);
      this.setLabelAndShow_('.status', status.status, false);
    },
  };

  /**
   * A single policy's entry in the policy table.
   * @constructor
   * @extends {HTMLTableSectionElement}
   */
  const Policy = cr.ui.define(function() {
    const node = $('policy-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  Policy.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLTableSectionElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      const links = this.querySelectorAll('.overflow-link');
      for (let i = 0; i < links.length; i++) {
        this.setExpandedText_(links[i], true);
      }
      this.querySelector('.toggle-expanded-value')
          .addEventListener('click', this.toggleExpanded_);
      this.querySelector('.toggle-expanded-status')
          .addEventListener('click', this.toggleExpanded_);
    },

    /**
     * Populate the table columns with information about the policy name, value
     * and status.
     * @param {string} name The policy name.
     * @param {Object} value Dictionary with information about the policy value.
     * @param {boolean} unknown Whether the policy name is not recognized.
     */
    initialize: function(name, value, unknown) {
      this.name = name;
      this.unset = !value;

      // Populate the name column.
      this.querySelector('.name-link').textContent = name;
      this.querySelector('.name-link').href =
          'https://chromium.org/administrators/policy-list-3#' + name;
      this.querySelector('.name-link').title =
          loadTimeData.getStringF('policyLearnMore', name);

      if (unknown) {
        this.classList.add('no-help-link');
      }

      // Populate the remaining columns with policy scope, level and value if a
      // value has been set. Otherwise, leave them blank.
      if (value) {
        this.querySelector('.scope').textContent = loadTimeData.getString(
            value.scope == 'user' ? 'scopeUser' : 'scopeDevice');
        this.querySelector('.level').textContent = loadTimeData.getString(
            value.level == 'recommended' ? 'levelRecommended' :
                                           'levelMandatory');
        this.querySelector('.source').textContent =
            loadTimeData.getString(value.source);
        this.querySelector('.value').textContent = value.value;
        this.querySelector('.expanded-value-container .expanded-text')
            .textContent = value.value;
      }

      // Populate the status column.
      let status;
      if (!value) {
        // If the policy value has not been set, show an error message.
        status = loadTimeData.getString('unset');
      } else if (unknown) {
        // If the policy name is not recognized, show an error message.
        status = loadTimeData.getString('unknown');
      } else if (value.error) {
        // If an error occurred while parsing the policy value, show the error
        // message.
        status = value.error;
      } else {
        // Otherwise, indicate that the policy value was parsed correctly.
        status = loadTimeData.getString('ok');
      }
      this.querySelector('.status').textContent = status;
      this.querySelector('.expanded-status-container .expanded-text')
          .textContent = status;
      if (isMobilePage()) {
        // The number of columns which are hidden by the css file for the mobile
        // (Android) version of this page.
        /** @const */ const HIDDEN_COLUMNS_IN_MOBILE_VERSION = 2;
        const expandedCells = this.querySelector('.expanded-text');
        for (const cell in expandedCells) {
          cell.setAttribute(
              'colspan', cell.colSpan - HIDDEN_COLUMNS_IN_MOBILE_VERSION);
        }
      }
    },

    /*
     * Get width of a DOM element inside a container.
     * @param {Object} container Container for the element.
     * @param {string} elemClass Class of the element containing text.
     * @private
     */
    getElementWidth_: function(container, elemClass) {
      return container.querySelector(elemClass).offsetWidth;
    },

    /*
     * Update the value width for a container if necessary.
     * @param {Object} container Container for the DOM element.
     * @param {string} elemClass Class of the element containing text.
     * @private
     */
    updateContainerWidth_: function(container, elemClass) {
      if (container.valueWidth == undefined) {
        container.valueWidth = this.getElementWidth_(container, elemClass);
      }
    },

    /**
     * Check the table columns for overflow. Most columns are automatically
     * elided when overflow occurs. The only action required is to add a
     * tooltip that shows the complete content. The value and status columns
     * are exceptions. If overflow occurs here, the contents are replaced links
     * that toggle the visibility of additional rows containing the complete
     * value and/or status texts.
     */
    checkOverflow: function() {
      // Set a tooltip on all overflowed columns except the columns value and
      // status.
      const divs = this.querySelectorAll('div.elide');
      for (let i = 0; i < divs.length; i++) {
        const div = divs[i];
        div.title = div.offsetWidth < div.scrollWidth ? div.textContent : '';
      }
      const collapsibleCells = this.querySelectorAll('.collapsible-cell');
      for (let i = 0; i < collapsibleCells.length; i++) {
        const cell = collapsibleCells[i];
        // Cache the width of the column's contents when it is first shown.
        // This is required to be able to check whether the contents would still
        // overflow the column once it has been hidden and replaced by a link.
        this.updateContainerWidth_(cell, '.cell-text');
        // Determine whether the contents of the column overflows.
        if (cell.offsetWidth <= cell.valueWidth) {
          cell.querySelector('.cell-text').hidden = true;
          cell.querySelector('.overflow-link').hidden = false;
        } else {
          cell.querySelector('.cell-text').hidden = false;
          cell.querySelector('.overflow-link').hidden = true;
          this.querySelector(cell.dataset.expandableRow).hidden = true;
          this.setExpandedText_(cell.querySelector('.overflow-link'), true);
        }
      }
    },

    /**
     * Sets the text for a toggle link with hide/show modes.
     * @param {Object} link The DOM element to set the text for.
     * @param {bool} show Indicates if the link is in show of hide mode.
     */
    setExpandedText_: function(link, show) {
      link.textContent =
          loadTimeData.getString(show ? link.dataset.show : link.dataset.hide);
    },

    /**
     * Toggle the visibility of an additional row containing the complete text.
     * @private
     */
    toggleExpanded_: function() {
      const cell = this.parentElement;
      // get the expandable row corresponding to this collapsed cell
      row = cell.closest('tbody').querySelector(cell.dataset.expandableRow);
      row.hidden = !row.hidden;
      this.setExpandedText_(this, !row.hidden);
    },
  };

  /**
   * A table of policies and their values.
   * @constructor
   * @extends {HTMLTableElement}
   */
  const PolicyTable = cr.ui.define('tbody');

  PolicyTable.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLTableElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.policies_ = {};
      this.filterPattern_ = '';
      window.addEventListener('resize', this.checkOverflow_.bind(this));
    },

    /**
     * Initialize the list of all known policies.
     * @param {Object} names Dictionary containing all known policy names.
     */
    setPolicyNames: function(names) {
      this.policies_ = names;
      this.setPolicyValues({});
    },

    /**
     * Populate the table with the currently set policy values and any errors
     * detected while parsing these.
     * @param {Object} values Dictionary containing the current policy values.
     */
    setPolicyValues: function(values) {
      // Remove all policies from the table.
      const policies = this.getElementsByTagName('tbody');
      while (policies.length > 0) {
        this.removeChild(policies.item(0));
      }

      // First, add known policies whose value is currently set.
      const unset = [];
      for (const name in this.policies_) {
        if (name in values) {
          this.setPolicyValue_(name, values[name], false);
        } else {
          unset.push(name);
        }
      }

      // Second, add policies whose value is currently set but whose name is not
      // recognized.
      for (const name in values) {
        if (!(name in this.policies_)) {
          this.setPolicyValue_(name, values[name], true);
        }
      }

      // Finally, add known policies whose value is not currently set.
      for (let i = 0; i < unset.length; i++) {
        this.setPolicyValue_(unset[i], undefined, false);
      }

      // Filter the policies.
      this.filter();
    },

    /**
     * Set the filter pattern. Only policies whose name contains |pattern| are
     * shown in the policy table. The filter is case insensitive. It can be
     * disabled by setting |pattern| to an empty string.
     * @param {string} pattern The filter pattern.
     */
    setFilterPattern: function(pattern) {
      this.filterPattern_ = pattern.toLowerCase();
      this.filter();
    },

    /**
     * Filter policies. Only policies whose name contains the filter pattern are
     * shown in the table. Furthermore, policies whose value is not currently
     * set are only shown if the corresponding checkbox is checked.
     */
    filter: function() {
      const showUnset = $('show-unset').checked;
      const policies = this.getElementsByTagName('tbody');
      for (let i = 0; i < policies.length; i++) {
        const policy = policies[i];
        policy.hidden = policy.unset && !showUnset ||
            policy.name.toLowerCase().indexOf(this.filterPattern_) == -1;
      }
      if (this.querySelector('tbody:not([hidden])')) {
        this.parentElement.classList.remove('empty');
      } else {
        this.parentElement.classList.add('empty');
      }
      setTimeout(this.checkOverflow_.bind(this), 0);
    },

    /**
     * Check the table columns for overflow.
     * @private
     */
    checkOverflow_: function() {
      const policies = this.getElementsByTagName('tbody');
      for (let i = 0; i < policies.length; i++) {
        if (!policies[i].hidden) {
          policies[i].checkOverflow();
        }
      }
    },

    /**
     * Add a policy with the given |name| and |value| to the table.
     * @param {string} name The policy name.
     * @param {Object} value Dictionary with information about the policy value.
     * @param {boolean} unknown Whether the policy name is not recoginzed.
     * @private
     */
    setPolicyValue_: function(name, value, unknown) {
      const policy = new Policy;
      policy.initialize(name, value, unknown);
      this.appendChild(policy);
    },
  };

  /**
   * A singelton object that handles communication between browser and WebUI.
   * @constructor
   */
  function Page() {}

  // Make Page a singleton.
  cr.addSingletonGetter(Page);

  /**
   * Provide a list of all known policies to the UI. Called by the browser on
   * page load.
   * @param {Object} names Dictionary containing all known policy names.
   */
  Page.setPolicyNames = function(names) {
    const page = this.getInstance();

    // Clear all policy tables.
    page.mainSection.innerHTML = '';
    page.policyTables = {};

    let table;
    // Create tables and set known policy names for Chrome and extensions.
    if (names.hasOwnProperty('chromePolicyNames')) {
      table = page.appendNewTable('chrome', 'Chrome policies', '');
      table.setPolicyNames(names.chromePolicyNames);
    }

    if (names.hasOwnProperty('extensionPolicyNames')) {
      for (const ext in names.extensionPolicyNames) {
        table = page.appendNewTable(
            'extension-' + ext, names.extensionPolicyNames[ext].name,
            'ID: ' + ext);
        table.setPolicyNames(names.extensionPolicyNames[ext].policyNames);
      }
    }
  };

  /**
   * Provide a list of the currently set policy values and any errors detected
   * while parsing these to the UI. Called by the browser on page load and
   * whenever policy values change.
   * @param {Object} values Dictionary containing the current policy values.
   */
  Page.setPolicyValues = function(values) {
    const page = this.getInstance();
    let table;
    if (values.hasOwnProperty('chromePolicies')) {
      table = page.policyTables['chrome'];
      table.setPolicyValues(values.chromePolicies);
    }

    if (values.hasOwnProperty('extensionPolicies')) {
      for (const extensionId in values.extensionPolicies) {
        table = page.policyTables['extension-' + extensionId];
        if (table) {
          table.setPolicyValues(values.extensionPolicies[extensionId]);
        }
      }
    }
  };

  /**
   * Provide the current cloud policy status to the UI. Called by the browser on
   * page load if cloud policy is present and whenever the status changes.
   * @param {Object} status Dictionary containing the current policy status.
   */
  Page.setStatus = function(status) {
    this.getInstance().setStatus(status);
  };

  /**
   * Notify the UI that a request to reload policy values has completed. Called
   * by the browser after a request to reload policy has been sent by the UI.
   */
  Page.reloadPoliciesDone = function() {
    this.getInstance().reloadPoliciesDone();
  };

  Page.prototype = {
    /**
     * Main initialization function. Called by the browser on page load.
     */
    initialize: function() {
      cr.ui.FocusOutlineManager.forDocument(document);

      this.mainSection = $('main-section');
      this.policyTables = {};

      // Place the initial focus on the filter input field.
      $('filter').focus();

      const self = this;
      $('filter').onsearch = function(event) {
        for (policyTable in self.policyTables) {
          self.policyTables[policyTable].setFilterPattern(this.value);
        }
      };
      $('reload-policies').onclick = function(event) {
        this.disabled = true;
        chrome.send('reloadPolicies');
      };

      $('export-policies').onclick = function(event) {
        chrome.send('exportPoliciesJSON');
      };

      $('show-unset').onchange = function() {
        for (policyTable in self.policyTables) {
          self.policyTables[policyTable].filter();
        }
      };

      // Notify the browser that the page has loaded, causing it to send the
      // list of all known policies, the current policy values and the cloud
      // policy status.
      chrome.send('initialized');
    },

    /**
     * Creates a new policy table section, adds the section to the page,
     * and returns the new table from that section.
     * @param {string} id The key for storing the new table in policyTables.
     * @param {string} label_title Title for this policy table.
     * @param {string} label_content Description for the policy table.
     * @return {Element} The newly created table.
     */
    appendNewTable: function(id, label_title, label_content) {
      const newSection =
          this.createPolicyTableSection(id, label_title, label_content);
      if (id != 'chrome') {
        newSection.classList.add('no-help-link');
      }
      this.mainSection.appendChild(newSection);
      return this.policyTables[id];
    },

    /**
     * Creates a new section containing a title, description and table of
     * policies.
     * @param {id} id The key for storing the new table in policyTables.
     * @param {string} label_title Title for this policy table.
     * @param {string} label_content Description for the policy table.
     * @return {Element} The newly created section.
     */
    createPolicyTableSection: function(id, label_title, label_content) {
      const section = document.createElement('section');
      section.setAttribute('class', 'policy-table-section');

      // Add title and description.
      const title = window.document.createElement('h3');
      title.textContent = label_title;
      section.appendChild(title);

      if (label_content) {
        const description = window.document.createElement('div');
        description.classList.add('table-description');
        description.textContent = label_content;
        section.appendChild(description);
      }

      // Add 'No Policies Set' element.
      const noPolicies = window.document.createElement('div');
      noPolicies.classList.add('no-policies-set');
      noPolicies.textContent = loadTimeData.getString('noPoliciesSet');
      section.appendChild(noPolicies);

      // Add table of policies.
      const newTable = this.createPolicyTable();
      this.policyTables[id] = newTable;
      section.appendChild(newTable);

      return section;
    },

    tableHeadings: ['Scope', 'Level', 'Source', 'Name', 'Value', 'Status'],

    /**
     * Creates a new table for displaying policies.
     * @return {Element} The newly created table.
     */
    createPolicyTable: function() {
      const newTable = window.document.createElement('table');
      const tableHead = window.document.createElement('thead');
      const tableRow = window.document.createElement('tr');
      for (let i = 0; i < this.tableHeadings.length; i++) {
        const tableHeader = window.document.createElement('th');
        tableHeader.classList.add(
            this.tableHeadings[i].toLowerCase() + '-column');
        tableHeader.textContent =
            loadTimeData.getString('header' + this.tableHeadings[i]);
        tableRow.appendChild(tableHeader);
      }
      tableHead.appendChild(tableRow);
      newTable.appendChild(tableHead);
      cr.ui.decorate(newTable, PolicyTable);
      return newTable;
    },

    /**
     * Update the status section of the page to show the current cloud policy
     * status.
     * @param {Object} status Dictionary containing the current policy status.
     */
    setStatus: function(status) {
      // Remove any existing status boxes.
      const container = $('status-box-container');
      while (container.firstChild) {
        container.removeChild(container.firstChild);
      }
      // Hide the status section.
      const section = $('status-section');
      section.hidden = true;

      // Add a status box for each scope that has a cloud policy status.
      for (const scope in status) {
        const box = new StatusBox;
        box.initialize(scope, status[scope]);
        container.appendChild(box);
        // Show the status section.
        section.hidden = false;
      }
    },

    /**
     * Re-enable the reload policies button when the previous request to reload
     * policies values has completed.
     */
    reloadPoliciesDone: function() {
      $('reload-policies').disabled = false;
    },
  };

  return {Page: Page, PolicyTable: PolicyTable, Policy: Policy};
});
