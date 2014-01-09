// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * ManagedUserListData class.
   * Handles requests for retrieving a list of existing managed users which are
   * supervised by the current profile. This list is cached in order to make it
   * possible to serve future requests immediately. The first request will be
   * handled asynchronously.
   * @constructor
   * @class
   */
  function ManagedUserListData() {
    this.callbacks_ = [];
    this.errbacks_ = [];
    this.requestInProgress_ = false;
    this.managedUsers_ = null;
  };

  cr.addSingletonGetter(ManagedUserListData);

  /**
   * Resets to the initial state of no pending requests.
   * @private
   */
  ManagedUserListData.prototype.reset_ = function() {
    this.callbacks_ = [];
    this.errbacks_ = [];
    this.requestInProgress_ = false;
  }

  /**
   * Receives a list of managed users and passes the list to each of the
   * callbacks.
   * @param {Array.<Object>} managedUsers An array of managed user objects.
   *     Each object is of the form:
   *       managedUser = {
   *         id: "Managed User ID",
   *         name: "Managed User Name",
   *         iconURL: "chrome://path/to/icon/image",
   *         onCurrentDevice: true or false,
   *         needAvatar: true or false
   *       }
   */
  ManagedUserListData.receiveExistingManagedUsers = function(managedUsers) {
    var instance = ManagedUserListData.getInstance();
    var i;
    for (i = 0; i < instance.callbacks_.length; i++) {
      instance.callbacks_[i](managedUsers);
    }
    instance.managedUsers_ = managedUsers;
    instance.reset_();
  };

  /**
   * Called when there is a signin error when retrieving the list of managed
   * users. Calls the error callbacks which will display an appropriate error
   * message to the user.
   */
  ManagedUserListData.onSigninError = function() {
    var instance = ManagedUserListData.getInstance();
    var i;
    for (i = 0; i < instance.errbacks_.length; i++) {
      instance.errbacks_[i]();
    }
    // Reset the list of managed users in order to avoid showing stale data.
    instance.managedUsers_ = null;
    instance.reset_();
  };

  /**
   * Handles the request for the list of existing managed users. If the data is
   * already available, it will call |callback| immediately. Otherwise, it
   * retrieves the list of existing managed users which is then processed in
   * receiveExistingManagedUsers().
   * @param {Object} callback The callback function which is called on success.
   * @param {Object} errback the callback function which is called on error.
   */
  ManagedUserListData.requestExistingManagedUsers = function(callback,
                                                             errback) {
    var instance = ManagedUserListData.getInstance();
    instance.callbacks_.push(callback);
    instance.errbacks_.push(errback);
    if (!instance.requestInProgress_) {
      if (instance.managedUsers_ != null) {
        ManagedUserListData.receiveExistingManagedUsers(instance.managedUsers_);
        return;
      }
      instance.requestInProgress_ = true;
      chrome.send('requestManagedUserImportUpdate');
    }
  };

  /**
   * Reload the list of existing managed users. Should be called when a new
   * supervised user profile was created or a supervised user profile was
   * deleted.
   */
  ManagedUserListData.reloadExistingManagedUsers = function() {
    var instance = ManagedUserListData.getInstance();
    if (instance.requestInProgress_)
      return;

    instance.managedUsers_ = null;
    instance.requestInProgress_ = true;
    chrome.send('requestManagedUserImportUpdate');
  };

  // Export
  return {
    ManagedUserListData: ManagedUserListData,
  };
});
