// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webstore API.

var webstoreNatives = requireNative('webstore');

function Installer() {
  this._pendingInstall = null;
}

Installer.prototype.install = function(url, onSuccess, onFailure) {
  if (this._pendingInstall) {
    throw 'A Chrome Web Store installation is already pending.';
  }
  var installId = webstoreNatives.Install(url, onSuccess, onFailure);
  if (installId !== undefined) {
    this._pendingInstall = {
      installId: installId,
      onSuccess: onSuccess,
      onFailure: onFailure
    };
  }
};

Installer.prototype.onInstallResponse = function(installId, success, error) {
  var pendingInstall = this._pendingInstall;
  if (!pendingInstall || pendingInstall.installId != installId) {
    // TODO(kalman): should this be an error?
    return;
  }

  try {
    if (success && pendingInstall.onSuccess)
      pendingInstall.onSuccess();
    else if (!success && pendingInstall.onFailure)
      pendingInstall.onFailure(error);
  } finally {
    this._pendingInstall = null;
  }
};

var installer = new Installer();

var chromeWebstore = {
  install: function install(url, onSuccess, onFailure) {
    installer.install(url, onSuccess, onFailure);
  }
};

// Called by webstore_binding.cc.
var chromeHiddenWebstore = {
  onInstallResponse: function(installId, success, error) {
    installer.onInstallResponse(installId, success, error);
  }
};

// These must match the names in InstallWebstorebinding in
// chrome/renderer/extensions/dispatcher.cc.
exports.chromeWebstore = chromeWebstore;
exports.chromeHiddenWebstore = chromeHiddenWebstore;
