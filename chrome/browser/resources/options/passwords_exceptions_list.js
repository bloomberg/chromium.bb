// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.passwordsExceptions', function() {

  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new passwords list item.
   * @param contentType passwords or passwordsExceptions
   * @param {Array} entry A pair of the form [url, username].
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function PasswordsListItem(contentType, entry) {
    var el = cr.doc.createElement('li');
    el.contentType = contentType;
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

      if (this.contentType == 'passwords') {
        var usernameLabel = cr.doc.createElement('span');
        usernameLabel.textContent = this.username;
        usernameLabel.className = 'passwordsUsername';
        this.appendChild(usernameLabel);
        this.usernameLabel = usernameLabel;
      }
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
      if (this.contentType == 'passwords')
        return this.dataItem[1];
      else
        return undefined;
     },
     set username(username) {
      if (this.contentType == 'passwords')
        this.dataItem[1] = username;
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
      return new PasswordsListItem(this.contentType, entry);
    },

    /**
     * Adds an entry to the js model.
     * @param {Array} entry A pair of the form [url, username].
     */
    addEntry: function(entry) {
      this.dataModel.push(entry);
    },

    /**
     * Remove all entries from the js model.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
    },

    /**
     * Remove selected row from browser's model.
     */
    removeSelectedRow: function() {
      var selectedIndex = this.selectionModel.selectedIndex;
      PasswordsExceptions.removeEntry(this.contentType, selectedIndex);
    },

    showSelectedPassword: function() {
      if (this.contentType == 'passwords') {
        var selectedIndex = this.selectionModel.selectedIndex;
        PasswordsExceptions.showSelectedPassword(selectedIndex);
      }
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

  };

  var ListArea = cr.ui.define('div');

  ListArea.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.passwordsList = this.querySelector('list');
      this.passwordsList.contentType = this.contentType;

      PasswordsList.decorate(this.passwordsList);
      this.passwordsList.selectionModel.addEventListener(
          'change', cr.bind(this.handleOnSelectionChange_, this));
      this.passwordsList.dataModel.addEventListener(
          'change', cr.bind(this.handleOnDataModelChange_, this));

      var removeRow = cr.doc.createElement('button');
      removeRow.textContent = templateData.passwordsRemoveButton;
      this.appendChild(removeRow);
      this.removeRow = removeRow;

      var removeAll = cr.doc.createElement('button');
      removeAll.textContent = templateData.passwordsRemoveAllButton;
      this.appendChild(removeAll);
      this.removeAll = removeAll;

      if (this.contentType == 'passwords') {
        var showHidePassword = cr.doc.createElement('button');
        showHidePassword.textContent = templateData.passwordsShowButton;
        this.appendChild(showHidePassword);
        this.showHidePassword = showHidePassword;
        this.showingPassword = false

        var passwordLabel = cr.doc.createElement('span');
        this.appendChild(passwordLabel);
        this.passwordLabel = passwordLabel;
      }

      var self = this;
      removeRow.onclick = function(event) {
        self.passwordsList.removeSelectedRow();
      };

      if (this.contentType == 'passwords') {
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

        removeAll.onclick = function(event) {
          OptionsPage.showOverlay('passwordsRemoveAllOverlay');
        };
      } else {
        removeAll.onclick = function(event) {
          PasswordsExceptions.removeAll(this.contentType);
        };
      }

      this.updateButtonSensitivity();
    },

    /**
     * The content type for the exceptions area, such as 'passwords'
     * @type {string}
     */
    get contentType() {
      return this.getAttribute('contentType');
    },
    set contentType(type) {
      return this.getAttribute('contentType', type);
    },

    displayReturnedPassword: function(password) {
      if (this.contentType == 'passwords') {
        this.passwordLabel.textContent = password;
      }
    },

    /**
     * Update the button's states
     */
    updateButtonSensitivity: function() {
      var selectionSize = this.passwordsList.selectedItems.length;
      this.removeRow.disabled = selectionSize == 0;
      if (this.contentType == 'passwords')
        this.showHidePassword.disabled = selectionSize == 0;
    },

    updateRemoveAllSensitivity: function() {
      this.removeAll.disabled = this.passwordsList.length == 0;
    },
    /**
     * Callback from selection model
     * @param {!cr.Event} ce Event with change info.
     * @private
     */
    handleOnSelectionChange_: function(ce) {
      if (this.contentType == 'passwords') {
        this.passwordLabel.textContent = "";
        this.showHidePassword.textContent = templateData.passwordsShowButton;
        this.showingPassword = false;
      }
      this.updateButtonSensitivity();
    },

    /**
     * Callback from data model
     * @param {!cr.Event} ce Event with change info.
     * @private
     */
    handleOnDataModelChange_: function(ce) {
      this.updateRemoveAllSensitivity();
    },
  };

  return {
    PasswordsListItem: PasswordsListItem,
    PasswordsList: PasswordsList,
    ListArea: ListArea
  };
});
