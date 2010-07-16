// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.accounts', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new user list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var UserList = cr.ui.define('list');

  UserList.prototype = {
    __proto__: List.prototype,

    pref: 'cros.accounts.users',

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);

      // HACK(arv): http://crbug.com/40902
      window.addEventListener('resize', cr.bind(this.redraw, this));

      var self = this;
      if (!this.boundHandleChange_) {
        this.boundHandleChange_ =
            cr.bind(this.handleChange_, this);
      }

      // Listens to pref changes.
      Preferences.getInstance().addEventListener(this.pref,
          function(event) {
            self.load_(event.value);
          });

      // Listens to list selection change.
      this.addEventListener('change', this.boundHandleChange_);
    },

    createItem: function(user) {
      return new ListItem({label: user.email});
    },

    /**
     * Adds given user to model and update backend.
     * @param {Object} user A user to be added to user list.
     */
    addUser: function(user) {
      var dataModel = this.dataModel;
      dataModel.splice(dataModel.length, 0, user);

      this.updateBackend_();
    },

    /**
     * Removes currently selected user from model and update backend.
     */
    removeSelectedUser: function() {
      var sm = this.selectionModel;
      var dataModel = this.dataModel;

      var newUsers = [];
      for (var i = 0; i < dataModel.length; ++i) {
        if (!sm.getIndexSelected(i)) {
          newUsers.push(dataModel.item(i));
        }
      }
      this.load_(newUsers);

      this.updateBackend_();
    },

    /**
     * Loads given user list.
     * @param {Array} users An array of user object.
     */
    load_: function(users) {
      this.dataModel = new ArrayDataModel(users);
    },

    /**
     * Updates backend.
     */
    updateBackend_: function() {
      Preferences.setObjectPref(this.pref, this.dataModel.slice());
    },

    /**
     * Handles selection change.
     */
    handleChange_: function(e) {
      $('removeUserButton').disabled = this.selectionModel.selectedIndex == -1;
    }
  };

  return {
    UserList: UserList
  };
});
