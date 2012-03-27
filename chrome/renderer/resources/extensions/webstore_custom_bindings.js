// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the webstore API.

var webstoreNatives = requireNative('webstore');

function Installer() {
  this._pendingInstalls = {};
}

Installer.prototype.install = function(url, onSuccess, onFailure) {
  var installId = webstoreNatives.Install(url, onSuccess, onFailure);
  if (installId !== undefined) {
    if (installId in this._pendingInstalls)
      throw new Error('Duplicate installId ' + installId);
    this._pendingInstalls[installId] = {
      onSuccess: onSuccess,
      onFailure: onFailure
    };
  }
};

Installer.prototype.onInstallResponse = function(installId, success, error) {
  var pendingInstall = this._pendingInstalls[installId];
  if (!pendingInstall) {
    // TODO(kalman): should this be an error?
    return;
  }

  try {
    if (success && pendingInstall.onSuccess)
      pendingInstall.onSuccess();
    else if (!success && pendingInstall.onFailure)
      pendingInstall.onFailure(error);
  } finally {
    delete this._pendingInstalls[installId];
  }
};

var installer = new Installer();

var chromeWebstore = {
  install: function install(url, onSuccess, onFailure) {
    installer.install(url, onSuccess, onFailure);
  }
};

// Called by webstore_bindings.cc.
var chromeHiddenWebstore = {
  onInstallResponse: function(installId, success, error) {
    installer.onInstallResponse(installId, success, error);
  }
};

// These must match the names in InstallWebstoreBindings in
// extension_dispatcher.cc.
exports.chromeWebstore = chromeWebstore;
exports.chromeHiddenWebstore = chromeHiddenWebstore;
