var chrome = chrome || {};
chrome.sync = chrome.sync || {};
(function () {

function Log() {
  this.entries = [];

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
}

Log.prototype.log_ = function(submodule, event, details) {
  var entry = {
    submodule: submodule,
    event: event,
    date: new Date(),
    details: details
  };
  this.entries.push(entry);
}

chrome.sync.log = new Log();
})();
