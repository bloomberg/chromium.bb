// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function showMessage(msg) {
  var area = $('message-area');
  var entry = document.createElement('div');
  entry.textContent = msg;
  area.appendChild(entry);
  window.setTimeout(function() {
      area.removeChild(entry);
    }, 3000);
}

function onPageLoad() {
  var networkConfig = $('network-config');
  network.config.NetworkConfig.decorate(networkConfig);

  $('close').onclick = function() {
    networkConfig.applyUserSettings();
  };

  $('connect').onclick = function() {
    chrome.networkingPrivate.startConnect(
        networkConfig.networkId,
        function() {
          showMessage('Successfully requested connect to ' +
              networkConfig.networkId + '!');
        });
  };

  $('disconnect').onclick = function() {
    chrome.networkingPrivate.startDisconnect(
        networkConfig.networkId,
        function() {
          showMessage('Successfully requested disconnect from ' +
              networkConfig.networkId + '!');
        });
  };
}

document.addEventListener('DOMContentLoaded', onPageLoad);
