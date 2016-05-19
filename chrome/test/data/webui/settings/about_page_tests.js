// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_about_page', function() {
  /**
   * @constructor
   * @implements {settings.AboutPageBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestAboutPageBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'pageReady',
      'refreshUpdateStatus',
      'openHelpPage',
      'openFeedbackDialog',
      'getCurrentChannel',
      'getVersionInfo',
    ]);

    /** @type {!VersionInfo} */
    this.versionInfo_ = {
      arcVersion: '',
      osFirmware: '',
      osVersion: '',
    };
  };

  TestAboutPageBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @param {!VersionInfo} */
    setVersionInfo: function(versionInfo) {
      this.versionInfo_ = versionInfo;
    },

    /** @override */
    pageReady: function() {
      this.methodCalled('pageReady');
    },

    /** @override */
    refreshUpdateStatus: function() {
      this.methodCalled('refreshUpdateStatus');
    },

    /** @override */
    getCurrentChannel: function() {
      this.methodCalled('getCurrentChannel');
      return Promise.resolve(BrowserChannel.BETA);
    },

    /** @override */
    getVersionInfo: function() {
      this.methodCalled('getVersionInfo');
      return Promise.resolve(this.versionInfo_);
    },

    /** @override */
    openFeedbackDialog: function() {
      this.methodCalled('openFeedbackDialog');
    },

    /** @override */
    openHelpPage: function() {
      this.methodCalled('openHelpPage');
    },
  };

  function registerAboutPageTests() {
    suite('AboutPageTest', function() {
      var page = null;
      var browserProxy = null;

      setup(function() {
        browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        document.body.appendChild(page);
      });

      test('GetHelp', function() {
        assertTrue(!!page.$.help);
        MockInteractions.tap(page.$.help);
        return browserProxy.whenCalled('openHelpPage');
      });
    });
  }

  function registerOfficialBuildTests() {
    suite('AboutPageTest_OfficialBuild', function() {
      var page = null;
      var browserProxy = null;

      setup(function() {
        browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        document.body.appendChild(page);
      });

      test('ReportAnIssue', function() {
        assertTrue(!!page.$.reportIssue);
        MockInteractions.tap(page.$.reportIssue);
        return browserProxy.whenCalled('openFeedbackDialog');
      });
    });
  }

  if (cr.isChromeOS) {
    function registerDetailedBuildInfoTests() {
      suite('DetailedBuildInfoTest', function() {
        var page = null;
        var browserProxy = null;

        setup(function() {
          browserProxy = new TestAboutPageBrowserProxy();
          settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
          PolymerTest.clearBody();
        });

        teardown(function() { page.remove(); });

        test('Initialization', function() {
          var versionInfo = {
            arcVersion: 'dummyArcVersion',
            osFirmware: 'dummyOsFirmware',
            osVersion: 'dummyOsVersion',
          };
          browserProxy.setVersionInfo(versionInfo);

          page = document.createElement('settings-detailed-build-info');
          document.body.appendChild(page);

          return Promise.all([
            browserProxy.whenCalled('pageReady'),
            browserProxy.whenCalled('getVersionInfo'),
            browserProxy.whenCalled('getCurrentChannel'),
          ]).then(function() {
            assertEquals(versionInfo.arcVersion, page.$.arcVersion.textContent);
            assertEquals(versionInfo.osVersion, page.$.osVersion.textContent);
            assertEquals(versionInfo.osFirmware, page.$.osFirmware.textContent);
          });
        });
      });
    }
  }

  return {
    registerTests: function() {
      if (cr.isChromeOS) {
        registerDetailedBuildInfoTests();
      }
      registerAboutPageTests();
    },
    registerOfficialBuildTests: registerOfficialBuildTests,
  };
});
