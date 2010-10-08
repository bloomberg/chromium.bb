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
    local_strings_: new LocalStrings();
    // UI states.
    state_ : 0,
    STATE_UNKNOWN_: "unknown",
    STATE_CONNECTING_: "connecting",
    STATE_ERROR_: "error",
    STATE_PAYMENT_: "payment",
    STATE_ACTIVATING_: "activating",
    STATE_CONNECTED_: "connected",

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

    changeState: function(new_state, errorText) {
      if (state_ == new_state)
        return;
      if (new_state == STATE_UNKNOWN_)
        document.body.setAttribute('state', STATE_CONNECTING_);
      else
        document.body.setAttribute('state', new_state);
      switch(new_state) {
        case STATE_UNKNOWN_:
        case STATE_CONNECTING_:
          $('status-header').textContent =
              local_strings_.getString('connecting_header');
          $('error-message').textContent = '';
          break;
        case STATE_ERROR_:
          $('status-header').textContent =
              local_strings_.getString('error_header');
          $('error-message').textContent = errorText;
          break;
        case STATE_ACTIVATING_:
          $('status-header').textContent =
              local_strings_.getString('activating_header');
          $('error-message').textContent = '';
          break;
      }
      state_ = new_state;
    },

    updateDeviceStatus_: function(deviceInfo) {
      this.changeState(deviceInfo.state, deviceInfo.error);
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

  MobileSetup.drawProgress = function () {
    var canvas = $('wheel');
    var ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    var segmentCount = Math.min(12, canvas.width/1.6) // Number of segments
    var rotation = 0.75; // Counterclockwise rotation

    // Rotate canvas over time
    ctx.translate(canvas.width/2, canvas.height/2);
    ctx.rotate(Math.PI * 2 / (segmentCount + rotation));
    ctx.translate(-canvas.width/2, -canvas.height/2);

    var gap = canvas.width / 24; // Gap between segments
    var oRadius = canvas.width/2; // Outer radius
    var iRadius = oRadius * 0.618; // Inner radius
    var oCircumference = Math.PI * 2 * oRadius; // Outer circumference
    var iCircumference = Math.PI * 2 * iRadius; // Inner circumference
    var oGap = gap / oCircumference; // Gap size as fraction of  outer ring
    var iGap = gap / iCircumference; // Gap size as fraction of  inner ring
    var oArc = Math.PI * 2 * ( 1 / segmentCount - oGap); // Angle of outer arcs
    var iArc = Math.PI * 2 * ( 1 / segmentCount - iGap); // Angle of inner arcs

    for (i = 0; i < segmentCount; i++){ // Draw each segment
      var opacity = Math.pow(1.0 - i / segmentCount, 3.0);
      opacity = (0.15 + opacity * 0.8) // Vary from 0.15 to 0.95
      var angle = - Math.PI * 2 * i / segmentCount;

      ctx.beginPath();
      ctx.arc(canvas.width/2, canvas.height/2, oRadius,
        angle - oArc/2, angle + oArc/2, false);
      ctx.arc(canvas.width/2, canvas.height/2, iRadius,
        angle + iArc/2, angle - iArc/2, true);
      ctx.closePath();
      ctx.fillStyle = "rgba(240, 30, 29, " + opacity + ")";
      ctx.fill();
    }
  };

  MobileSetup.getDeviceInfoCallback = function(deviceInfo) {
    MobileSetup.getInstance().loadPaymentFrame(deviceInfo);
  };


  MobileSetup.deviceStateChanged = function(deviceInfo) {
    MobileSetup.getInstance().updateDeviceStatus_(deviceInfo);
  };

  // Export
  return {
    MobileSetup: MobileSetup
  };

});

