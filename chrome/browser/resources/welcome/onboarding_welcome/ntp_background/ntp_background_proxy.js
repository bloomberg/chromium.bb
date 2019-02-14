// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /**
   * @typedef {{
   *   id: number,
   *   imageUrl: string,
   *   thumbnailClass: string,
   *   title: string,
   * }}
   */
  let NtpBackgroundData;

  /** @interface */
  class NtpBackgroundProxy {
    /** @return {!Promise<!Array<!nux.NtpBackgroundData>>} */
    getBackgrounds() {}
  }

  /** @implements {nux.NtpBackgroundProxy} */
  class NtpBackgroundProxyImpl {
    /** @override */
    getBackgrounds() {
      return cr.sendWithPromise('getBackgrounds');
    }
  }

  cr.addSingletonGetter(NtpBackgroundProxyImpl);

  return {
    NtpBackgroundData: NtpBackgroundData,
    NtpBackgroundProxy: NtpBackgroundProxy,
    NtpBackgroundProxyImpl: NtpBackgroundProxyImpl,
  };
});
