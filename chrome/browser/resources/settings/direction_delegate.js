// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  class DirectionDelegate {
    /**
     * @return {boolean} Whether the direction of the settings UI is
     *     right-to-left.
     */
    isRtl() {}
  }

  /** @implements {settings.DirectionDelegate} */
  class DirectionDelegateImpl {
    /** @override */
    isRtl() {
      return loadTimeData.getString('textdirection') == 'rtl';
    }
  }

  return {
    DirectionDelegate: DirectionDelegate,
    DirectionDelegateImpl: DirectionDelegateImpl,
  };
});
