// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // ProxyOptions class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function ProxyOptions(model) {
    OptionsPage.call(this, 'proxy', localStrings.getString('proxyPage'),
                     'proxyPage');
  }

  ProxyOptions.getInstance = function() {
    if (!ProxyOptions.instance_) {
      ProxyOptions.instance_ = new ProxyOptions(null);
    }
    return ProxyOptions.instance_;
  };

  ProxyOptions.prototype = {
    // Inherit ProxyOptions from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initializes ProxyOptions page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Set up ignored page.
      options.proxyexceptions.ProxyExceptions.decorate($('ignoredHostList'));

      this.addEventListener('visibleChange', this.handleVisibleChange_);
      $('removeHost').addEventListener('click', this.handleRemoveExceptions_);
      $('addHost').addEventListener('click', this.handleAddException_);
      $('directProxy').addEventListener('click', this.disableManual_);
      $('manualProxy').addEventListener('click', this.enableManual_);
      $('autoProxy').addEventListener('click', this.disableManual_);
      $('proxyAllProtocols').addEventListener('click', this.toggleSingle_);
    },

    proxyListInitalized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      this.toggleSingle_();
      if ($('manualProxy').checked) {
        this.enableManual_();
      } else {
        this.disableManual_();
      }
      if (!this.proxyListInitalized_ && this.visible) {
        this.proxyListInitalized_ = true;
        $('ignoredHostList').redraw();
      }
    },

    /**
     * Handler for when the user clicks on the checkbox to allow a
     * single proxy usage.
     * @private
     * @param {Event} e Click Event.
     */
    toggleSingle_: function(e) {
      if($('proxyAllProtocols').value) {
        $('multiProxy').style.display = 'none';
        $('singleProxy').style.display = 'block';
      } else {
        $('multiProxy').style.display = 'block';
        $('singleProxy').style.display = 'none';
      }
    },

    /**
     * Handler for selecting a radio button that will disable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    disableManual_: function(e) {
      $('proxyAllProtocols').disabled = true;
      $('proxyHostName').disabled = true;
      $('proxyHostPort').disabled = true;
      $('proxyHostSingleName').disabled = true;
      $('proxyHostSinglePort').disabled = true;
      $('secureProxyHostName').disabled = true;
      $('secureProxyPort').disabled = true;
      $('ftpProxy').disabled = true;
      $('ftpProxyPort').disabled = true;
      $('socksHost').disabled = true;
      $('socksPort').disabled = true;
      $('newHost').disabled = true;
      $('removeHost').disabled = true;
      $('addHost').disabled = true;
      $('advancedConfig').style.display = 'none';
    },

    /**
     * Handler for selecting a radio button that will enable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    enableManual_: function(e) {
      $('proxyAllProtocols').disabled = false;
      $('proxyHostName').disabled = false;
      $('proxyHostPort').disabled = false;
      $('proxyHostSingleName').disabled = false;
      $('proxyHostSinglePort').disabled = false;
      $('secureProxyHostName').disabled = false;
      $('secureProxyPort').disabled = false;
      $('ftpProxy').disabled = false;
      $('ftpProxyPort').disabled = false;
      $('socksHost').disabled = false;
      $('socksPort').disabled = false;
      $('newHost').disabled = false;
      $('removeHost').disabled = false;
      $('addHost').disabled = false;
      $('advancedConfig').style.display = '-webkit-box';
    },

    /**
     * Handler for "add" event fired from userNameEdit.
     * @private
     * @param {Event} e Add event fired from userNameEdit.
     */
    handleAddException_: function(e) {
      var exception = $('newHost').value;
      $('newHost').value = '';
      $('ignoredHostList').addException(exception);
    },

    /**
     * Handler for when the remove button is clicked
     * @private
     */
    handleRemoveExceptions_: function(e) {
      var selectedItems = $('ignoredHostList').selectedItems;
      for (var x = 0; x < selectedItems.length; x++) {
        $('ignoredHostList').removeException(selectedItems[x]);
      }
    }
  };

  // Export
  return {
    ProxyOptions: ProxyOptions
  };

});
