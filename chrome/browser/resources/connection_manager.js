// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function chromeos() {
}

chromeos.connectionManager = function() {
};

chromeos.connectionManager.device_info_callback_ = null;
chromeos.connectionManager.transaction_status_callback_ = null;
chromeos.connectionManager.parent_page_url_ = 'chrome://mobilesetup';

chromeos.connectionManager.getDeviceInfo = function(callback) {
  chromeos.connectionManager.device_info_callback_ = callback;
  chromeos.connectionManager.requestDeviceInfo_();
};

chromeos.connectionManager.setTransactionStatus = function(status, callback) {
  chromeos.connectionManager.transaction_status_callback_ = callback;
  chromeos.connectionManager.reportTransactionStatus_(status);
};

chromeos.connectionManager.reportTransactionStatus_ = function(status) {
  var msg = {
    'type': 'reportTransactionStatusMsg',
    'domain': location.href,
    'status': status
  };
  window.parent.postMessage(msg, chromeos.connectionManager.parent_page_url_);
};

chromeos.connectionManager.requestDeviceInfo_ = function() {
  var msg = {
    'type': 'requestDeviceInfoMsg',
    'domain': location.href,
  };
  window.parent.postMessage(msg, chromeos.connectionManager.parent_page_url_);
}

window.addEventListener('message', function(e) {
  if (e.data.type == 'deviceInfoMsg') {
    if (chromeos.connectionManager.device_info_callback_)
      chromeos.connectionManager.device_info_callback_(e.data.payload);
  }
});

