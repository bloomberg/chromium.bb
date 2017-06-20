// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AlertOverlay = options.AlertOverlay;
var OptionsPage = options.OptionsPage;
var PageManager = cr.ui.pageManager.PageManager;
var CertificateManager = options.CertificateManager;
var CertificateRestoreOverlay = options.CertificateRestoreOverlay;
var CertificateBackupOverlay = options.CertificateBackupOverlay;
var CertificateEditCaTrustOverlay = options.CertificateEditCaTrustOverlay;
var CertificateImportErrorOverlay = options.CertificateImportErrorOverlay;

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  if (cr.isChromeOS)
    document.documentElement.setAttribute('os', 'chromeos');

  // Setup tab change handers.
  var subpagesNavTabs = document.querySelectorAll('.subpages-nav-tabs');
  for (var i = 0; i < subpagesNavTabs.length; i++) {
    subpagesNavTabs[i].onclick = function(event) {
      OptionsPage.showTab(event.srcElement);
    };
  }

  // Shake the dialog if the user clicks outside the dialog bounds.
  var containers = [$('overlay-container-2')];
  for (var i = 0; i < containers.length; i++) {
    var overlay = containers[i];
    cr.ui.overlay.setupOverlay(overlay);
    overlay.addEventListener(
        'cancelOverlay', PageManager.cancelOverlay.bind(PageManager));
  }

  // Hide elements that should not be part of the dialog.
  $('certificate-confirm').hidden = true;
  $('cert-manager-header').hidden = true;

  PageManager.isDialog = true;
  CertificateManager.getInstance().setIsKiosk(true);
  CertificateManager.getInstance().initializePage();
  PageManager.registerOverlay(
      AlertOverlay.getInstance(), CertificateManager.getInstance());
  PageManager.registerOverlay(
      CertificateBackupOverlay.getInstance(), CertificateManager.getInstance());
  PageManager.registerOverlay(
      CertificateEditCaTrustOverlay.getInstance(),
      CertificateManager.getInstance());
  PageManager.registerOverlay(
      CertificateImportErrorOverlay.getInstance(),
      CertificateManager.getInstance());
  PageManager.registerOverlay(CertificateManager.getInstance());
  PageManager.registerOverlay(
      CertificateRestoreOverlay.getInstance(),
      CertificateManager.getInstance());

  PageManager.showPageByName('certificates', false);
}

disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
      src instanceof HTMLInputElement && /text|url/.test(src.type);
});

document.addEventListener('DOMContentLoaded', load);
