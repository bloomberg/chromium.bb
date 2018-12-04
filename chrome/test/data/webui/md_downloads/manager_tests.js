// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('manager tests', function() {
  const DOWNLOAD_DATA_TEMPLATE = Object.freeze({
    byExtId: '',
    byExtName: '',
    dangerType: downloads.DangerType.NOT_DANGEROUS,
    dateString: '',
    fileExternallyRemoved: false,
    filePath: '/some/file/path',
    fileName: 'download 1',
    fileUrl: 'file:///some/file/path',
    id: '',
    lastReasonText: '',
    otr: false,
    percent: 100,
    progressStatusText: '',
    resume: false,
    retry: false,
    return: false,
    sinceString: 'Today',
    started: Date.now() - 10000,
    state: downloads.States.COMPLETE,
    total: -1,
    url: 'http://permission.site',
  });

  /** @implements {mdDownloads.mojom.PageHandlerInterface} */
  class TestDownloadsProxy extends TestBrowserProxy {
    /** @param {!mdDownloads.mojom.PageHandlerCallbackRouter} */
    constructor(pageRouterProxy) {
      super(['remove']);

      /** @private {!mdDownloads.mojom.PageHandlerCallbackRouter} */
      this.pageRouterProxy_ = pageRouterProxy;
    }

    /** @override */
    remove(id) {
      this.pageRouterProxy_.removeItem(id);
      this.pageRouterProxy_.flushForTesting().then(
          () => this.methodCalled('remove', id));
    }

    /** @override */
    getDownloads(searchTerms) {}

    /** @override */
    openFileRequiringGesture(id) {}

    /** @override */
    drag(id) {}

    /** @override */
    saveDangerousRequiringGesture(id) {}

    /** @override */
    discardDangerous(id) {}

    /** @override */
    retryDownload(id) {}

    /** @override */
    show(id) {}

    /** @override */
    pause(id) {}

    /** @override */
    resume(id) {}

    /** @override */
    undo() {}

    /** @override */
    cancel(id) {}

    /** @override */
    clearAll() {}

    /** @override */
    openDownloadsFolderRequiringGesture() {}
  }

  /**
   * @param {Object=} config
   * @return {!downloads.Data}
   */
  function createDownload(config) {
    if (!config)
      config = {};

    return Object.assign({}, DOWNLOAD_DATA_TEMPLATE, config);
  }

  /** @type {!downloads.Manager} */
  let manager;

  /** @type {!mdDownloads.mojom.PageInterface} */
  let pageRouterProxy;

  /** @type {TestDownloadsProxy} */
  let testPageHandlerProxy;

  setup(function() {
    pageRouterProxy =
        downloads.BrowserProxy.getInstance().callbackRouter.createProxy();
    testPageHandlerProxy = new TestDownloadsProxy(pageRouterProxy);
    downloads.BrowserProxy.getInstance().handler = testPageHandlerProxy;

    PolymerTest.clearBody();
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
    let dangerousDownload = Object.assign({}, DOWNLOAD_DATA_TEMPLATE, {
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
          return testPageHandlerProxy.whenCalled('remove');
        })
        .then(() => {
          Polymer.dom.flush();
          const list = manager.$$('iron-list');
          assertTrue(list.hidden);
        });
  });
});
