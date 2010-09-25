// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('mobile', function() {

  function MobileSetup() {
  }

  cr.addSingletonGetter(MobileSetup);

  MobileSetup.prototype = {
    // Mobile device information.
    deviceInfo_: null,
    frame_name_ : '',

    initialize: function(frame_name) {
      self = this;
      this.frame_name_ = frame_name;
      document.addEventListener('DOMContentLoaded', function(deviceInfo) {
          chrome.send('getDeviceInfo',
                      ['mobile.MobileSetup.getDeviceInfoCallback']);
      });
      window.addEventListener('message', function(e) {
          self.onMessageReceived_(e);
      });
    },

    loadPaymentFrame: function(deviceInfo) {
      if (deviceInfo) {
        this.deviceInfo_ = deviceInfo;

        // HACK(zelidrag): Remove the next line for real testing.
        this.deviceInfo_.payment_url = 'http://link.to/mobile_activation.html';
            
        $(this.frame_name_).contentWindow.location.href =
            this.deviceInfo_.payment_url;
      }
    },

    onMessageReceived_: function(e) {
      if (e.origin !=
            this.deviceInfo_.payment_url.substring(0, e.origin.length))
        return;

      if (e.data.type == 'requestDeviceInfoMsg') {
        this.sendDeviceInfo_();
      } else if (e.data.type == 'reportTransactionStatusMsg') {
        chrome.send('setTransactionStatus', [e.data.status]);
      }
    },

    sendDeviceInfo_ : function() {
      var msg = {
        type: 'deviceInfoMsg',
        domain: document.location,
        payload: {
          'carrier': this.deviceInfo_.carrier,
          'MEID': this.deviceInfo_.MEID,
          'IMEI': this.deviceInfo_.IMEI,
          'IMSI': this.deviceInfo_.IMSI,
          'ESN': this.deviceInfo_.ESN,
          'MDN': this.deviceInfo_.MDN
        }
      };
      $(this.frame_name_).contentWindow.postMessage(msg,
          this.deviceInfo_.payment_url);
    }
  };

  MobileSetup.getDeviceInfoCallback = function(deviceInfo) {
    MobileSetup.getInstance().loadPaymentFrame(deviceInfo);
  };

  // Export
  return {
    MobileSetup: MobileSetup
  };

});

