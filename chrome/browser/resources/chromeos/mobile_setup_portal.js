// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('mobile', function() {

  // TODO(tbarzic): Share code with mobile_setup.js.
  var EXTENSION_BASE_URL =
      'chrome-extension://iadeocfgjdjdmpenejdbfeaocpbikmab/';
  var REDIRECT_POST_PAGE_URL = EXTENSION_BASE_URL + 'redirect.html?autoPost=1';
  var PORTAL_OFFLINE_PAGE_URL = EXTENSION_BASE_URL + 'portal_offline.html';

  var NetworkState = {
    UNKNOWN: 0,
    PORTAL_REACHABLE: 1,
    PORTAL_UNREACHABLE: 2,
  };

  var localStrings = new LocalStrings();

  function PortalImpl() {
    // Mobile device information.
    this.deviceInfo_ = null;
    this.spinnerInt_ = -1;
    this.networkState_ = NetworkState.UNKNOWN;
    this.portalFrameSet_ = false;
  }

  cr.addSingletonGetter(PortalImpl);

  PortalImpl.prototype = {
    initialize: function() {
      $('carrierPage').contentWindow.location.href = PORTAL_OFFLINE_PAGE_URL;
      $('statusHeader').textContent =
          localStrings.getString('portal_unreachable_header');

      // Get network device info for which portal should be opened.
      // For LTE networks, this will also start observing network connection
      // state and raise |updatePortalReachability| messages when the portal
      // reachability changes.
      chrome.send('getDeviceInfo');
    },

    updateDeviceInfo: function(deviceInfo) {
      this.deviceInfo_ = deviceInfo;
      this.updateState_();
    },

    updateNetworkState: function(networkState) {
      if (this.networkState_ == networkState)
        return;
      this.networkState_ = networkState;

      // If the device info is not yet set, the state will be updated on the
      // device info update.
      if (this.deviceInfo_)
        this.updateState_();
    },

    updateState_: function() {
      if (!this.deviceInfo_ || this.networkState_ == NetworkState.UNKNOWN)
        return;

      if (this.networkState_ == NetworkState.PORTAL_REACHABLE) {
        // If the portal is reachable, set and show portalFrame; and hide system
        // status displaying 'offline portal' page.
        this.setPortalFrameIfNeeded_(this.deviceInfo_);
        $('portalFrame').hidden = false;
        $('systemStatus').hidden = true;
        this.stopSpinner_();
      } else {
        // If the portal is not reachable, hide portalFrame and show system
        // status displaying 'offline portal' page.
        $('portalFrame').hidden = true;
        $('systemStatus').hidden = false;
        this.startSpinner_();
      }
    },

    setPortalFrameIfNeeded_: function(deviceInfo) {
      // The portal should be set only once.
      if (this.portalFrameSet_)
        return;

      var postData = '';
      if (deviceInfo.post_data && deviceInfo.post_data.length)
        postData = '&post_data=' + encodeURIComponent(deviceInfo.post_data);

      $('portalFrame').contentWindow.location.href = REDIRECT_POST_PAGE_URL +
          postData + '&formUrl=' + encodeURIComponent(deviceInfo.payment_url);

      this.portalFrameSet_ = true;
    },

    startSpinner_: function() {
      this.stopSpinner_();
      this.spinnerInt_ = setInterval(this.drawProgress_.bind(this), 100);
    },

    stopSpinner_: function() {
      if (this.spinnerInt_ != -1) {
        clearInterval(this.spinnerInt_);
        this.spinnerInt_ = -1;
      }
    },

    drawProgress_: function() {
      var ctx = canvas.getContext('2d');
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      var segmentCount = Math.min(12, canvas.width / 1.6); // Number of segments
      var rotation = 0.75; // Counterclockwise rotation

      // Rotate canvas over time
      ctx.translate(canvas.width / 2, canvas.height / 2);
      ctx.rotate(Math.PI * 2 / (segmentCount + rotation));
      ctx.translate(-canvas.width / 2, -canvas.height / 2);

      var gap = canvas.width / 24; // Gap between segments
      var oRadius = canvas.width / 2; // Outer radius
      var iRadius = oRadius * 0.618; // Inner radius
      var oCircumference = Math.PI * 2 * oRadius; // Outer circumference
      var iCircumference = Math.PI * 2 * iRadius; // Inner circumference
      var oGap = gap / oCircumference; // Gap size as fraction of  outer ring
      var iGap = gap / iCircumference; // Gap size as fraction of  inner ring
      var oArc = Math.PI * 2 * (1 / segmentCount - oGap); // Angle of outer arcs
      var iArc = Math.PI * 2 * (1 / segmentCount - iGap); // Angle of inner arcs

      for (i = 0; i < segmentCount; i++) { // Draw each segment
        var opacity = Math.pow(1.0 - i / segmentCount, 3.0);
        opacity = (0.15 + opacity * 0.8); // Vary from 0.15 to 0.95
        var angle = - Math.PI * 2 * i / segmentCount;

        ctx.beginPath();
        ctx.arc(canvas.width / 2, canvas.height / 2, oRadius,
            angle - oArc / 2, angle + oArc / 2, false);
        ctx.arc(canvas.width / 2, canvas.height / 2, iRadius,
            angle + iArc / 2, angle - iArc / 2, true);
        ctx.closePath();
        ctx.fillStyle = 'rgba(240, 30, 29, ' + opacity + ')';
        ctx.fill();
      }
    }
  };

  function MobileSetupPortal() {}

  MobileSetupPortal.loadPage = function() {
    PortalImpl.getInstance().initialize();
  };

  MobileSetupPortal.onGotDeviceInfo = function(deviceInfo) {
    PortalImpl.getInstance().updateDeviceInfo(deviceInfo);
  };

  MobileSetupPortal.onConnectivityChanged = function(portalReachable) {
    PortalImpl.getInstance().updateNetworkState(
        portalReachable ? NetworkState.PORTAL_REACHABLE :
                          NetworkState.PORTAL_UNREACHABLE);
  };

  // Export
  return {
    MobileSetupPortal: MobileSetupPortal
  };
});

document.addEventListener('DOMContentLoaded',
                          mobile.MobileSetupPortal.loadPage);
