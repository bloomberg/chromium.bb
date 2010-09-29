// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // CertificateManagerTab class:

  /**
   * blah
   * @param {!string} id The id of this tab.
   */
  function CertificateManagerTab(id) {
    this.tree = $(id + '-tree');

    options.CertificatesTree.decorate(this.tree);
    this.tree.addEventListener('change',
        this.handleCertificatesTreeChange_.bind(this));


    this.viewButton = $(id + '-view');

    var tree = this.tree;
    this.viewButton.onclick = function(e) {
      var selected = tree.selectedItem;
      chrome.send('viewCertificate', [selected.data.id]);
    }
  }

  CertificateManagerTab.prototype = {

    /**
     * Update button state.
     * @private
     * @param {!Object} data The data of the selected item.
     */
    updateButtonState: function(data) {
      var isCert = !!data && data.id.substr(0, 5) == 'cert-';
      this.viewButton.disabled = !isCert;
    },

    /**
     * Handles certificate tree selection change.
     * @private
     * @param {!Event} e The change event object.
     */
    handleCertificatesTreeChange_: function(e) {
      var data = null;
      if (this.tree.selectedItem) {
        data = this.tree.selectedItem.data;
      }

      this.updateButtonState(data);
    },

  }

  /////////////////////////////////////////////////////////////////////////////
  // CertificateManager class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function CertificateManager(model) {
    OptionsPage.call(this, 'certificateManager',
                     localStrings.getString('certificateManagerPage'),
                     'certificateManagerPage');
  }

  cr.addSingletonGetter(CertificateManager);

  CertificateManager.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.personalTab = new CertificateManagerTab('personalCertsTab');
      this.emailTab = new CertificateManagerTab('emailCertsTab');
      this.serverTab = new CertificateManagerTab('serverCertsTab');
      this.caTab = new CertificateManagerTab('caCertsTab');
      this.otherTab = new CertificateManagerTab('otherCertsTab');

      this.addEventListener('visibleChange', this.handleVisibleChange_);
    },

    initalized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (!this.initalized_ && this.visible) {
        this.initalized_ = true;
        chrome.send('populateCertificateManager');
      }
    }
  };

  // CertificateManagerHandler callbacks.
  CertificateManager.onPopulateTree = function(args) {
    $(args[0]).populate(args[1]);
  };

  // Export
  return {
    CertificateManagerTab: CertificateManagerTab,
    CertificateManager: CertificateManager
  };

});
