// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for testing the Device page. */
cr.define('settings', function() {
  /** @interface */
  function DevicePageBrowserProxy() {}

  DevicePageBrowserProxy.prototype = {
    /**
     * Override to interact with the on-tap/on-keydown event on the Learn More
     * link.
     * @param {!Event} e
     */
    handleLinkEvent: function(e) {},
  };

  /**
   * @constructor
   * @implements {settings.DevicePageBrowserProxy}
   */
  function DevicePageBrowserProxyImpl() {}
  cr.addSingletonGetter(DevicePageBrowserProxyImpl);

  DevicePageBrowserProxyImpl.prototype = {
    /** override */
    handleLinkEvent: function(e) {
      // Prevent the link from activating its parent element when tapped or
      // when Enter is pressed.
      if (e.type != 'keydown' || e.keyCode == 13)
        e.stopPropagation();
    },
  };

  return {
    DevicePageBrowserProxy: DevicePageBrowserProxy,
    DevicePageBrowserProxyImpl: DevicePageBrowserProxyImpl,
  };
});
