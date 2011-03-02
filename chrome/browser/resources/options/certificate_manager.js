// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

    var tree = this.tree;

    this.viewButton = $(id + '-view');
    this.viewButton.onclick = function(e) {
      var selected = tree.selectedItem;
      chrome.send('viewCertificate', [selected.data.id]);
    }

    this.editButton = $(id + '-edit');
    if (this.editButton !== null) {
      if (id == 'serverCertsTab') {
        this.editButton.onclick = function(e) {
          var selected = tree.selectedItem;
          chrome.send('editServerCertificate', [selected.data.id]);
        }
      } else if (id == 'caCertsTab') {
        this.editButton.onclick = function(e) {
          var data = tree.selectedItem.data;
          CertificateEditCaTrustOverlay.show(data.id, data.name);
        }
      }
    }

    this.backupButton = $(id + '-backup');
    if (this.backupButton !== null) {
      this.backupButton.onclick = function(e) {
        var selected = tree.selectedItem;
        chrome.send('exportPersonalCertificate', [selected.data.id]);
      }
    }

    this.backupAllButton = $(id + '-backup-all');
    if (this.backupAllButton !== null) {
      this.backupAllButton.onclick = function(e) {
        chrome.send('exportAllPersonalCertificates', []);
      }
    }

    this.importButton = $(id + '-import');
    if (this.importButton !== null) {
      if (id == 'personalCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importPersonalCertificate', []);
        }
      } else if (id == 'serverCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importServerCertificate', []);
        }
      } else if (id == 'caCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importCaCertificate', []);
        }
      }
    }

    this.exportButton = $(id + '-export');
    if (this.exportButton !== null) {
      this.exportButton.onclick = function(e) {
        var selected = tree.selectedItem;
        chrome.send('exportCertificate', [selected.data.id]);
      }
    }

    this.deleteButton = $(id + '-delete');
    this.deleteButton.onclick = function(e) {
      var data = tree.selectedItem.data;
      AlertOverlay.show(
          localStrings.getStringF(id + 'DeleteConfirm', data.name),
          localStrings.getString(id + 'DeleteImpact'),
          localStrings.getString('ok'),
          localStrings.getString('cancel'),
          function() { chrome.send('deleteCertificate', [data.id]); });
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
      var readOnly = !!data && data.readonly;
      var hasChildren = this.tree.items.length > 0;
      this.viewButton.disabled = !isCert;
      if (this.editButton !== null)
        this.editButton.disabled = !isCert;
      if (this.backupButton !== null)
        this.backupButton.disabled = !isCert;
      if (this.backupAllButton !== null)
        this.backupAllButton.disabled = !hasChildren;
      if (this.exportButton !== null)
        this.exportButton.disabled = !isCert;
      this.deleteButton.disabled = !isCert || readOnly;
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
    OptionsPage.call(this, 'certificates',
                     templateData.certificateManagerPageTabTitle,
                     'certificateManagerPage');
  }

  cr.addSingletonGetter(CertificateManager);

  CertificateManager.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.personalTab = new CertificateManagerTab('personalCertsTab');
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

  CertificateManager.exportPersonalAskPassword = function(args) {
    CertificateBackupOverlay.show();
  };

  CertificateManager.importPersonalAskPassword = function(args) {
    CertificateRestoreOverlay.show();
  };

  // Export
  return {
    CertificateManagerTab: CertificateManagerTab,
    CertificateManager: CertificateManager
  };

});
