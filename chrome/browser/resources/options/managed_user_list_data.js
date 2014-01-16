// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * ManagedUserListData class.
   * Handles requests for retrieving a list of existing managed users which are
   * supervised by the current profile. For each request a promise is returned,
   * which is cached in order to reuse the retrieved managed users for future
   * requests. The first request will be handled asynchronously.
   * @constructor
   * @class
   */
  function ManagedUserListData() {};

  cr.addSingletonGetter(ManagedUserListData);

  /**
   * Receives a list of managed users and resolves the promise.
   * @param {Array.<Object>} managedUsers An array of managed user objects.
   *     Each object is of the form:
   *       managedUser = {
   *         id: "Managed User ID",
   *         name: "Managed User Name",
   *         iconURL: "chrome://path/to/icon/image",
   *         onCurrentDevice: true or false,
   *         needAvatar: true or false
   *       }
   * @private
   */
  ManagedUserListData.prototype.receiveExistingManagedUsers_ = function(
      managedUsers) {
    assert(this.promise_);
    this.resolve_(managedUsers);
  };

  /**
   * Called when there is a signin error when retrieving the list of managed
   * users. Rejects the promise and resets the cached promise to null.
   * @private
   */
  ManagedUserListData.prototype.onSigninError_ = function() {
    assert(this.promise_);
    this.reject_();
    this.resetPromise_();
  };

  /**
   * Handles the request for the list of existing managed users by returning a
   * promise for the requested data. If there is no cached promise yet, a new
   * one will be created.
   * @return {Promise} The promise containing the list of managed users.
   * @private
   */
  ManagedUserListData.prototype.requestExistingManagedUsers_ = function() {
    if (this.promise_)
      return this.promise_;
    this.promise_ = this.createPromise_();
    chrome.send('requestManagedUserImportUpdate');
    return this.promise_;
  };

  /**
   * Creates the promise containing the list of managed users. The promise is
   * resolved in receiveExistingManagedUsers_() or rejected in
   * onSigninError_(). The promise is cached, so that for future requests it can
   * be resolved immediately.
   * @return {Promise} The promise containing the list of managed users.
   * @private
   */
  ManagedUserListData.prototype.createPromise_ = function() {
    var self = this;
    return new Promise(function(resolve, reject) {
      self.resolve_ = resolve;
      self.reject_ = reject;
    });
  };

  /**
   * Resets the promise to null in order to avoid stale data. For the next
   * request, a new promise will be created.
   * @private
   */
  ManagedUserListData.prototype.resetPromise_ = function() {
    this.promise_ = null;
  };

  // Forward public APIs to private implementations.
  [
    'onSigninError',
    'receiveExistingManagedUsers',
    'requestExistingManagedUsers',
    'resetPromise',
  ].forEach(function(name) {
    ManagedUserListData[name] = function() {
      var instance = ManagedUserListData.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserListData: ManagedUserListData,
  };
});
