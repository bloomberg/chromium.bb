// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.passwordManager', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const DeletableItemList = options.DeletableItemList;
  const DeletableItem = options.DeletableItem;
  const List = cr.ui.List;

  /**
   * Creates a new passwords list item.
   * @param {Array} entry An array of the form [url, username, password].
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function PasswordListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = PasswordListItem.prototype;
    el.decorate();

    return el;
  }

  PasswordListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The URL of the site.
      var urlLabel = this.ownerDocument.createElement('div');
      urlLabel.className = 'url';
      urlLabel.classList.add('favicon-cell');
      urlLabel.textContent = this.url;
      urlLabel.style.backgroundImage = url('chrome://favicon/' + this.url);
      this.contentElement.appendChild(urlLabel);

      // The stored username.
      var usernameLabel = this.ownerDocument.createElement('div');
      usernameLabel.className = 'name';
      usernameLabel.textContent = this.username;
      this.contentElement.appendChild(usernameLabel);

      // The stored password.
      var passwordInputDiv = this.ownerDocument.createElement('div');
      passwordInputDiv.className = 'password';

      // The password input field.
      var passwordInput = this.ownerDocument.createElement('input');
      passwordInput.className = 'inactive-password';
      passwordInput.readOnly = true;
      passwordInput.type = 'password';
      passwordInput.value = this.password;
      passwordInputDiv.appendChild(passwordInput);

      // The show/hide button.
      var buttonSpan = this.ownerDocument.createElement('span');
      buttonSpan.className = 'hidden';
      buttonSpan.addEventListener('click', this.onClick_, true);
      passwordInputDiv.appendChild(buttonSpan);

      this.contentElement.appendChild(passwordInputDiv);
    },

    /** @inheritDoc */
    selectionChanged: function() {
      var passwordInput = this.querySelector('input[type=password]');
      var textInput = this.querySelector('input[type=text]');
      var input = passwordInput || textInput;
      var buttonSpan = input.nextSibling;
      if (this.selected) {
        input.classList.remove('inactive-password');
        buttonSpan.classList.remove('hidden');
      } else {
        input.classList.add('inactive-password');
        buttonSpan.classList.add('hidden');
      }
    },

    /**
     * On-click event handler. Swaps the type of the input field from password
     * to text and back.
     * @private
     */
    onClick_: function(event) {
      // The password is the input element previous to the button span.
      var buttonSpan = event.currentTarget;
      var passwordInput = buttonSpan.previousSibling;
      var type = passwordInput.type;
      passwordInput.type = type == 'password' ? 'text' : 'password';
    },

    /**
     * Get and set the URL for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem[0];
    },
    set url(url) {
      this.dataItem[0] = url;
    },

    /**
     * Get and set the username for the entry.
     * @type {string}
     */
    get username() {
      return this.dataItem[1];
    },
    set username(username) {
      this.dataItem[1] = username;
    },

    /**
     * Get and set the password for the entry.
     * @type {string}
     */
    get password() {
      return this.dataItem[2];
    },
    set password(password) {
      this.dataItem[2] = password;
    },
  };

  /**
   * Creates a new PasswordExceptions list item.
   * @param {Array} entry A pair of the form [url, username].
   * @constructor
   * @extends {Deletable.ListItem}
   */
  function PasswordExceptionsListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = PasswordExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  PasswordExceptionsListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Call when an element is decorated as a list item.
     */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The URL of the site.
      var urlLabel = this.ownerDocument.createElement('div');
      urlLabel.className = 'url';
      urlLabel.classList.add('favicon-cell');
      urlLabel.textContent = this.url;
      urlLabel.style.backgroundImage = url('chrome://favicon/' + this.url);
      this.contentElement.appendChild(urlLabel);
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
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    createItem: function(entry) {
      return new PasswordListItem(entry);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      PasswordManager.removeSavedPassword(index);
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
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    createItem: function(entry) {
      return new PasswordExceptionsListItem(entry);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      PasswordManager.removePasswordException(index);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },
  };

  return {
    PasswordListItem: PasswordListItem,
    PasswordExceptionsListItem: PasswordExceptionsListItem,
    PasswordsList: PasswordsList,
    PasswordExceptionsList: PasswordExceptionsList,
  };
});
