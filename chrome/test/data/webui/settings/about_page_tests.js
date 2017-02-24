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
        'getChannelInfo',
        'getVersionInfo',
        'getRegulatoryInfo',
        'setChannel');
    }

    if (cr.isMac)
      methodNames.push('promoteUpdater');

    settings.TestBrowserProxy.call(this, methodNames);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;

    if (cr.isChromeOS) {
      /** @private {!VersionInfo} */
      this.versionInfo_ = {
        arcVersion: '',
        osFirmware: '',
        osVersion: '',
      };

      /** @private {!ChannelInfo} */
      this.channelInfo_ = {
        currentChannel: BrowserChannel.BETA,
        targetChannel: BrowserChannel.BETA,
        canChangeChannel: true,
      };

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

    sendStatusNoInternet: function() {
      cr.webUIListenerCallback('update-status-changed', {
        progress: 0,
        status: UpdateStatus.FAILED,
        message: 'offline',
        connectionTypes: 'no internet',
      });
    },

    /** @override */
    pageReady: function() { this.methodCalled('pageReady'); },

    /** @override */
    refreshUpdateStatus: function() {
      cr.webUIListenerCallback('update-status-changed', {
        progress: 1,
        status: this.updateStatus_,
      });
      this.methodCalled('refreshUpdateStatus');
    },

    /** @override */
    openFeedbackDialog: function() { this.methodCalled('openFeedbackDialog'); },

    /** @override */
    openHelpPage: function() { this.methodCalled('openHelpPage'); },
  };

  if (cr.isMac) {
    /** @override */
    TestAboutPageBrowserProxy.prototype.promoteUpdater = function() {
      this.methodCalled('promoteUpdater');
    };
  }

  if (cr.isChromeOS) {
    /** @param {!VersionInfo} */
    TestAboutPageBrowserProxy.prototype.setVersionInfo = function(versionInfo) {
      this.versionInfo_ = versionInfo;
    };

    /** @param {boolean} canChangeChannel */
    TestAboutPageBrowserProxy.prototype.setCanChangeChannel = function(
        canChangeChannel) {
      this.channelInfo_.canChangeChannel = canChangeChannel;
    };

    /**
     * @param {!BrowserChannel} current
     * @param {!BrowserChannel} target
     */
    TestAboutPageBrowserProxy.prototype.setChannels = function(
        current, target) {
      this.channelInfo_.currentChannel = current;
      this.channelInfo_.targetChannel = target;
    };

    /** @param {?RegulatoryInfo} regulatoryInfo */
    TestAboutPageBrowserProxy.prototype.setRegulatoryInfo = function(
        regulatoryInfo) {
      this.regulatoryInfo_ = regulatoryInfo;
    };

    /** @override */
    TestAboutPageBrowserProxy.prototype.getChannelInfo = function() {
      this.methodCalled('getChannelInfo');
      return Promise.resolve(this.channelInfo_);
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

    /** @override */
    TestAboutPageBrowserProxy.prototype.setChannel = function(
        channel, isPowerwashAllowed) {
      this.methodCalled('setChannel', [channel, isPowerwashAllowed]);
    };
  }


  function registerAboutPageTests() {
    /**
     * @param {!UpdateStatus} status
     * @param {{
     *   progress: number|undefined,
     *   message: string|undefined
     * }} opt_options
     */
    function fireStatusChanged(status, opt_options) {
      var options = opt_options || {};
      cr.webUIListenerCallback('update-status-changed', {
        progress: options.progress === undefined ? 1 : options.progress,
        message: options.message,
        status: status,
      });
    }

    suite('AboutPageTest', function() {
      var page = null;

      /** @type {?settings.TestAboutPageBrowserProxy} */
      var aboutBrowserProxy = null;

      /** @type {?settings.TestLifetimeBrowserProxy} */
      var lifetimeBrowserProxy = null;

      var SPINNER_ICON = 'chrome://resources/images/throbber_small.svg';

      setup(function() {
        lifetimeBrowserProxy = new settings.TestLifetimeBrowserProxy();
        settings.LifetimeBrowserProxyImpl.instance_ = lifetimeBrowserProxy;

        aboutBrowserProxy = new TestAboutPageBrowserProxy();
        settings.AboutPageBrowserProxyImpl.instance_ = aboutBrowserProxy;
        return initNewPage();
      });

      teardown(function() {
        page.remove();
        page = null;
        loadTimeData.overrideValues({
          aboutObsoleteNowOrSoon: false,
          aboutObsoleteEndOfTheLine: false,
        });
      });

      /** @return {!Promise} */
      function initNewPage() {
        aboutBrowserProxy.reset();
        lifetimeBrowserProxy.reset();
        PolymerTest.clearBody();
        page = document.createElement('settings-about-page');
        settings.navigateTo(settings.Route.ABOUT);
        document.body.appendChild(page);
        if (!cr.isChromeOS) {
          return aboutBrowserProxy.whenCalled('refreshUpdateStatus');
        } else {
          return Promise.all([
            aboutBrowserProxy.whenCalled('getChannelInfo'),
            aboutBrowserProxy.whenCalled('refreshUpdateStatus'),
          ]);
        }
      }

      /**
       * Test that the status icon and status message update according to
       * incoming 'update-status-changed' events.
       */
      test('IconAndMessageUpdates', function() {
        var icon = page.$$('iron-icon');
        assertTrue(!!icon);
        var statusMessageEl = page.$.updateStatusMessage;
        var previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.CHECKING);
        assertEquals(SPINNER_ICON, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 0});
        assertEquals(SPINNER_ICON, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertFalse(statusMessageEl.textContent.includes('%'));
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.UPDATING, {progress: 1});
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        assertTrue(statusMessageEl.textContent.includes('%'));
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertEquals(null, icon.src);
        assertEquals('settings:check-circle', icon.icon);
        assertNotEquals(previousMessageText, statusMessageEl.textContent);
        previousMessageText = statusMessageEl.textContent;

        fireStatusChanged(UpdateStatus.DISABLED_BY_ADMIN);
        assertEquals(null, icon.src);
        assertEquals('cr20:domain', icon.icon);
        assertEquals(0, statusMessageEl.textContent.trim().length);

        fireStatusChanged(UpdateStatus.FAILED);
        assertEquals(null, icon.src);
        assertEquals('settings:error', icon.icon);
        assertEquals(0, statusMessageEl.textContent.trim().length);

        fireStatusChanged(UpdateStatus.DISABLED);
        assertEquals(null, icon.src);
        assertEquals(null, icon.getAttribute('icon'));
        assertEquals(0, statusMessageEl.textContent.trim().length);
      });

      test('ErrorMessageWithHtml', function() {
        var htmlError = 'hello<br>there<br>was<pre>an</pre>error';
        fireStatusChanged(
            UpdateStatus.FAILED, {message: htmlError});
        var statusMessageEl = page.$.updateStatusMessage;
        assertEquals(htmlError, statusMessageEl.innerHTML);
      });

      /**
       * Test that when the current platform has been marked as deprecated (but
       * not end of the line) a deprecation warning message is displayed,
       * without interfering with the update status message and icon.
       */
      test('ObsoleteSystem', function() {
        loadTimeData.overrideValues({
          aboutObsoleteNowOrSoon: true,
          aboutObsoleteEndOfTheLine: false,
        });

        return initNewPage().then(function() {
          var icon = page.$$('iron-icon');
          assertTrue(!!icon);
          assertTrue(!!page.$.updateStatusMessage);
          assertTrue(!!page.$.deprecationWarning);

          assertFalse(page.$.deprecationWarning.hidden);
          // Update status message should be hidden before user has checked for
          // updates, on ChromeOS.
          assertEquals(cr.isChromeOS, page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertEquals(SPINNER_ICON, icon.src);
          assertEquals(null, icon.getAttribute('icon'));
          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.UPDATING);
          assertEquals(SPINNER_ICON, icon.src);
          assertEquals(null, icon.getAttribute('icon'));
          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertEquals(null, icon.src);
          assertEquals('settings:check-circle', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.updateStatusMessage.hidden);
        });
      });

      /**
       * Test that when the current platform has reached the end of the line, a
       * deprecation warning message and an error icon is displayed.
       */
      test('ObsoleteSystemEndOfLine', function() {
        loadTimeData.overrideValues({
          aboutObsoleteNowOrSoon: true,
          aboutObsoleteEndOfTheLine: true,
        });
        return initNewPage().then(function() {
          var icon = page.$$('iron-icon');
          assertTrue(!!icon);
          assertTrue(!!page.$.deprecationWarning);
          assertTrue(!!page.$.updateStatusMessage);

          assertFalse(page.$.deprecationWarning.hidden);
          assertFalse(page.$.deprecationWarning.hidden);

          fireStatusChanged(UpdateStatus.CHECKING);
          assertEquals(null, icon.src);
          assertEquals('settings:error', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertTrue(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.FAILED);
          assertEquals(null, icon.src);
          assertEquals('settings:error', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertTrue(page.$.updateStatusMessage.hidden);

          fireStatusChanged(UpdateStatus.UPDATED);
          assertEquals(null, icon.src);
          assertEquals('settings:error', icon.icon);
          assertFalse(page.$.deprecationWarning.hidden);
          assertTrue(page.$.updateStatusMessage.hidden);
        });
      });

      test('Relaunch', function() {
        var relaunch = page.$.relaunch;
        assertTrue(!!relaunch);
        assertTrue(relaunch.hidden);

        fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
        assertFalse(relaunch.hidden);

        var relaunch = page.$.relaunch;
        assertTrue(!!relaunch);
        MockInteractions.tap(relaunch);
        return lifetimeBrowserProxy.whenCalled('relaunch');
      });

      if (cr.isChromeOS) {
        test('NoInternet', function() {
          assertTrue(page.$.updateStatusMessage.hidden);
          aboutBrowserProxy.sendStatusNoInternet();
          Polymer.dom.flush();
          assertFalse(page.$.updateStatusMessage.hidden);
          assertNotEquals(
              page.$.updateStatusMessage.innerHTML.includes('no internet'));
        });

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
            // Ensure that when all buttons are hidden, the container is also
            // hidden.
            assertTrue(page.$.buttonContainer.hidden);
          }

          // Check that |UPDATED| status is ignored if the user has not
          // explicitly checked for updates yet.
          fireStatusChanged(UpdateStatus.UPDATED);
          assertFalse(checkForUpdates.hidden);
          assertTrue(relaunch.hidden);
          assertTrue(relaunchAndPowerwash.hidden);

          // Check that the "Check for updates" button gets hidden for certain
          // UpdateStatus values, even if the CHECKING state was never
          // encountered (for example triggering update from crosh command
          // line).
          fireStatusChanged(UpdateStatus.UPDATING);
          assertAllHidden();
          fireStatusChanged(UpdateStatus.NEARLY_UPDATED);
          assertTrue(checkForUpdates.hidden);
          assertFalse(relaunch.hidden);
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
          aboutBrowserProxy.setChannels(
              BrowserChannel.BETA, BrowserChannel.STABLE);
          aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertTrue(!!page.$.relaunch);
            assertTrue(!!page.$.relaunchAndPowerwash);

            assertTrue(page.$.relaunch.hidden);
            assertFalse(page.$.relaunchAndPowerwash.hidden);

            MockInteractions.tap(page.$.relaunchAndPowerwash);
            return lifetimeBrowserProxy.whenCalled('factoryReset');
          });
        });

        /**
         * Test that buttons update according to incoming
         * 'update-status-changed' events for the case where the target channel
         * is less stable than current channel.
         */
        test('ButtonsUpdate_StableToBeta', function() {
          aboutBrowserProxy.setChannels(
              BrowserChannel.STABLE, BrowserChannel.BETA);
          aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertTrue(!!page.$.relaunch);
            assertTrue(!!page.$.relaunchAndPowerwash);

            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);

            MockInteractions.tap(page.$.relaunch);
            return lifetimeBrowserProxy.whenCalled('relaunch');
          });
        });

        /**
         * Test that buttons update as a result of receiving a
         * 'target-channel-changed' event (normally fired from
         * <settings-channel-switcher-dialog>).
         */
        test('ButtonsUpdate_TargetChannelChangedEvent', function() {
          aboutBrowserProxy.setChannels(
              BrowserChannel.BETA, BrowserChannel.BETA);
          aboutBrowserProxy.setUpdateStatus(UpdateStatus.NEARLY_UPDATED);

          return initNewPage().then(function() {
            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);

            page.fire('target-channel-changed', BrowserChannel.DEV);
            assertFalse(page.$.relaunch.hidden);
            assertTrue(page.$.relaunchAndPowerwash.hidden);

            page.fire('target-channel-changed', BrowserChannel.STABLE);
            assertTrue(page.$.relaunch.hidden);
            assertFalse(page.$.relaunchAndPowerwash.hidden);
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
            return aboutBrowserProxy.whenCalled('getRegulatoryInfo').then(
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
            aboutBrowserProxy.setRegulatoryInfo(regulatoryInfo);
            return initNewPage();
          }).then(function() {
            return checkRegulatoryInfo(true);
          });
        });
      }

      if (!cr.isChromeOS) {
        /*
         * Test that the "Relaunch" button updates according to incoming
         * 'update-status-changed' events.
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
        return aboutBrowserProxy.whenCalled('openHelpPage');
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

      if (cr.isMac) {
        /**
         * A list of possible scenarios for the promoteUpdater.
         * @enum {!PromoteUpdaterStatus}
         */
        var PromoStatusScenarios = {
          CANT_PROMOTE: {
            hidden: true,
            disabled: true,
            actionable: false,
          },
          CAN_PROMOTE: {
            hidden: false,
            disabled: false,
            actionable: true,
          },
          IN_BETWEEN: {
            hidden: false,
            disabled: true,
            actionable: true,
          },
          PROMOTED: {
            hidden: false,
            disabled: true,
            actionable: false,
          },
        };

        /**
         * @param {!PromoteUpdaterStatus} status
         */
        function firePromoteUpdaterStatusChanged(status) {
          cr.webUIListenerCallback('promotion-state-changed', status);
        }

        /**
         * Tests that the button's states are wired up to the status correctly.
         */
        test('PromoteUpdaterButtonCorrectStates', function() {
          var item = page.$$('#promoteUpdater');
          var arrow = page.$$('#promoteUpdater button');
          assertFalse(!!item);
          assertFalse(!!arrow);

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CANT_PROMOTE);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          arrow = page.$$('#promoteUpdater button');
          assertFalse(!!item);
          assertFalse(!!arrow);

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
          Polymer.dom.flush();

          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertFalse(item.hasAttribute('disabled'));
          assertTrue(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater button');
          assertTrue(!!arrow);
          assertFalse(arrow.hidden);
          assertFalse(arrow.hasAttribute('disabled'));

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.IN_BETWEEN);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertTrue(item.hasAttribute('disabled'));
          assertTrue(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater button');
          assertTrue(!!arrow);
          assertFalse(arrow.hidden);
          assertTrue(arrow.hasAttribute('disabled'));

          firePromoteUpdaterStatusChanged(PromoStatusScenarios.PROMOTED);
          Polymer.dom.flush();
          item = page.$$('#promoteUpdater');
          assertTrue(!!item);
          assertTrue(item.hasAttribute('disabled'));
          assertFalse(item.hasAttribute('actionable'));

          arrow = page.$$('#promoteUpdater button');
          assertTrue(!!arrow);
          assertTrue(arrow.hidden);
          assertTrue(arrow.hasAttribute('disabled'));
        });

        test('PromoteUpdaterButtonWorksWhenEnabled', function() {
          firePromoteUpdaterStatusChanged(PromoStatusScenarios.CAN_PROMOTE);
          Polymer.dom.flush();
          var item = page.$$('#promoteUpdater');
          assertTrue(!!item);

          MockInteractions.tap(item);

          return browserProxy.whenCalled('promoteUpdater');
        });
      }
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

        teardown(function() {
          page.remove();
          page = null;
        });

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
            browserProxy.whenCalled('getChannelInfo'),
          ]).then(function() {
            assertEquals(versionInfo.arcVersion, page.$.arcVersion.textContent);
            assertEquals(versionInfo.osVersion, page.$.osVersion.textContent);
            assertEquals(versionInfo.osFirmware, page.$.osFirmware.textContent);
          });
        });

        /**
         * Checks whether the "change channel" button state (enabled/disabled)
         * correctly reflects whether the user is allowed to change channel (as
         * dictated by the browser via loadTimeData boolean).
         * @param {boolean} canChangeChannel Whether to simulate the case where
         *     changing channels is allowed.
         * @return {!Promise}
         */
        function checkChangeChannelButton(canChangeChannel) {
          browserProxy.setCanChangeChannel(canChangeChannel);
          page = document.createElement('settings-detailed-build-info');
          document.body.appendChild(page);
          return browserProxy.whenCalled('getChannelInfo').then(function() {
            var changeChannelButton = page.$$('paper-button');
            assertTrue(!!changeChannelButton);
            assertEquals(canChangeChannel, !changeChannelButton.disabled);
          });
        }

        test('ChangeChannel_Enabled', function() {
          return checkChangeChannelButton(true);
        });

        test('ChangeChannel_Disabled', function() {
          return checkChangeChannelButton(false);
        });
      });
    }

    function registerChannelSwitcherDialogTests() {
      suite('ChannelSwitcherDialogTest', function() {
        var dialog = null;
        var radioButtons = null;
        var browserProxy = null;
        var currentChannel = BrowserChannel.BETA;

        setup(function() {
          browserProxy = new TestAboutPageBrowserProxy();
          browserProxy.setChannels(currentChannel, currentChannel);
          settings.AboutPageBrowserProxyImpl.instance_ = browserProxy;
          PolymerTest.clearBody();
          dialog = document.createElement('settings-channel-switcher-dialog');
          document.body.appendChild(dialog);

          radioButtons = dialog.shadowRoot.querySelectorAll(
              'paper-radio-button');
          assertEquals(3, radioButtons.length);
          return browserProxy.whenCalled('getChannelInfo');
        });

        teardown(function() { dialog.remove(); });

        test('Initialization', function() {
          var radioGroup = dialog.$$('paper-radio-group');
          assertTrue(!!radioGroup);
          assertTrue(!!dialog.$.warning);
          assertTrue(!!dialog.$.changeChannel);
          assertTrue(!!dialog.$.changeChannelAndPowerwash);

          // Check that upon initialization the radio button corresponding to
          // the current release channel is pre-selected.
          assertEquals(currentChannel, radioGroup.selected);
          assertTrue(dialog.$.warning.hidden);

          // Check that action buttons are hidden when current and target
          // channel are the same.
          assertTrue(dialog.$.changeChannel.hidden);
          assertTrue(dialog.$.changeChannelAndPowerwash.hidden);
        });

        // Test case where user switches to a less stable channel.
        test('ChangeChannel_LessStable', function() {
          assertEquals(BrowserChannel.DEV, radioButtons.item(2).name);
          MockInteractions.tap(radioButtons.item(2));
          Polymer.dom.flush();

          return browserProxy.whenCalled('getChannelInfo').then(function() {
            assertFalse(dialog.$.warning.hidden);
            // Check that only the "Change channel" button becomes visible.
            assertTrue(dialog.$.changeChannelAndPowerwash.hidden);
            assertFalse(dialog.$.changeChannel.hidden);

            var whenTargetChannelChangedFired = test_util.eventToPromise(
                'target-channel-changed', dialog);

            MockInteractions.tap(dialog.$.changeChannel);
            return browserProxy.whenCalled('setChannel').then(function(args) {
              assertEquals(BrowserChannel.DEV, args[0]);
              assertFalse(args[1]);
              return whenTargetChannelChangedFired;
            }).then(function(event) {
              assertEquals(BrowserChannel.DEV, event.detail);
            });
          });
        });

        // Test case where user switches to a more stable channel.
        test('ChangeChannel_MoreStable', function() {
          assertEquals(BrowserChannel.STABLE, radioButtons.item(0).name);
          MockInteractions.tap(radioButtons.item(0));
          Polymer.dom.flush();

          return browserProxy.whenCalled('getChannelInfo').then(function() {
            assertFalse(dialog.$.warning.hidden);
            // Check that only the "Change channel and Powerwash" button becomes
            // visible.
            assertFalse(dialog.$.changeChannelAndPowerwash.hidden);
            assertTrue(dialog.$.changeChannel.hidden);

            var whenTargetChannelChangedFired = test_util.eventToPromise(
                'target-channel-changed', dialog);

            MockInteractions.tap(dialog.$.changeChannelAndPowerwash);
            return browserProxy.whenCalled('setChannel').then(function(args) {
              assertEquals(BrowserChannel.STABLE, args[0]);
              assertTrue(args[1]);
              return whenTargetChannelChangedFired;
            }).then(function(event) {
              assertEquals(BrowserChannel.STABLE, event.detail);
            });
          });
        });
      });
    }
  }

  return {
    registerTests: function() {
      if (cr.isChromeOS) {
        registerDetailedBuildInfoTests();
        registerChannelSwitcherDialogTests();
      }
      registerAboutPageTests();
    },
    registerOfficialBuildTests: registerOfficialBuildTests,
  };
});
