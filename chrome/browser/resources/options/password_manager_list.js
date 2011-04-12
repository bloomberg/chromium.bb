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
  function PasswordListItem(entry, showPasswords) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = PasswordListItem.prototype;
    el.decorate(showPasswords);

    return el;
  }

  PasswordListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function(showPasswords) {
      DeletableItem.prototype.decorate.call(this);

      // The URL of the site.
      var urlLabel = this.ownerDocument.createElement('div');
      urlLabel.classList.add('favicon-cell');
      urlLabel.classList.add('url');
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
      passwordInput.type = 'password';
      passwordInput.className = 'inactive-password';
      passwordInput.readOnly = true;
      passwordInput.value = showPasswords ? this.password : "********";
      passwordInputDiv.appendChild(passwordInput);

      // The show/hide button.
      if (showPasswords) {
        var button = this.ownerDocument.createElement('button');
        button.classList.add('hidden');
        button.classList.add('password-button');
        button.textContent = localStrings.getString('passwordShowButton');
        button.addEventListener('click', this.onClick_, true);
        passwordInputDiv.appendChild(button);
      }

      this.contentElement.appendChild(passwordInputDiv);
    },

    /** @inheritDoc */
    selectionChanged: function() {
      var passwordInput = this.querySelector('input[type=password]');
      var textInput = this.querySelector('input[type=text]');
      var input = passwordInput || textInput;
      var button = input.nextSibling;
      // |button| doesn't exist when passwords can't be shown.
      if (!button)
        return;
      if (this.selected) {
        input.classList.remove('inactive-password');
        button.classList.remove('hidden');
      } else {
        input.classList.add('inactive-password');
        button.classList.add('hidden');
      }
    },

    /**
     * On-click event handler. Swaps the type of the input field from password
     * to text and back.
     * @private
     */
    onClick_: function(event) {
      // The password is the input element previous to the button.
      var button = event.currentTarget;
      var passwordInput = button.previousSibling;
      if (passwordInput.type == 'password') {
        passwordInput.type = 'text';
        button.textContent = localStrings.getString('passwordHideButton');
      } else {
        passwordInput.type = 'password';
        button.textContent = localStrings.getString('passwordShowButton');
      }
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

    /**
     * Whether passwords can be revealed or not.
     * @type {boolean}
     * @private
     */
    showPasswords_: true,

    /** @inheritDoc */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      Preferences.getInstance().addEventListener(
          "profile.password_manager_allow_show_passwords",
          this.onPreferenceChanged_.bind(this));
    },

    /**
     * Listener for changes on the preference.
     * @param {Event} event The preference update event.
     * @private
     */
    onPreferenceChanged_: function(event) {
      this.showPasswords_ = event.value.value;
      this.redraw();
    },

    /** @inheritDoc */
    createItem: function(entry) {
      return new PasswordListItem(entry, this.showPasswords_);
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
