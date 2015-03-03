// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('webui-view', (function() {
  /** @const */ var CALLBACK_CONTEXT_CHANGED = 'contextChanged';
  /** @const */ var CALLBACK_READY = 'ready';

  return {
    /** @const */
    CALLBACK_EVENT_FIRED: 'eventFired',

    /**
     * Dictionary of context observers that are methods of |this| bound to
     * |this|.
     */
    contextObservers_: null,

    /**
     * Full path to the element in views hierarchy.
     */
    path_: null,

    /**
     * Whether DOM is ready.
     */
    domReady_: false,

    /**
     * Context used for sharing data with native backend.
     */
    context: null,

    /**
     * Internal storage of |this.context|.
     * |C| bound to the native part of the context, that means that all the
     * changes in the native part appear in |C| automatically. Reverse is not
     * true, you should use:
     *    this.context.set(...);
     *    this.context.commitContextChanges();
     * to send updates to the native part.
     */
    C: null,

    ready: function() {
      this.context = new wug.Context;
      this.C = this.context.storage_;
      this.contextObservers_ = {};
    },

    /**
     * One of element's lifecycle methods.
     */
    domReady: function() {
      var id = this.getAttribute('wugid');
      if (!id) {
        console.error('"wugid" attribute is missing.');
        return;
      }
      if (id == 'WUG_ROOT')
        this.setPath_('WUG_ROOT');
      this.domReady_ = true;
      if (this.path_)
        this.init_();
    },

    setPath_: function(path) {
      this.path_ = path;
      if (this.domReady_)
        this.init_();
    },

    init_: function() {
      this.initContext_();
      this.initChildren_();
      window[this.path_ + '$contextChanged'] =
          this.onContextChanged_.bind(this);
      this.initialize();
      this.send(CALLBACK_READY);
    },

    fireEvent: function(_, _, source) {
      this.send(this.CALLBACK_EVENT_FIRED, source.getAttribute('event'));
    },

    i18n: function(args) {
      if (!(args instanceof Array))
        args = [args];
      args[0] = this.getType() + '$' + args[0];
      return loadTimeData.getStringF.apply(loadTimeData, args);
    },

    /**
     * Sends message to Chrome, adding needed prefix to message name. All
     * arguments after |messageName| are packed into message parameters list.
     *
     * @param {string} messageName Name of message without a prefix.
     * @param {...*} varArgs parameters for message.
     * @private
     */
    send: function(messageName, varArgs) {
      if (arguments.length == 0)
        throw Error('Message name is not provided.');
      var fullMessageName = this.path_ + '$' + messageName;
      var payload = Array.prototype.slice.call(arguments, 1);
      chrome.send(fullMessageName, payload);
    },

    /**
     * Starts observation of property with |key| of the context attached to
     * current screen. In contrast with "wug.Context.addObserver" this method
     * can automatically detect if observer is method of |this| and make
     * all needed actions to make it work correctly. So there is no need in
     * binding method to |this| before adding it. For example, if |this| has
     * a method |onKeyChanged_|, you can do:
     *
     *   this.addContextObserver('key', this.onKeyChanged_);
     *   ...
     *   this.removeContextObserver('key', this.onKeyChanged_);
     *
     * instead of:
     *
     *   this.keyObserver_ = this.onKeyChanged_.bind(this);
     *   this.addContextObserver('key', this.keyObserver_);
     *   ...
     *   this.removeContextObserver('key', this.keyObserver_);
     *
     * @private
     */
    addContextObserver: function(key, observer) {
      var realObserver = observer;
      var propertyName = this.getPropertyNameOf_(observer);
      if (propertyName) {
        if (!this.contextObservers_.hasOwnProperty(propertyName))
          this.contextObservers_[propertyName] = observer.bind(this);
        realObserver = this.contextObservers_[propertyName];
      }
      this.context.addObserver(key, realObserver);
    },

    /**
     * Removes |observer| from the list of context observers. Observer could be
     * a method of |this| (see comment to |addContextObserver|).
     * @private
     */
    removeContextObserver: function(observer) {
      var realObserver = observer;
      var propertyName = this.getPropertyNameOf_(observer);
      if (propertyName) {
        if (!this.contextObservers_.hasOwnProperty(propertyName))
          return;
        realObserver = this.contextObservers_[propertyName];
        delete this.contextObservers_[propertyName];
      }
      this.context.removeObserver(realObserver);
    },

    /**
     * Sends recent context changes to C++ handler.
     * @private
     */
    commitContextChanges: function() {
      if (!this.context.hasChanges())
        return;
      this.send(CALLBACK_CONTEXT_CHANGED, this.context.getChangesAndReset());
    },

    /**
     * Called when context changed on C++ side.
     */
    onContextChanged_: function(diff) {
      var changedKeys = this.context.applyChanges(diff);
      this.contextChanged(changedKeys);
    },

    /**
     * If |value| is the value of some property of |this| returns property's
     * name. Otherwise returns empty string.
     * @private
     */
    getPropertyNameOf_: function(value) {
      for (var key in this)
        if (this[key] === value)
          return key;
      return '';
    }
  };
})());

