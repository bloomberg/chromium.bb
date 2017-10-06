// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-item. */
cr.define('extension_service_tests', function() {
  /** @constructor */
  function ChangeListener() {}

  ChangeListener.prototype = {
    /**
     * @param {!ChromeEvent} chromeEvent
     * @param {!function(...):boolean} matchFunction
     */
    init_: function(chromeEvent, matchFunction) {
      this.onUpdate = new Promise(function(resolve, reject) {
        this.resolvePromise_ = resolve;
      }.bind(this));
      this.listener_ = this.onChanged_.bind(this);
      this.event_ = chromeEvent;
      this.matches_ = matchFunction;
      this.event_.addListener(this.listener_);
    },

    /** @private */
    onChanged_: function() {
      if (this.matches_.apply(null, arguments)) {
        setTimeout(function() {
          this.event_.removeListener(this.listener_);
          this.resolvePromise_();
        }.bind(this));
      }
    },
  };

  /**
   * @param {string} id The id of the item to watch.
   * @param {chrome.developerPrivate.EventType} eventType The type of event
   *     to wait for.
   * @extends {extension_service_tests.ChangeListener}
   * @constructor
   */
  function ItemChangedListener(id, eventType) {
    this.__proto__ = ChangeListener.prototype,
    this.init_(chrome.developerPrivate.onItemStateChanged,
               function(data) {
      return data.event_type == eventType && data.item_id == id;
    });
  }

  /**
   * @extends {extension_service_tests.ChangeListener}
   * @constructor
   */
  function ProfileChangedListener() {
    this.__proto__ = ChangeListener.prototype,
    this.init_(chrome.developerPrivate.onProfileStateChanged,
               function() { return true; });
  }

  /** @enum {string} */
  var TestNames = {
    EnableAndDisable: 'enable and disable',
    ToggleIncognitoMode: 'toggle incognito mode',
    Uninstall: 'uninstall',
    ProfileSettings: 'profile settings',
  };

  suite('ExtensionServiceTest', function() {
    /** @const{string} */
    var kExtensionId = 'ldnnhddmnhbkjipkidpdiheffobcpfmf';

    /** @const */
    var EventType = chrome.developerPrivate.EventType;

    /** @const */
    var ExtensionState = chrome.developerPrivate.ExtensionState;

    /** @type {extensions.Service} */
    var service;

    /** @type {extensions.Manager} */
    var manager;

    var getItemData = function(id) {
      var elMatches = function(el) {
        return el.id == id;
      };
      return manager.extensions.find(elMatches) || manager.apps.find(elMatches);
    };

    // Initialize an extension item before each test.
    setup(function() {
      service = extensions.Service.getInstance();
      manager = document.getElementsByTagName('extensions-manager')[0];
    });

    test(assert(TestNames.EnableAndDisable), function(done) {
      var item = getItemData(kExtensionId);
      assertTrue(!!item);
      expectEquals(kExtensionId, item.id);
      expectEquals('My extension 1', item.name);
      expectEquals(ExtensionState.ENABLED, item.state);

      var disabledListener =
          new ItemChangedListener(kExtensionId, EventType.UNLOADED);
      service.setItemEnabled(kExtensionId, false);
      disabledListener.onUpdate
          .then(function() {
            // Note: we need to re-get the item since the object in the
            // manager's collection was replaced, rather than updated.
            var item = getItemData(kExtensionId);
            assertTrue(!!item);
            expectEquals(ExtensionState.DISABLED, item.state);

            enabledListener =
                new ItemChangedListener(kExtensionId, EventType.LOADED);
            service.setItemEnabled(kExtensionId, true);
            return enabledListener.onUpdate;
          })
          .then(function() {
            var item = getItemData(kExtensionId);
            assertTrue(!!item);
            expectEquals(ExtensionState.ENABLED, item.state);
            done();
          });
    });

    test(assert(TestNames.ToggleIncognitoMode), function(done) {
      var item = getItemData(kExtensionId);
      assertTrue(!!item);
      expectTrue(item.incognitoAccess.isEnabled);
      expectFalse(item.incognitoAccess.isActive);

      var incognitoListener =
          new ItemChangedListener(kExtensionId, EventType.LOADED);
      chrome.test.runWithUserGesture(function() {
        service.setItemAllowedIncognito(kExtensionId, true);
      });
      incognitoListener.onUpdate
          .then(function() {
            var item = getItemData(kExtensionId);
            assertTrue(!!item);
            expectTrue(item.incognitoAccess.isActive);

            var disabledIncognitoListener =
                new ItemChangedListener(kExtensionId, EventType.LOADED);
            chrome.test.runWithUserGesture(function() {
              service.setItemAllowedIncognito(kExtensionId, false);
            });
            return disabledIncognitoListener.onUpdate;
          })
          .then(function() {
            var item = getItemData(kExtensionId);
            assertTrue(!!item);
            expectFalse(item.incognitoAccess.isActive);
            done();
          });
    });

    test(assert(TestNames.Uninstall), function(done) {
      var item = getItemData(kExtensionId);
      assertTrue(!!item);
      var uninstallListener =
          new ItemChangedListener(kExtensionId, EventType.UNINSTALLED);
      chrome.test.runWithUserGesture(function() {
        service.deleteItem(kExtensionId);
      });
      uninstallListener.onUpdate.then(function() {
        expectFalse(!!getItemData(kExtensionId));
        done();
      });
    });

    test(assert(TestNames.ProfileSettings), function(done) {
      expectFalse(manager.inDevMode);

      var profileListener = new ProfileChangedListener();
      service.setProfileInDevMode(true);

      profileListener.onUpdate
          .then(function() {
            expectTrue(manager.inDevMode);

            var noDevModeProfileListener = new ProfileChangedListener();
            service.setProfileInDevMode(false);
            return noDevModeProfileListener.onUpdate;
          })
          .then(function() {
            expectFalse(manager.inDevMode);
            done();
          });
    });
  });

  return {
    ChangeListener: ChangeListener,
    TestNames: TestNames,
  };
});
