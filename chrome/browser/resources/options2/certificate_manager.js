// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
        chrome.send('exportAllPersonalCertificates');
      }
    }

    this.importButton = $(id + '-import');
    if (this.importButton !== null) {
      if (id == 'personalCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importPersonalCertificate', [false]);
        }
      } else if (id == 'serverCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importServerCertificate');
        }
      } else if (id == 'caCertsTab') {
        this.importButton.onclick = function(e) {
          chrome.send('importCaCertificate');
        }
      }
    }

    this.importAndBindButton = $(id + '-import-and-bind');
    if (this.importAndBindButton !== null) {
      if (id == 'personalCertsTab') {
        this.importAndBindButton.onclick = function(e) {
          chrome.send('importPersonalCertificate', [true]);
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
          loadTimeData.getStringF(id + 'DeleteConfirm', data.name),
          loadTimeData.getString(id + 'DeleteImpact'),
          loadTimeData.getString('ok'),
          loadTimeData.getString('cancel'),
          function() {
            tree.selectedItem = null;
            chrome.send('deleteCertificate', [data.id]);
          });
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
  };

  // TODO(xiyuan): Use notification from backend instead of polling.
  // TPM token check polling timer.
  var tpmPollingTimer;

  // Initiate tpm token check if needed.
  function checkTpmToken() {
    var importAndBindButton = $('personalCertsTab-import-and-bind');

    if (importAndBindButton && importAndBindButton.disabled)
      chrome.send('checkTpmTokenReady');
  }

  // Stop tpm polling timer.
  function stopTpmTokenCheckPolling() {
    if (tpmPollingTimer) {
      window.clearTimeout(tpmPollingTimer);
      tpmPollingTimer = undefined;
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // CertificateManager class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function CertificateManager(model) {
    OptionsPage.call(this, 'certificates',
                     loadTimeData.getString('certificateManagerPageTabTitle'),
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

      $('certificate-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };
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
        OptionsPage.showTab($('personal-certs-nav-tab'));
        chrome.send('populateCertificateManager');
      }

      if (cr.isChromeOS) {
        // Ensure TPM token check on visible and stop polling when hidden.
        if (this.visible)
          checkTpmToken();
        else
          stopTpmTokenCheckPolling();
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

  CertificateManager.onCheckTpmTokenReady = function(ready) {
    var importAndBindButton = $('personalCertsTab-import-and-bind');
    if (importAndBindButton) {
      importAndBindButton.disabled = !ready;

      // Check again after 5 seconds if Tpm is not ready and certificate manager
      // is still visible.
      if (!ready && CertificateManager.getInstance().visible)
        tpmPollingTimer = window.setTimeout(checkTpmToken, 5000);
    }
  };

  // Export
  return {
    CertificateManagerTab: CertificateManagerTab,
    CertificateManager: CertificateManager
  };
});
