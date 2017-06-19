// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suiteSetup(function() {
  cr.define('bookmarks', function() {
    var TestStore = function(data) {
      bookmarks.Store.call(this);
      this.data_ = Object.assign(bookmarks.util.createEmptyState(), data);
      this.initialized_ = true;

      this.lastAction_ = null;
      this.acceptInit_ = false;
      this.enableReducers_ = false;
      /** @type {!Map<string, !PromiseResolver>} */
      this.resolverMap_ = new Map();
    };

    TestStore.prototype = {
      __proto__: bookmarks.Store.prototype,

      /** @override */
      init: function(state) {
        if (this.acceptInit_)
          bookmarks.Store.prototype.init.call(this, state);
      },

      get lastAction() {
        return this.lastAction_;
      },

      resetLastAction() {
        this.lastAction_ = null;
      },

      get data() {
        return this.data_;
      },

      set data(newData) {
        this.data_ = newData;
      },

      /** Replace the global store instance with this TestStore. */
      replaceSingleton: function() {
        bookmarks.Store.instance_ = this;
      },

      /**
       * Enable or disable calling bookmarks.reduceAction for each action.
       * With reducers disabled (the default), TestStore is a stub which
       * requires state be managed manually (suitable for unit tests). With
       * reducers enabled, TestStore becomes a proxy for observing actions
       * (suitable for integration tests).
       * @param {boolean} enabled
       */
      setReducersEnabled: function(enabled) {
        this.enableReducers_ = enabled;
      },

      /** @override */
      reduce_: function(action) {
        this.lastAction_ = action;
        if (this.enableReducers_)
          bookmarks.Store.prototype.reduce_.call(this, action);
        if (this.resolverMap_.has(action.name))
          this.resolverMap_.get(action.name).resolve(action);
      },

      /**
       * Notifies UI elements that the store data has changed. When reducers are
       * disabled, tests are responsible for manually changing the data to make
       * UI elements update correctly (eg, tests must replace the whole list
       * when changing a single element).
       */
      notifyObservers: function() {
        this.notifyObservers_(this.data);
      },

      // Call in order to accept data from an init call to the TestStore once.
      acceptInitOnce: function() {
        this.acceptInit_ = true;
        this.initialized_ = false;
      },

      /**
       * Track actions called |name|, allowing that type of action to be waited
       * for with `waitForAction`.
       * @param {string} name
       */
      expectAction: function(name) {
        this.resolverMap_.set(name, new PromiseResolver());
      },

      /**
       * Returns a Promise that will resolve when an action called |name| is
       * dispatched. The promise must be prepared by calling
       * `expectAction(name)` before the action is dispatched.
       * @param {string} name
       * @return {!Promise<!Action>}
       */
      waitForAction: function(name) {
        assertTrue(
            this.resolverMap_.has(name),
            'Must call expectAction before each call to waitForAction');
        return this.resolverMap_.get(name).promise.then((action) => {
          this.resolverMap_.delete(name);
          return action;
        });
      },
    };

    return {
      TestStore: TestStore,
    };
  });
});
