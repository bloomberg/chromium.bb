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
    var methodNames = [
      'pageReady',
      'refreshUpdateStatus',
      'openHelpPage',
      'openFeedbackDialog',
    ];

    if (cr.isChromeOS) {
      methodNames.push(
        'getCurrentChannel',
        'getTargetChannel',
        'getVersionInfo',
        'getRegulatoryInfo');
    }

    settings.TestBrowserProxy.call(this, methodNames);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;

    if (cr.isChromeOS) {
      /** @type {!VersionInfo} */
      this.versionInfo_ = {
        arcVersion: '',
        osFirmware: '',
        osVersion: '',
      };

      /** @private {!BrowserChannel} */
      this.currentChannel_ = BrowserChannel.BETA;

      /** @private {!BrowserChannel} */
      this.targetChannel_ = BrowserChannel.BETA;

      /** @private {?RegulatoryInfo} */
      this.regulatoryInfo_ = null;
    }
  };

  TestAboutPageBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @param {!UpdateStatus} updateStatus */
    setUpdateStatus: function(updateStatus) {
      this.updateStatus_ = updateStatus;
    },

    /** @override */
    pageReady: function() {
      this.methodCalled('pageReady');
    },

    /** @override */
    refreshUpdateStatus: function() {
      cr.webUIListenerCallback(
          'update-status-changed', {status: this.updateStatus_});
      this.methodCalled('refreshUpdateStatus');
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

  if (cr.isChromeOS) {
    /** @param {!VersionInfo} */
    TestAboutPageBrowserProxy.prototype.setVersionInfo = function(versionInfo) {
      this.versionInfo_ = versionInfo;
    };

    /**
     * @param {!BrowserChannel} current
     * @param {!BrowserChannel} target
     */
    TestAboutPageBrowserProxy.prototype.setChannels = function(
        current, target) {
      this.currentChannel_ = current;
      this.targetChannel_ = target;
    };


    /** @param {?RegulatoryInfo} regulatoryInfo */
    TestAboutPageBrowserProxy.prototype.setRegulatoryInfo = function(
        regulatoryInfo) {
      this.regulatoryInfo_ = regulatoryInfo;
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getCurrentChannel = function() {
      this.methodCalled('getCurrentChannel');
      return Promise.resolve(this.currentChannel_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getTargetChannel = function() {
      this.methodCalled('getTargetChannel');
      return Promise.resolve(this.targetChannel_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getVersionInfo = function() {
      this.methodCalled('getVersionInfo');
      return Promise.resolve(this.versionInfo_);
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getRegulatoryInfo = function() {
      this.methodCalled('getRegulatoryInfo');
      return Promise.resolve(this.regulatoryInfo_);
    };
  }


  function registerAboutPageTests() {
    /** @param {!UpdateStatus} status */
    function fireStatusChanged(status) {
      cr.webUIListenerCallback('update-status-changed', {status: status});
    }

    suite('AboutPageTest', function() {
      var page = null;
      var browserProxy = null;

      setup(function() {
        browserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
        return initNewPage();
      });

      /** @return {!Promise} */
      function initNewPage() {
        browserProxy.reset();
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        document.body.appendChild(page);
        return browserProxy.whenCalled('refreshUpdateStatus');
      }

      /**
       * Test that the status icon updates according to incoming
       * 'update-status-changed' events.
       */
      test('IconUpdates', function() {
        var SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

        var icon = page.$$('iron-icon');
        assertTrue(!!icon);

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

      if (cr.isChromeOS) {
        /**
         * Test that all buttons update according to incoming
         * 'update-status-changed' events for the case where target and current
         * channel are the same.
         */
        test('ButtonsUpdate_SameChannel', function() {
          var relaunch = page.$.relaunch;
          var checkForUpdates = page.$.checkForUpdates;
          var relaunchAndPowerwash = page.$.relaunchAndPowerwash;

          assertTrue(!!relaunch);
          assertTrue(!!relaunchAndPowerwash);
          assertTrue(!!checkForUpdates);

          function assertAllHidden() {
            assertTrue(checkForUpdates.hidden);
            assertTrue(relaunch.hidden);
            assertTrue(relaunchAndPowerwash.hidden);
          }

          // Check that |UPDATED| status is ignored if the user has not
          // explicitly checked for updates yet.
          fireStatusChanged(UpdateStatus.UPDATED);
          assertFalse(checkForUpdates.hidden);
          assertTrue(relaunch.hidden);
          assertTrue(relaunchAndPowerwash.hidden);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertAllHidden();

          fireStatusChanged(UpdateStatus.UPDATING);
          assertAllHidden();

          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertTrue(checkForUpdates.hidden);
          assertFalse(relaunch.hidden);
          assertTrue(relaunchAndPowerwash.hidden);

          fireStatusChanged(UpdateStatus.UPDATED);
          assertAllHidden();

          fireStatusChanged(UpdateStatus.FAILED);
          assertFalse(checkForUpdates.hidden);
          assertTrue(relaunch.hidden);
          assertTrue(relaunchAndPowerwash.hidden);

          fireStatusChanged(UpdateStatus.DISABLED);
          assertAllHidden();

          fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
          assertAllHidden();
        });

        /**
         * Test that buttons update according to incoming
         * 'update-status-changed' events for the case where the target channel
         * is more stable than current channel.
         */
        test('ButtonsUpdate_BetaToStable', function() {
          browserProxy.setChannels(BrowserChannel.BETA, BrowserChannel.STABLE);
          browserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertTrue(!!page.$.relaunch);
            assertTrue(!!page.$.relaunchAndPowerwash);

            assertTrue(page.$.relaunch.hidden);
            assertFalse(page.$.relaunchAndPowerwash.hidden);
          });
        });

        /**
         * Test that buttons update according to incoming
         * 'update-status-changed' events for the case where the target channel
         * is less stable than current channel.
         */
        test('ButtonsUpdate_StableToBeta', function() {
          browserProxy.setChannels(BrowserChannel.STABLE, BrowserChannel.BETA);
          browserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertTrue(!!page.$.relaunch);
            assertTrue(!!page.$.relaunchAndPowerwash);

            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);
          });
        });

        test('RegulatoryInfo', function() {
          var regulatoryInfo = null;

          /**
           * Checks the visibility of the "regulatory info" section.
           * @param {boolean} isShowing Whether the section is expected to be
           *     visible.
           * @return {!Promise}
           */
          function checkRegulatoryInfo(isShowing) {
            return browserProxy.whenCalled('getRegulatoryInfo').then(
                function() {
                  var regulatoryInfoEl = page.$.regulatoryInfo;
                  assertTrue(!!regulatoryInfoEl);
                  assertEquals(isShowing, !regulatoryInfoEl.hidden);

                  if (isShowing) {
                    var img = regulatoryInfoEl.querySelector('img');
                    assertTrue(!!img);
                    assertEquals(regulatoryInfo.text, img.getAttribute('alt'));
                    assertEquals(regulatoryInfo.url, img.getAttribute('src'));
                  }
                });
          }

          return checkRegulatoryInfo(false).then(function() {
            regulatoryInfo = {text: 'foo', url: 'bar'};
            browserProxy.setRegulatoryInfo(regulatoryInfo);
            return initNewPage();
          }).then(function() {
            return checkRegulatoryInfo(true);
          });
        });
      }

      if (!cr.isChromeOS) {
        /*
         * Test that the "Check for updates" button updates according to
         * incoming 'update-status-changed' events.
         */
        test('ButtonsUpdate', function() {
          var relaunch = page.$.relaunch;
          assertTrue(!!relaunch);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.UPDATING);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertFalse(relaunch.hidden);

          fireStatusChanged(UpdateStatus.UPDATED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.FAILED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.DISABLED);
          assertTrue(relaunch.hidden);

          fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
          assertTrue(relaunch.hidden);
        });
      }

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
