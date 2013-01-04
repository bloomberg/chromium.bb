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
      {adapter: 'wlan0', name: localStrings.getString('wlan0'), kind: 'wifi'},
      {adapter: 'eth0', name: localStrings.getString('eth0'), kind: 'ethernet'},
      {adapter: 'eth1', name: localStrings.getString('eth1'), kind: 'ethernet'},
      {adapter: 'wwan0', name: localStrings.getString('wwan0'), kind: '3g'},
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
   * List of ping test status.
   * The numeric value assigned to each status reflects how much progress has
   * been made for the ping test.
   *
   * @enum {int}
   */
  DiagPage.PingTestStatus = {
      NOT_STARTED: 0,
      IN_PROGRESS: 1,
      FAILED: 2,
      SUCCEEDED: 3
  };

  /**
   * Image elements for icons.
   */
  DiagPage.FailIconElement = document.createElement('img');
  DiagPage.TickIconElement = document.createElement('img');
  DiagPage.FailIconElement.setAttribute('src', 'chrome://diagnostics/fail.png');
  DiagPage.TickIconElement.setAttribute('src', 'chrome://diagnostics/tick.png');

  DiagPage.prototype = {
    /**
     * Perform initial setup.
     */
    initialize: function() {
      // Reset the diag page state.
      this.reset_();

      // Register event handlers.
      $('connectivity-refresh').addEventListener('click', function() {
        if (!this.getNetifStatusInProgress_)
          this.reset_();
      }.bind(this));
    },

    /**
     * Resets the diag page state.
     */
    reset_: function() {
      // Initialize member variables.
      this.activeAdapter_ = -1;
      this.adapterStatus_ = new Array();
      if (!this.pingTestStatus_ ||
          this.pingTestStatus_ != DiagPage.PingTestStatus.IN_PROGRESS) {
        this.pingTestStatus_ = DiagPage.PingTestStatus.NOT_STARTED;
      }

      // Initialize the UI with "loading" message.
      $('loading').hidden = false;
      $('choose-adapter').hidden = true;
      removeChildren($('adapter-selection'));
      removeChildren($('connectivity-status'));

      // Call into Chrome to get network interfaces status.
      chrome.send('getNetworkInterfaces');
      this.getNetifStatusInProgress_ = true;
    },

    /**
     * Updates the connectivity status with netif information.
     * @param {Object} netifStatus Dictionary of network adapter status.
     */
    setNetifStatus_: function(netifStatus) {
      // Hide the "loading" message and show the "choose-adapter" message.
      $('loading').hidden = true;
      $('choose-adapter').hidden = false;

      // Update netif state.
      var found_valid_ip = false;
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

        if (this.adapterStatus_[i] == DiagPage.AdapterStatus.VALID_IP)
          found_valid_ip = true;
      }

      // If we have valid IP, start ping test.
      if (found_valid_ip &&
          this.pingTestStatus_ == DiagPage.PingTestStatus.NOT_STARTED) {
        this.pingTestStatus_ == DiagPage.PingTestStatus.IN_PROGRESS;
        chrome.send('testICMP', [String('8.8.8.8')]);
      }

      // Update UI
      this.updateAdapterSelection_();
      this.updateConnectivityStatus_();

      // Clear the getNetifStatusInProgress flag.
      this.getNetifStatusInProgress_ = false;
    },

    /**
     * Updates the ICMP connectivity status.
     * @param {Object} testICMPStatus Dictionary of ICMP connectivity status.
     */
    setTestICMPStatus_: function(testICMPStatus) {
      // Update the ping test state.
      for (var prop in testICMPStatus) {
        var status = testICMPStatus[prop];
        if (status.sent && status.recvd && status.sent == status.recvd)
          this.pingTestStatus_ = DiagPage.PingTestStatus.SUCCEEDED;
        else
          this.pingTestStatus_ = DiagPage.PingTestStatus.FAILED;
        break;
      }

      // Update UI
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
      // Clear adapter selection section.
      var adapterSelectionElement = $('adapter-selection');
      removeChildren(adapterSelectionElement);
      // Create HTML radio input elements.
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
      var kind = DiagPage.AdapterType[adapter].kind;

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
              localStrings.getStringF('fix-no-ip-' + kind);
          connectivityStatusElement.insertBefore(errorElement,
              testStatusElements[2]);
          connectivityStatusElement.insertBefore(recommendationElement,
              testStatusElements[2]);
          testStatusElements[2].className = 'test-pending';
        } else {
          testStatusElements[1].appendChild(
              DiagPage.TickIconElement.cloneNode());
          testStatusElements[2].className = 'test-performed';
          if (this.pingTestStatus_ == DiagPage.PingTestStatus.NOT_STARTED ||
              this.pingTestStatus_ == DiagPage.PingTestStatus.IN_PROGRESS) {
            // TODO(hshi): make the ellipsis below i18n-friendly.
            testStatusElements[2].innerHTML += '...';
          } else {
            if (this.pingTestStatus_ == DiagPage.PingTestStatus.FAILED) {
              errorElement.appendChild(DiagPage.FailIconElement.cloneNode());
              errorElement.appendChild(document.createTextNode(
                localStrings.getString('gateway-not-connected-to-internet')));
              recommendationElement.innerHTML =
                localStrings.getStringF('fix-gateway-connection');
              connectivityStatusElement.appendChild(errorElement);
              connectivityStatusElement.appendChild(recommendationElement);
            } else {
              testStatusElements[2].appendChild(
                DiagPage.TickIconElement.cloneNode());
            }
          }
        }
      }
    }
  };

  DiagPage.setNetifStatus = function(netifStatus) {
    DiagPage.getInstance().setNetifStatus_(netifStatus);
  }

  DiagPage.setTestICMPStatus = function(testICMPStatus) {
    DiagPage.getInstance().setTestICMPStatus_(testICMPStatus);
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
