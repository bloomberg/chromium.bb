// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on loaded Mac kernel extensions.
 *
 * Displays the output from running kexstat.
 *
 * @constructor
 */
function KernelExtensionsView(tabId, mainBoxId, textPReId) {
  DivView.call(this, mainBoxId);

  var tab = document.getElementById(tabId);
  setNodeDisplay(tab, true);
  this.textPre_ = document.getElementById(textPReId);

  g_browser.addKernelExtensionsObserver(this);
}

inherits(KernelExtensionsView, DivView);

KernelExtensionsView.prototype.onKernelExtensionsChanged =
function(kernelExtensionsText) {
  this.textPre_.innerHTML = '';
  if (kernelExtensionsText == null) {
    addTextNode(this.textPre_, 'No data');
  } else {
    addTextNode(this.textPre_, kernelExtensionsText);
  }
};
