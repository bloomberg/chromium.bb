// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_ntp_background_test', function() {
  suite('NuxNtpBackgroundTest', function() {
    /** @type {!Array<!nux.NtpBackgroundData} */
    let backgrounds = [
      {
        id: 0,
        title: 'Cat',
        image_url: 'some/cute/photo/of/a/cat',
      },
      {
        id: 1,
        title: 'Venice',
        image_url: 'some/scenic/photo/of/a/beach',
      },
    ];

    /** @type {NuxNtpBackgroundElement} */
    let testElement;

    /** @type {nux.NtpBackgroundProxy} */
    let testNtpBackgroundProxy;

    setup(function() {
      testNtpBackgroundProxy = new TestNtpBackgroundProxy();
      nux.NtpBackgroundProxyImpl.instance_ = testNtpBackgroundProxy;
      testNtpBackgroundProxy.setBackgroundsList(backgrounds);

      PolymerTest.clearBody();
      testElement = document.createElement('nux-ntp-background');
      document.body.appendChild(testElement);

      testElement.onRouteEnter();
      return Promise.all([
        testNtpBackgroundProxy.whenCalled('getBackgrounds'),
      ]);
    });

    teardown(function() {
      testElement.remove();
    });

    test('test placeholder test', function() {
      // TODO(johntlee): Add actual tests
      assertTrue(true);
    });
  });
});
