// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr.js
// require: cr/event_target.js

/**
 * @fileoverview This creates a log object which listens to and
 * records all sync events.
 */

cr.define('chrome.sync', function() {
  /**
   * Creates a new log object which then immediately starts recording
   * sync events.  Recorded entries are available in the 'entries'
   * property and there is an 'append' event which can be listened to.
   * @constructor
   * @extends {cr.EventTarget}
   */
  var Log = function() {
    var self = this;

    // Service

    chrome.sync.onSyncServiceStateChanged.addListener(function () {
      self.log_('service', 'onSyncServiceStateChanged', {});
    });

    // Notifier

    chrome.sync.onSyncNotificationStateChange.addListener(
      function (notificationsEnabled) {
        self.log_('notifier', 'onSyncNotificationStateChange', {
                    notificationsEnabled: notificationsEnabled
                  });
      });

    chrome.sync.onSyncIncomingNotification.addListener(function (changedTypes) {
      self.log_('notifier', 'onSyncIncomingNotification', {
                  changedTypes: changedTypes
                });
    });

    // Manager

    chrome.sync.onChangesApplied.addListener(function (modelType, changes) {
      self.log_('manager', 'onChangesApplied', {
                  modelType: modelType,
                  changes: changes
                });
    });

    chrome.sync.onChangesComplete.addListener(function (modelType) {
      self.log_('manager', 'onChangesComplete', {
                  modelType: modelType
                });
    });

    chrome.sync.onSyncCycleCompleted.addListener(function (snapshot) {
      self.log_('manager', 'onSyncCycleCompleted', {
                  snapshot: snapshot
                });
    });

    chrome.sync.onAuthError.addListener(function (authError) {
      self.log_('manager', 'onAuthError', {
                  authError: authError
                });
    });

    chrome.sync.onUpdatedToken.addListener(function (token) {
      self.log_('manager', 'onUpdatedToken', {
                  token: token
                });
    });

    chrome.sync.onPassphraseRequired.addListener(function (forDecryption) {
      self.log_('manager', 'onPassphraseRequired', {
                  forDecryption: forDecryption
                });
    });

    chrome.sync.onPassphraseAccepted.addListener(function (bootstrapToken) {
      self.log_('manager', 'onPassphraseAccepted', {
                  bootstrapToken: bootstrapToken
                });
    });

    chrome.sync.onEncryptionComplete.addListener(function (encrypted_types) {
      self.log_('manager', 'onEncryptionComplete', {
                  encrypted_types: encrypted_types
                });
    });

    chrome.sync.onMigrationNeededForTypes.addListener(function (model_types) {
      self.log_('manager', 'onMigrationNeededForTypes', {
                  model_types: model_types
                });
    });

    chrome.sync.onInitializationComplete.addListener(function () {
      self.log_('manager', 'onInitializationComplete', {});
    });

    chrome.sync.onPaused.addListener(function () {
      self.log_('manager', 'onPaused', {});
    });

    chrome.sync.onResumed.addListener(function () {
      self.log_('manager', 'onResumed', {});
    });

    chrome.sync.onStopSyncingPermanently.addListener(function () {
      self.log_('manager', 'onStopSyncingPermanently', {});
    });

    chrome.sync.onClearServerDataSucceeded.addListener(function () {
      self.log_('manager', 'onClearServerDataSucceeded', {});
    });

    chrome.sync.onClearServerDataFailed.addListener(function () {
      self.log_('manager', 'onClearServerDataFailed', {});
    });
  };

  Log.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * The recorded log entries.
     * @type {array}
     */
    entries: [],

    /**
     * Records a single event with the given parameters and fires the
     * 'append' event with the newly-created event as the 'detail'
     * field of a custom event.
     * @param {string} submodule The sync submodule for the event.
     * @param {string} event The name of the event.
     * @param {dictionary} details A dictionary of event-specific details.
     */
    log_: function(submodule, event, details) {
      var entry = {
        submodule: submodule,
        event: event,
        date: new Date(),
        details: details
      };
      this.entries.push(entry);
      // Fire append event.
      var e = cr.doc.createEvent('CustomEvent');
      e.initCustomEvent('append', false, false, entry);
      this.dispatchEvent(e);
    }
  };

  return {
    log: new Log()
  };
});
