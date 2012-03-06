// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  var webstoreNatives = requireNative('webstore');
  var Install = webstoreNatives.Install;

  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
  var pendingInstalls = {};
  chrome.webstore = new function() {
    this.install = function(
        preferredStoreLinkUrl, successCallback, failureCallback) {
      var installId = Install(
          preferredStoreLinkUrl, successCallback, failureCallback);
      if (installId !== undefined) {
        pendingInstalls[installId] = {
          successCallback: successCallback,
          failureCallback: failureCallback
        }
      }
    };
  }();

  chromeHidden.webstore = {};
  chromeHidden.webstore.onInstallResponse = function(
      installId, success, error) {
    if (!(installId in pendingInstalls)) {
      return;
    }

    var pendingInstall = pendingInstalls[installId];
    try {
      if (success && pendingInstall.successCallback) {
        pendingInstall.successCallback();
      } else if (!success && pendingInstall.failureCallback) {
        pendingInstall.failureCallback(error);
      }
    } finally {
      delete pendingInstall[installId];
    }
  }
