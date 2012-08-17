// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

cr.define('diag', function() {
  /**
   * Encapsulated handling of the diagnostics page.
   */
  function DiagPage() {}

  cr.addSingletonGetter(DiagPage);

  /*
   * Remove all children nodes for an element.
   * @param {element} parent of the elements to be removed.
   */
  function removeChildren(element) {
    element.textContent = '';
  }

  /**
   * List of network adapter types.
   */
  DiagPage.AdapterType = [
      {adapter: 'wlan0', name: localStrings.getString('wlan0')},
      {adapter: 'eth0', name: localStrings.getString('eth0')},
      {adapter: 'eth1', name: localStrings.getString('eth1')},
      {adapter: 'wwan0', name: localStrings.getString('wwan0')},
  ];

  /**
   * List of network adapter status.
   * The numeric value assigned to each status reflects how healthy the network
   * adapter is.
   *
   * @enum {int}
   */
  DiagPage.AdapterStatus = {
      NOT_FOUND: 0,
      DISABLED: 1,
      NO_IP: 2,
      VALID_IP: 3
  };

  /**
   * Image elements for icons.
   */
  DiagPage.FailIconElement = document.createElement('img');
  DiagPage.TickIconElement = document.createElement('img');
  DiagPage.FailIconElement.setAttribute('src', 'fail.png');
  DiagPage.TickIconElement.setAttribute('src', 'tick.png');

  DiagPage.prototype = {
    /**
     * Perform initial setup.
     */
    initialize: function() {
      // Initialize member variables.
      this.activeAdapter_ = -1;
      this.adapterStatus_ = new Array();

      // Attempt to update.
      chrome.send('pageLoaded');
    },

    /**
     * Updates the connectivity status with netif information.
     * @param {String} netifStatus Dictionary of network adapter status.
     */
    setNetifStatus_: function(netifStatus) {
      // Hide the "loading" message.
      $('loading').hidden = true;

      // Update netif state.
      for (var i = 0; i < DiagPage.AdapterType.length; i++) {
        var adapterType = DiagPage.AdapterType[i];
        var status = netifStatus[adapterType.adapter];
        if (!status)
          this.adapterStatus_[i] = DiagPage.AdapterStatus.NOT_FOUND;
        else if (!status.flags || status.flags.indexOf('up') == -1)
          this.adapterStatus_[i] = DiagPage.AdapterStatus.DISABLED;
        else if (!status.ipv4)
          this.adapterStatus_[i] = DiagPage.AdapterStatus.NO_IP;
        else
          this.adapterStatus_[i] = DiagPage.AdapterStatus.VALID_IP;
      }

      // Update UI
      this.updateAdapterSelection_();
      this.updateConnectivityStatus_();
    },

    /**
     * Gets the HTML radio input element id for a network adapter.
     * @private
     */
    getAdapterElementId_: function(adapter) {
      return 'adapter-' + DiagPage.AdapterType[adapter].adapter;
    },

    /**
     * Gets the most active adapter based on their status.
     * @private
     */
    getActiveAdapter_: function() {
      var activeAdapter = -1;
      var activeAdapterStatus = DiagPage.AdapterStatus.NOT_FOUND;
      for (var i = 0; i < DiagPage.AdapterType.length; i++) {
        var status = this.adapterStatus_[i];
        if (status == DiagPage.AdapterStatus.NOT_FOUND)
          continue;
        if (activeAdapter == -1 || status > activeAdapterStatus) {
          activeAdapter = i;
          activeAdapterStatus = status;
        }
      }
      return activeAdapter;
    },

    /**
     * Update the adapter selection section.
     * @private
     */
    updateAdapterSelection_: function() {
      // Determine active adapter.
      if (this.activeAdapter_ == -1)
        this.activeAdapter_ = this.getActiveAdapter_();
      // Create HTML radio input elements.
      var adapterSelectionElement = $('adapter-selection');
      removeChildren(adapterSelectionElement);
      for (var i = 0; i < DiagPage.AdapterType.length; i++) {
        if (this.adapterStatus_[i] == DiagPage.AdapterStatus.NOT_FOUND)
          continue;
        var radioElement = document.createElement('input');
        var elementId = this.getAdapterElementId_(i);
        radioElement.setAttribute('type', 'radio');
        radioElement.setAttribute('name', 'adapter');
        radioElement.setAttribute('id', elementId);
        if (i == this.activeAdapter_)
          radioElement.setAttribute('checked', 'true');
        radioElement.onclick = function(adapter) {
          this.activeAdapter_ = adapter;
          this.updateConnectivityStatus_();
        }.bind(this, i);
        var labelElement = document.createElement('label');
        labelElement.setAttribute('for', elementId);
        labelElement.appendChild(radioElement);
        labelElement.appendChild(
            document.createTextNode(DiagPage.AdapterType[i].name));
        adapterSelectionElement.appendChild(labelElement);
        adapterSelectionElement.appendChild(document.createElement('br'));
      }
    },

    /**
     * Update the connectivity status for the specified network interface.
     * @private
     */
    updateConnectivityStatus_: function() {
      var adapter = this.activeAdapter_;
      var status = this.adapterStatus_[adapter];
      var name = DiagPage.AdapterType[adapter].name;

      // Status messages for individual tests.
      var connectivityStatusElement = $('connectivity-status');
      var testStatusElements = new Array();
      removeChildren(connectivityStatusElement);
      for (var i = 0; i < 3; i++) {
        testStatusElements[i] = document.createElement('div');
        connectivityStatusElement.appendChild(testStatusElements[i]);
      }
      testStatusElements[0].innerHTML =
        localStrings.getStringF('testing-hardware', name);
      testStatusElements[1].innerHTML =
        localStrings.getString('testing-connection-to-router');
      testStatusElements[2].innerHTML =
        localStrings.getString('testing-connection-to-internet');

      // Error and recommendation messages may be inserted in test status
      // elements.
      var errorElement = document.createElement('div');
      var recommendationElement = document.createElement('div');
      errorElement.className = 'test-error';
      recommendationElement.className = 'recommendation';
      testStatusElements[0].className = 'test-performed';
      if (status == DiagPage.AdapterStatus.DISABLED) {
        errorElement.appendChild(DiagPage.FailIconElement.cloneNode());
        errorElement.appendChild(document.createTextNode(
            localStrings.getStringF('adapter-disabled', name)));
        recommendationElement.innerHTML =
            localStrings.getStringF('enable-adapter', name);
        connectivityStatusElement.insertBefore(errorElement,
            testStatusElements[1]);
        connectivityStatusElement.insertBefore(recommendationElement,
            testStatusElements[1]);
        testStatusElements[1].className = 'test-pending';
        testStatusElements[2].className = 'test-pending';
      } else {
        testStatusElements[0].appendChild(DiagPage.TickIconElement.cloneNode());
        testStatusElements[1].className = 'test-performed';
        if (status == DiagPage.AdapterStatus.NO_IP) {
          errorElement.appendChild(DiagPage.FailIconElement.cloneNode());
          errorElement.appendChild(document.createTextNode(
              localStrings.getStringF('adapter-no-ip', name)));
          recommendationElement.innerHTML =
              localStrings.getStringF('fix-connection-to-router');
          connectivityStatusElement.insertBefore(errorElement,
              testStatusElements[2]);
          connectivityStatusElement.insertBefore(recommendationElement,
              testStatusElements[2]);
          testStatusElements[2].className = 'test-pending';
        } else {
          testStatusElements[1].appendChild(
              DiagPage.TickIconElement.cloneNode());
          testStatusElements[2].className = 'test-performed';
          testStatusElements[2].appendChild(
              DiagPage.TickIconElement.cloneNode());
        }
      }
    }
  };

  DiagPage.setNetifStatus = function(netifStatus) {
    DiagPage.getInstance().setNetifStatus_(netifStatus);
  }

  // Export
  return {
    DiagPage: DiagPage
  };
});

/**
 * Initialize the DiagPage upon DOM content loaded.
 */
document.addEventListener('DOMContentLoaded', function() {
  diag.DiagPage.getInstance().initialize();
});
