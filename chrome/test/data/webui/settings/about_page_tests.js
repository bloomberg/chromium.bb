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
      'getTargetChannel',
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
    getTargetChannel: function() {
      this.methodCalled('getTargetChannel');
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
        return browserProxy.whenCalled('refreshUpdateStatus');
      });

      /**
       * Test that the status icon updates according to incoming
       * 'update-status-chanhed' events.
       */
      test('IconUpdates', function() {
        function fireStatusChanged(status) {
          cr.webUIListenerCallback('update-status-changed', {
            status: status, message: '', progress: 0,
          });
        }

        var SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

        var icon = page.$$('iron-icon');
        assertTrue(!!icon);
        assertEquals(null, icon.getAttribute('icon'));

        fireStatusChanged(UpdateStatus.CHECKING);
        assertEquals(SPINNER_ICON, icon.src);
        assertEquals(null, icon.getAttribute('icon'));

        fireStatusChanged(UpdateStatus.UPDATING);
        assertEquals(SPINNER_ICON, icon.src);
        assertEquals(null, icon.getAttribute('icon'));

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertEquals(null, icon.src);
        assertEquals('settings:check-circle', icon.icon);

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertEquals(null, icon.src);
        assertEquals('settings:check-circle', icon.icon);

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertEquals(null, icon.src);
        assertEquals('cr:domain', icon.icon);

        fireStatusChanged(UpdateStatus.FAILED);
        assertEquals(null, icon.src);
        assertEquals('settings:error', icon.icon);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertEquals(null, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
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
