// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /** @interface */
  class NuxSetAsDefaultProxy {
  }

  /** @implements {NuxSetAsDefaultProxy} */
  class NuxSetAsDefaultProxyImpl {
  }

  cr.addSingletonGetter(NuxSetAsDefaultProxyImpl);

  return {
    NuxSetAsDefaultProxy: NuxSetAsDefaultProxy,
    NuxSetAsDefaultProxyImpl: NuxSetAsDefaultProxyImpl,
  };
});