// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('manager tests', function() {
  /** @type {!downloads.Manager} */
  let manager;

  /** @type {!downloads.mojom.PageHandlerCallbackRouter} */
  let pageRouterProxy;

  /** @type {TestDownloadsProxy} */
  let testBrowserProxy;

  setup(function() {
    PolymerTest.clearBody();

    testBrowserProxy = new TestDownloadsProxy();
    pageRouterProxy = testBrowserProxy.pageRouterProxy;
    downloads.BrowserProxy.instance_ = testBrowserProxy;

    manager = document.createElement('downloads-manager');
    document.body.appendChild(manager);
    assertEquals(manager, downloads.Manager.get());
  });

  test('long URLs elide', function() {
    pageRouterProxy.insertItems(0, [createDownload({
                                  fileName: 'file name',
                                  state: downloads.States.COMPLETE,
                                  sinceString: 'Today',
                                  url: 'a'.repeat(1000),
                                })]);
    return pageRouterProxy.flushForTesting().then(() => {
      Polymer.dom.flush();

      const item = manager.$$('downloads-item');
      assertLT(item.$$('#url').offsetWidth, item.offsetWidth);
      assertEquals(300, item.$$('#url').textContent.length);
    });
  });

  test('inserting items at beginning render dates correctly', function() {
    const countDates = () => {
      const items = manager.shadowRoot.querySelectorAll('downloads-item');
      return Array.from(items).reduce((soFar, item) => {
        return item.$$('h3[id=date]:not(:empty)') ? soFar + 1 : soFar;
      }, 0);
    };

    let download1 = createDownload();
    let download2 = createDownload();

    pageRouterProxy.insertItems(0, [download1, download2]);
    return pageRouterProxy.flushForTesting()
        .then(() => {
          Polymer.dom.flush();
          assertEquals(1, countDates());

          pageRouterProxy.removeItem(0);
          return pageRouterProxy.flushForTesting();
        })
        .then(() => {
          Polymer.dom.flush();
          assertEquals(1, countDates());

          pageRouterProxy.insertItems(0, [download1]);
          return pageRouterProxy.flushForTesting();
        })
        .then(() => {
          Polymer.dom.flush();
          assertEquals(1, countDates());
        });
  });

  test('update', function() {
    let dangerousDownload = createDownload({
      dangerType: downloads.DangerType.DANGEROUS_FILE,
      state: downloads.States.DANGEROUS,
    });
    pageRouterProxy.insertItems(0, [dangerousDownload]);
    return pageRouterProxy.flushForTesting()
        .then(() => {
          Polymer.dom.flush();
          assertTrue(!!manager.$$('downloads-item').$$('.dangerous'));

          let safeDownload = Object.assign({}, dangerousDownload, {
            dangerType: downloads.DangerType.NOT_DANGEROUS,
            state: downloads.States.COMPLETE,
          });
          pageRouterProxy.updateItem(0, safeDownload);
          return pageRouterProxy.flushForTesting();
        })
        .then(() => {
          Polymer.dom.flush();
          assertFalse(!!manager.$$('downloads-item').$$('.dangerous'));
        });
  });

  test('remove', () => {
    pageRouterProxy.insertItems(0, [createDownload({
                                  fileName: 'file name',
                                  state: downloads.States.COMPLETE,
                                  sinceString: 'Today',
                                  url: 'a'.repeat(1000),
                                })]);
    return pageRouterProxy.flushForTesting()
        .then(() => {
          Polymer.dom.flush();
          const item = manager.$$('downloads-item');

          item.$.remove.click();
          return testBrowserProxy.handler.whenCalled('remove');
        })
        .then(() => {
          Polymer.dom.flush();
          const list = manager.$$('iron-list');
          assertTrue(list.hidden);
        });
  });

  test('loadTimeData contains isManaged and managedByOrg', function() {
    // Check that loadTimeData contains these values.
    loadTimeData.getBoolean('isManaged');
    loadTimeData.getString('managedByOrg');
  });
});
