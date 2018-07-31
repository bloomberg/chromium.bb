// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.DownloadsBrowserProxy} */
class TestDownloadsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initializeDownloads',
      'selectDownloadLocation',
      'resetAutoOpenFileTypes',
    ]);
  }

  /** @override */
  initializeDownloads() {
    this.methodCalled('initializeDownloads');
  }

  /** @override */
  selectDownloadLocation() {
    this.methodCalled('selectDownloadLocation');
  }

  /** @override */
  resetAutoOpenFileTypes() {
    this.methodCalled('resetAutoOpenFileTypes');
  }
}

let downloadsPage = null;

/** @type {?TestDownloadsBrowserProxy} */
let DownloadsBrowserProxy = null;

suite('DownloadsHandler', function() {
  setup(function() {
    DownloadsBrowserProxy = new TestDownloadsBrowserProxy();
    settings.DownloadsBrowserProxyImpl.instance_ = DownloadsBrowserProxy;

    PolymerTest.clearBody();

    downloadsPage = document.createElement('settings-downloads-page');
    document.body.appendChild(downloadsPage);

    // Page element must call 'initializeDownloads' upon attachment to the DOM.
    return DownloadsBrowserProxy.whenCalled('initializeDownloads');
  });

  teardown(function() {
    downloadsPage.remove();
  });

  test('select downloads location', function() {
    const button = downloadsPage.$$('#changeDownloadsPath');
    assertTrue(!!button);
    button.click();
    button.fire('transitionend');
    return DownloadsBrowserProxy.whenCalled('selectDownloadLocation');
  });

  test('openAdvancedDownloadsettings', function() {
    let button = downloadsPage.$$('#resetAutoOpenFileTypes');
    assertTrue(!button);

    cr.webUIListenerCallback('auto-open-downloads-changed', true);
    Polymer.dom.flush();
    button = downloadsPage.$$('#resetAutoOpenFileTypes');
    assertTrue(!!button);

    button.click();
    return DownloadsBrowserProxy.whenCalled('resetAutoOpenFileTypes')
        .then(function() {
          cr.webUIListenerCallback('auto-open-downloads-changed', false);
          Polymer.dom.flush();
          const button = downloadsPage.$$('#resetAutoOpenFileTypes');
          assertTrue(!button);
        });
  });

  if (cr.isChromeOS) {
    function setDefaultDownloadPathPref(downloadPath) {
      downloadsPage.prefs = {
        download: {
          default_directory: {
            key: 'download.default_directory',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: downloadPath,
          }
        }
      };
    }

    function getDefaultDownloadPathString() {
      const pathElement = downloadsPage.$$('#defaultDownloadPath');
      assertTrue(!!pathElement);
      return pathElement.textContent.trim();
    }

    test('rewrite default download paths.', function() {
      // Rewrite path for directories in Downloads volume.
      setDefaultDownloadPathPref('/home/chronos/u-0123456789abcdef/Downloads');
      assertEquals('Downloads', getDefaultDownloadPathString());

      // Rewrite path for directories in Google Drive.
      setDefaultDownloadPathPref('/special/drive-0123456789abcdef/root/foo');
      assertEquals('Google Drive \u203a foo', getDefaultDownloadPathString());

      // Rewrite path for directories in Android files volume.
      setDefaultDownloadPathPref('/run/arc/sdcard/write/emulated/0/foo/bar');
      assertEquals(
          'Play files \u203a foo \u203a bar', getDefaultDownloadPathString());
    });
  }
});
