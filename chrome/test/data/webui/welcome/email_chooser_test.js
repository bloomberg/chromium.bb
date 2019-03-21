// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_email_chooser', function() {
  suite('EmailChooserTest', function() {
    const emails = [
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
    ];

    /** @type {nux.NuxEmailProxy} */
    let testEmailBrowserProxy;

    /** @type {nux.ModuleMetricsProxy} */
    let testEmailMetricsProxy;

    /** @type {nux.BookmarkProxy} */
    let testBookmarkBrowserProxy;

    /** @type {EmailChooserElement} */
    let testElement;

    setup(async function() {
      testEmailBrowserProxy = new TestNuxEmailProxy();
      testEmailMetricsProxy = new TestMetricsProxy();
      testBookmarkBrowserProxy = new TestBookmarkProxy();
      nux.BookmarkProxyImpl.instance_ = testBookmarkBrowserProxy;
      // Reset w/ new proxy for test.
      nux.BookmarkBarManager.instance_ = new nux.BookmarkBarManager();

      testEmailBrowserProxy.setEmailList(emails);

      PolymerTest.clearBody();
      testElement = document.createElement('app-chooser');
      document.body.appendChild(testElement);
      testElement.singleSelect = true;  // Only allow selecting one email.
      testElement.appProxy = testEmailBrowserProxy;
      testElement.metricsManager =
          new nux.ModuleMetricsManager(testEmailMetricsProxy);
      // Simulate nux-email's onRouteEnter call.
      testElement.onRouteEnter();
      await testEmailMetricsProxy.whenCalled('recordPageShown');
      await testEmailBrowserProxy.whenCalled('getAppList');
    });

    teardown(function() {
      testElement.remove();
    });

    test('test email chooser options', async function() {
      const options = testElement.shadowRoot.querySelectorAll('.option');
      assertEquals(2, options.length);

      // First option is default selected and action button should be enabled.
      assertEquals(testElement.$$('.option[active]'), options[0]);
      assertFalse(testElement.$$('.action-button').disabled);

      options[0].click();

      assertEquals(
          1, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));

      assertFalse(!!testElement.$$('.option[active]'));
      assertTrue(testElement.$$('.action-button').disabled);

      // Click second option, it should be selected.
      testBookmarkBrowserProxy.reset();
      options[1].click();

      assertDeepEquals(
          {
            title: emails[1].name,
            url: emails[1].url,
            parentId: '1',
          },
          await testBookmarkBrowserProxy.whenCalled('addBookmark'));

      assertEquals(testElement.$$('.option[active]'), options[1]);
      assertFalse(testElement.$$('.action-button').disabled);

      // Click second option again, it should be deselected and action
      // button should be disabled.
      testBookmarkBrowserProxy.reset();
      options[1].click();

      assertEquals(
          2, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));

      assertFalse(!!testElement.$$('.option[active]'));
      assertTrue(testElement.$$('.action-button').disabled);
    });

    test('test email chooser skip button', async function() {
      const options = testElement.shadowRoot.querySelectorAll('.option');
      testElement.wasBookmarkBarShownOnInit_ = true;

      // First option should be selected and action button should be enabled.
      testElement.$.noThanksButton.click();

      await testEmailMetricsProxy.whenCalled('recordDidNothingAndChoseSkip');
      assertEquals(
          1, await testBookmarkBrowserProxy.whenCalled('removeBookmark'));
      assertEquals(
          true, await testBookmarkBrowserProxy.whenCalled('toggleBookmarkBar'));
    });

    test('test email chooser next button', async function() {
      const options = testElement.shadowRoot.querySelectorAll('.option');
      testElement.wasBookmarkBarShownOnInit_ = true;

      // First option should be selected and action button should be enabled.
      testElement.$$('.action-button').click();

      await testEmailMetricsProxy.whenCalled('recordDidNothingAndChoseNext');
      assertEquals(
          0, await testEmailBrowserProxy.whenCalled('recordProviderSelected'));
    });
  });
});
