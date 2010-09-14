// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.passwordsExceptions', function() {

  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new passwords list item.
   * @param {Array} entry A pair of the form [url, username].
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function PasswordsListItem(entry) {
    var el = cr.doc.createElement('li');
    el.dataItem = entry;
    el.__proto__ = PasswordsListItem.prototype;
    el.decorate();

    return el;
  }

  PasswordsListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Call when an element is decorated as a list item.
     */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      // Labels for display
      var urlLabel = cr.doc.createElement('span');
      urlLabel.textContent = this.url;
      this.appendChild(urlLabel);
      this.urlLabel = urlLabel;

      var usernameLabel = cr.doc.createElement('span');
      usernameLabel.textContent = this.username;
      usernameLabel.className = 'passwordsUsername';
      this.appendChild(usernameLabel);
      this.usernameLabel = usernameLabel;
    },

    /**
     * Get the url for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem[0];
    },
    set url(url) {
      this.dataItem[0] = url;
    },

    /**
     * Get the username for the entry.
     * @type {string}
     */
    get username() {
      return this.dataItem[1];
     },
     set username(username) {
      this.dataItem[1] = username;
     },
  };

  /**
   * Creates a new PasswordExceptions list item.
   * @param {Array} entry A pair of the form [url, username].
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function PasswordExceptionsListItem(entry) {
    var el = cr.doc.createElement('li');
    el.dataItem = entry;
    el.__proto__ = PasswordExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  PasswordExceptionsListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Call when an element is decorated as a list item.
     */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      // Labels for display
      var urlLabel = cr.doc.createElement('span');
      urlLabel.textContent = this.url;
      this.appendChild(urlLabel);
      this.urlLabel = urlLabel;
    },

    /**
     * Get the url for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem;
    },
    set url(url) {
      this.dataItem = url;
    },
  };


  /**
   * Create a new passwords list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var PasswordsList = cr.ui.define('list');

  PasswordsList.prototype = {
    __proto__: List.prototype,
    /**
     * Called when an element is decorated as a list.
     */
    decorate: function() {
      List.prototype.decorate.call(this);

      this.dataModel = new ArrayDataModel([]);
    },

    /**
     * Creates an item to go in the list.
     * @param {Object} entry The element from the data model for this row.
     */
    createItem: function(entry) {
      return new PasswordsListItem(entry);
    },

    /**
     * Adds an entry to the js model.
     * @param {Array} entry A pair of the form [url, username].
     */
    addEntry: function(entry) {
      this.dataModel.push(entry);
      this.listArea.updateButtonSensitivity();
    },

    /**
     * Remove all entries from the js model.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
      this.listArea.updateButtonSensitivity();
    },

    /**
     * Remove selected row from browser's model.
     */
    removeSelectedRow: function() {
      var selectedIndex = this.selectionModel.selectedIndex;
      PasswordsExceptions.removeSavedPassword(selectedIndex);
    },

    showSelectedPassword: function() {
      var selectedIndex = this.selectionModel.selectedIndex;
      PasswordsExceptions.showSelectedPassword(selectedIndex);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var PasswordExceptionsList = cr.ui.define('list');

  PasswordExceptionsList.prototype = {
    __proto__: List.prototype,
    /**
     * Called when an element is decorated as a list.
     */
    decorate: function() {
      List.prototype.decorate.call(this);

      this.dataModel = new ArrayDataModel([]);
    },

    /**
     * Creates an item to go in the list.
     * @param {Object} entry The element from the data model for this row.
     */
    createItem: function(entry) {
      return new PasswordExceptionsListItem(entry);
    },

    /**
     * Adds an entry to the js model.
     * @param {Array} entry A pair of the form [url, username].
     */
    addEntry: function(entry) {
      this.dataModel.push(entry);
      this.listArea.updateButtonSensitivity();
    },

    /**
     * Remove all entries from the js model.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
      this.listArea.updateButtonSensitivity();
    },

    /**
     * Remove selected row from browser's model.
     */
    removeSelectedRow: function() {
      var selectedIndex = this.selectionModel.selectedIndex;
      PasswordsExceptions.removePasswordException(selectedIndex);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },
  };

  /**
   * Create a new passwords list area.
   * @constructor
   * @extends {cr.ui.div}
   */
  var PasswordsListArea = cr.ui.define('div');

  PasswordsListArea.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.passwordsList = this.querySelector('list');
      this.passwordsList.listArea = this;

      PasswordsList.decorate(this.passwordsList);
      this.passwordsList.selectionModel.addEventListener(
          'change', this.handleOnSelectionChange_.bind(this));

      var removeRow = cr.doc.createElement('button');
      removeRow.textContent = templateData.passwordsRemoveButton;
      this.appendChild(removeRow);
      this.removeRow = removeRow;

      var removeAll = cr.doc.createElement('button');
      removeAll.textContent = templateData.passwordsRemoveAllButton;
      this.appendChild(removeAll);
      this.removeAll = removeAll;

      var showHidePassword = cr.doc.createElement('button');
      showHidePassword.textContent = templateData.passwordsShowButton;
      this.appendChild(showHidePassword);
      this.showHidePassword = showHidePassword;
      this.showingPassword = false

      var passwordLabel = cr.doc.createElement('span');
      this.appendChild(passwordLabel);
      this.passwordLabel = passwordLabel;

      var self = this;
      removeRow.onclick = function(event) {
        self.passwordsList.removeSelectedRow();
      };

      removeAll.onclick = function(event) {
        AlertOverlay.show(undefined,
            localStrings.getString('passwordsRemoveAllWarning'),
            localStrings.getString('yesButtonLabel'),
            localStrings.getString('noButtonLabel'),
            function() { PasswordsExceptions.removeAllPasswords(); });
      };

      showHidePassword.onclick = function(event) {
        if(self.showingPassword) {
          self.passwordLabel.textContent = "";
          this.textContent = templateData.passwordsShowButton;
        } else {
          self.passwordsList.showSelectedPassword();
          this.textContent = templateData.passwordsHideButton;
        }
        self.showingPassword = !self.showingPassword;
      };

      this.updateButtonSensitivity();
    },

    displayReturnedPassword: function(password) {
      this.passwordLabel.textContent = password;
    },

    /**
     * Update the button's states
     */
    updateButtonSensitivity: function() {
      var selectionSize = this.passwordsList.selectedItems.length;
      this.removeRow.disabled = selectionSize == 0;
      this.showHidePassword.disabled = selectionSize == 0;
      this.removeAll.disabled = this.passwordsList.length == 0;
    },

    /**
     * Callback from selection model
     * @param {!cr.Event} ce Event with change info.
     * @private
     */
    handleOnSelectionChange_: function(ce) {
      this.passwordLabel.textContent = "";
      this.showHidePassword.textContent = templateData.passwordsShowButton;
      this.showingPassword = false;
      this.updateButtonSensitivity();
    },
  };

  /**
   * Create a new passwords list area.
   * @constructor
   * @extends {cr.ui.div}
   */
  var PasswordExceptionsListArea = cr.ui.define('div');

  PasswordExceptionsListArea.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.passwordExceptionsList = this.querySelector('list');
      this.passwordExceptionsList.listArea = this;

      PasswordExceptionsList.decorate(this.passwordExceptionsList);
      this.passwordExceptionsList.selectionModel.addEventListener(
          'change', this.handleOnSelectionChange_.bind(this));

      var removeRow = cr.doc.createElement('button');
      removeRow.textContent = templateData.passwordsRemoveButton;
      this.appendChild(removeRow);
      this.removeRow = removeRow;

      var removeAll = cr.doc.createElement('button');
      removeAll.textContent = templateData.passwordsRemoveAllButton;
      this.appendChild(removeAll);
      this.removeAll = removeAll;

      var self = this;
      removeRow.onclick = function(event) {
        self.passwordExceptionsList.removeSelectedRow();
      };

      removeAll.onclick = function(event) {
        PasswordsExceptions.removeAllPasswordExceptions();
      };

      this.updateButtonSensitivity();
    },

    /**
     * Update the button's states
     */
    updateButtonSensitivity: function() {
      var selectionSize = this.passwordExceptionsList.selectedItems.length;
      this.removeRow.disabled = selectionSize == 0;
      this.removeAll.disabled = this.passwordExceptionsList.length == 0;
    },

    /**
     * Callback from selection model
     * @param {!cr.Event} ce Event with change info.
     * @private
     */
    handleOnSelectionChange_: function(ce) {
      this.updateButtonSensitivity();
    },
  };


  return {
    PasswordsListItem: PasswordsListItem,
    PasswordExceptionsListItem: PasswordExceptionsListItem,
    PasswordsList: PasswordsList,
    PasswordExceptionsList: PasswordExceptionsList,
    PasswordsListArea: PasswordsListArea,
    PasswordExceptionsListArea: PasswordExceptionsListArea
  };
});
