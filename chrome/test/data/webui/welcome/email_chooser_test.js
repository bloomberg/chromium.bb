// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_email_chooser', function() {
  suite('EmailChooserTest', function() {
    /** @type {nux.NuxEmailProxy} */
    let testEmailBrowserProxy;

    /** @type {EmailChooserElement} */
    let testElement;

    setup(function() {
      testEmailBrowserProxy = new TestNuxEmailProxy();
      nux.NuxEmailProxyImpl.instance_ = testEmailBrowserProxy;

      testEmailBrowserProxy.setEmailList([
        {
          id: 0,
          name: 'Gmail',
          icon: 'gmail',
          url: 'http://www.gmail.com',
        },
        {
          id: 1,
          name: 'Yahoo',
          icon: 'yahoo',
          url: 'http://mail.yahoo.com',
        }
      ]);

      PolymerTest.clearBody();
      testElement = document.createElement('email-chooser');
      document.body.appendChild(testElement);
      return Promise.all([
        testEmailBrowserProxy.whenCalled('recordPageInitialized'),
        testEmailBrowserProxy.whenCalled('getEmailList'),
      ]);
    });

    teardown(function() {
      testElement.remove();
    });

    test('test email chooser initialized options correctly', function() {
      assertEquals(
          2, testElement.shadowRoot.querySelectorAll('.option').length);

      // TODO(scottchen): Add more initial layout testing.
    });
  });
});
