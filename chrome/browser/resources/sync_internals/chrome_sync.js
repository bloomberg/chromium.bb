var chrome = chrome || {};
// TODO(akalin): Add mocking code for e.g. chrome.send() so that we
// can test this without rebuilding chrome.
chrome.sync = chrome.sync || {};
(function () {

// This Event class is a simplified version of the one from
// event_bindings.js.
function Event() {
  this.listeners_ = [];
}

Event.prototype.addListener = function(listener) {
  this.listeners_.push(listener);
};

Event.prototype.removeListener = function(listener) {
  var i = this.findListener_(listener);
  if (i == -1) {
    return;
  }
  this.listeners_.splice(i, 1);
};

Event.prototype.hasListener = function(listener) {
  return this.findListener_(listener) > -1;
};

Event.prototype.hasListeners = function(listener) {
  return this.listeners_.length > 0;
};

// Returns the index of the given listener, or -1 if not found.
Event.prototype.findListener_ = function(listener) {
  for (var i = 0; i < this.listeners_.length; i++) {
    if (this.listeners_[i] == listener) {
      return i;
    }
  }
  return -1;
};

// Fires the event.  Called by the actual event callback.
Event.prototype.dispatch_ = function() {
  var args = Array.prototype.slice.call(arguments);
  for (var i = 0; i < this.listeners_.length; i++) {
    try {
      this.listeners_[i].apply(null, args);
    } catch (e) {
      console.error(e);
    }
  }
};

// Sync service events.
chrome.sync.onSyncServiceStateChanged = new Event();

// Notification events.
chrome.sync.onSyncNotificationStateChange = new Event();
chrome.sync.onSyncIncomingNotification = new Event();

// Sync manager events.
chrome.sync.onChangesApplied = new Event();
chrome.sync.onChangesComplete = new Event();
chrome.sync.onSyncCycleCompleted = new Event();
chrome.sync.onAuthError = new Event();
chrome.sync.onUpdatedToken = new Event();
chrome.sync.onPassphraseRequired = new Event();
chrome.sync.onPassphraseAccepted = new Event();
chrome.sync.onInitializationComplete = new Event();
chrome.sync.onPaused = new Event();
chrome.sync.onResumed = new Event();
chrome.sync.onStopSyncingPermanently = new Event();
chrome.sync.onClearServerDataSucceeded = new Event();
chrome.sync.onClearServerDataFailed = new Event();

function AsyncFunction(name) {
  this.name_ = name;
  this.callbacks_ = [];
}

// Calls the function, assuming the last argument is a callback to be
// called with the return value.
AsyncFunction.prototype.call = function() {
  var args = Array.prototype.slice.call(arguments);
  this.callbacks_.push(args.pop());
  chrome.send(this.name_, args);
}

// Handle a reply, assuming that messages are processed in FIFO order.
AsyncFunction.prototype.handleReply = function() {
  var args = Array.prototype.slice.call(arguments);
  var callback = this.callbacks_.shift();
  try {
    callback.apply(null, args);
  } catch (e) {
    console.error(e);
  }
}

// Sync service functions.
chrome.sync.getAboutInfo_ = new AsyncFunction('getAboutInfo');
chrome.sync.getAboutInfo = function(callback) {
  chrome.sync.getAboutInfo_.call(callback);
}

// Notification functions.
chrome.sync.getNotificationState_ =
    new AsyncFunction('getNotificationState');
chrome.sync.getNotificationState = function(callback) {
  chrome.sync.getNotificationState_.call(callback);
}

// Node lookup functions.
chrome.sync.getRootNode_ = new AsyncFunction('getRootNode');
chrome.sync.getRootNode = function(callback) {
  chrome.sync.getRootNode_.call(callback);
}

chrome.sync.getNodeById_ = new AsyncFunction('getNodeById');
chrome.sync.getNodeById = function(id, callback) {
  chrome.sync.getNodeById_.call(id, callback);
}

})();

// TODO(akalin): Rewrite the C++ side to not need the handlers below.

// Sync service event handlers.

function onSyncServiceStateChanged() {
  chrome.sync.onSyncServiceStateChanged.dispatch_();
}

// Notification event handlers.

function onSyncNotificationStateChange(notificationsEnabled) {
  chrome.sync.onSyncNotificationStateChange.dispatch_(notificationsEnabled);
}

function onSyncIncomingNotification(changedTypes) {
  chrome.sync.onSyncIncomingNotification.dispatch_(changedTypes);
}

// Sync manager event handlers.

function onChangesApplied(modelType, changes) {
  chrome.sync.onChangesApplied.dispatch_(modelType, changes);
}

function onChangesComplete(modelType) {
  chrome.sync.onChangesComplete.dispatch_(modelType);
}

function onSyncCycleCompleted(snapshot) {
  chrome.sync.onSyncCycleCompleted.dispatch_(snapshot);
}

function onAuthError(authError) {
  chrome.sync.onAuthError.dispatch_(authError);
}

function onUpdatedToken(token) {
  chrome.sync.onUpdatedToken.dispatch_(token);
}

function onPassphraseRequired(forDecryption) {
  chrome.sync.onPassphraseRequired.dispatch_(forDecryption);
}

function onPassphraseAccepted(bootstrapToken) {
  chrome.sync.onPassphraseAccepted.dispatch_(bootstrapToken);
}

function onInitializationComplete() {
  chrome.sync.onInitializationComplete.dispatch_();
}

function onPaused() {
  chrome.sync.onPaused.dispatch_();
}

function onResumed() {
  chrome.sync.onResumed.dispatch_();
}

function onStopSyncingPermanently() {
  chrome.sync.onStopSyncingPermanently.dispatch_();
}

function onClearServerDataSucceeded() {
  chrome.sync.onClearServerDataSucceeded();
}

function onClearServerDataFailed() {
  chrome.sync.onClearServerDataFailed();
}

// Function reply handlers.

function onGetAboutInfoFinished(aboutInfo) {
  chrome.sync.getAboutInfo_.handleReply(aboutInfo);
}

function onGetNotificationStateFinished(notificationState) {
  chrome.sync.getNotificationState_.handleReply(notificationState);
}

function onGetRootNodeFinished(rootNode) {
  chrome.sync.getRootNode_.handleReply(rootNode);
}

function onGetNodeByIdFinished(node) {
  chrome.sync.getNodeById_.handleReply(node);
}
