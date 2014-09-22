// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.passwordManager', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var List = cr.ui.List;

  /**
   * Creates a new passwords list item.
   * @param {cr.ui.ArrayDataModel} dataModel The data model that contains this
   *     item.
   * @param {Array} entry An array of the form [url, username, password]. When
   *     the list has been filtered, a fourth element [index] may be present.
   * @param {boolean} showPasswords If true, add a button to the element to
   *     allow the user to reveal the saved password.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function PasswordListItem(dataModel, entry, showPasswords) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.dataModel = dataModel;
    el.__proto__ = PasswordListItem.prototype;
    el.decorate(showPasswords);

    return el;
  }

  PasswordListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /** @override */
    decorate: function(showPasswords) {
      InlineEditableItem.prototype.decorate.call(this);

      this.isPlaceholder = !this.url;

      this.contentElement.appendChild(this.createUrlElement());
      this.contentElement.appendChild(this.createUsernameElement());

      var passwordInputDiv = this.ownerDocument.createElement('div');
      passwordInputDiv.className = 'password';

      // The password input field.
      var passwordInput = this.ownerDocument.createElement('input');
      passwordInput.type = 'password';
      if (!this.isPlaceholder) {
        passwordInput.className = 'inactive-password';
        passwordInput.readOnly = true;
        passwordInput.value = showPasswords ? this.password : '********';
      }
      this.passwordField = passwordInput;

      // Makes the password input field editable.
      this.addEditField(passwordInput, null);

      // Keeps the password validity updated.
      this.updatePasswordValidity_();
      passwordInput.addEventListener('input', function(event) {
        this.updatePasswordValidity_();
      }.bind(this));

      passwordInputDiv.appendChild(passwordInput);

      // The list-inline buttons.
      if (!this.isPlaceholder) {
        // The container of the list-inline buttons.
        var buttonsContainer = this.ownerDocument.createElement('div');
        buttonsContainer.className = 'list-inline-buttons-container';

        var mousedownEventHandler = function(event) {
          // Don't focus on this button by mousedown.
          event.preventDefault();
          // Don't handle list item selection. It causes focus change.
          event.stopPropagation();
        };

        // The overwrite button.
        var overwriteButton = this.ownerDocument.createElement('button');
        overwriteButton.className = 'list-inline-button custom-appearance';
        overwriteButton.textContent = loadTimeData.getString(
            'passwordOverwriteButton');
        overwriteButton.addEventListener(
            'click', this.onClickOverwriteButton_.bind(this), true);
        overwriteButton.addEventListener(
            'mousedown', mousedownEventHandler, false);
        buttonsContainer.appendChild(overwriteButton);
        this.passwordOverwriteButton = overwriteButton;

        // The show/hide button.
        if (showPasswords) {
          var button = this.ownerDocument.createElement('button');
          button.className = 'list-inline-button custom-appearance';
          button.textContent = loadTimeData.getString('passwordShowButton');
          button.addEventListener(
              'click', this.onClickShowButton_.bind(this), true);
          button.addEventListener('mousedown', mousedownEventHandler, false);
          buttonsContainer.appendChild(button);
          this.passwordShowButton = button;
        }

        passwordInputDiv.appendChild(buttonsContainer);
      }

      this.contentElement.appendChild(passwordInputDiv);

      // Adds the event listeners for editing.
      this.addEventListener('canceledit', this.resetInputs);
      this.addEventListener('commitedit', this.finishEdit);
    },

    /**
     * Constructs and returns the URL element for this item.
     * @return {HTMLElement} The URL element.
     * @protected
     */
    createUrlElement: function() {
      var urlEl = this.ownerDocument.createElement('div');
      urlEl.className = 'favicon-cell weakrtl url';
      urlEl.setAttribute('title', this.url);
      urlEl.textContent = this.url;

      // The favicon URL is prefixed with "origin/", which essentially removes
      // the URL path past the top-level domain and ensures that a scheme (e.g.,
      // http) is being used. This ensures that the favicon returned is the
      // default favicon for the domain and that the URL has a scheme if none is
      // present in the password manager.
      urlEl.style.backgroundImage = getFaviconImageSet(
          'origin/' + this.url, 16);
      return urlEl;
    },

    /**
     * Constructs and returns the username element for this item.
     * @return {HTMLElement} The username element.
     * @protected
     */
    createUsernameElement: function() {
      var usernameEl = this.ownerDocument.createElement('div');
      usernameEl.className = 'name';
      usernameEl.textContent = this.username;
      usernameEl.title = this.username;
      return usernameEl;
    },

    /** @override */
    selectionChanged: function() {
      InlineEditableItem.prototype.selectionChanged.call(this);

      // Don't set 'inactive-password' class for the placeholder so that it
      // shows the background and the borders.
      if (this.isPlaceholder)
        return;

      this.passwordField.classList.toggle('inactive-password', !this.selected);
    },

    /** @override */
    get currentInputIsValid() {
      return !!this.passwordField.value;
    },

    /**
     * Returns if the password has been edited.
     * @return {boolean} Whether the password has been edited.
     * @protected
     */
    passwordHasBeenEdited: function() {
      return this.passwordField.value != this.password || this.overwriting;
    },

    /** @override */
    get hasBeenEdited() {
      return this.passwordHasBeenEdited();
    },

    /**
     * Reveals the plain text password of this entry. Never called for the Add
     * New Entry row.
     * @param {string} password The plain text password.
     */
    showPassword: function(password) {
      this.overwriting = false;
      this.password = password;
      this.setPasswordFieldValue_(password);
      this.passwordField.type = 'text';
      this.passwordField.readOnly = false;

      this.passwordOverwriteButton.hidden = true;
      var button = this.passwordShowButton;
      if (button)
        button.textContent = loadTimeData.getString('passwordHideButton');
    },

    /**
     * Hides the plain text password of this entry. Never called for the Add
     * New Entry row.
     * @private
     */
    hidePassword_: function() {
      this.passwordField.type = 'password';
      this.passwordField.readOnly = true;

      this.passwordOverwriteButton.hidden = false;
      var button = this.passwordShowButton;
      if (button)
        button.textContent = loadTimeData.getString('passwordShowButton');
    },

    /**
     * Resets the input fields to their original values and states.
     * @protected
     */
    resetInputs: function() {
      this.finishOverwriting_();
      this.setPasswordFieldValue_(this.password);
    },

    /**
     * Commits the new data to the browser.
     * @protected
     */
    finishEdit: function() {
      this.password = this.passwordField.value;
      this.finishOverwriting_();
      PasswordManager.updatePassword(
          this.getOriginalIndex_(), this.passwordField.value);
    },

    /**
     * Called with the response of the browser, which indicates the validity of
     * the URL.
     * @param {string} url The URL.
     * @param {boolean} valid The validity of the URL.
     */
    originValidityCheckComplete: function(url, valid) {
      assertNotReached();
    },

    /**
     * Updates the custom validity of the password input field.
     * @private
     */
    updatePasswordValidity_: function() {
      this.passwordField.setCustomValidity(this.passwordField.value ?
          '' : loadTimeData.getString('editPasswordInvalidPasswordTooltip'));
    },

    /**
     * Finishes password overwriting.
     * @private
     */
    finishOverwriting_: function() {
      if (!this.overwriting)
        return;
      this.overwriting = false;
      this.passwordOverwriteButton.hidden = false;
      this.passwordField.readOnly = true;
    },

    /**
     * Sets the value of the password input field.
     * @param {string} password The new value.
     * @private
     */
    setPasswordFieldValue_: function(password) {
      this.passwordField.value = password;
      this.updatePasswordValidity_();
    },

    /**
     * Get the original index of this item in the data model.
     * @return {number} The index.
     * @private
     */
    getOriginalIndex_: function() {
      var index = this.dataItem[3];
      return index ? index : this.dataModel.indexOf(this.dataItem);
    },

    /**
     * Called when clicking the overwrite button. Allows the user to overwrite
     * the hidden password.
     * @param {Event} event The click event.
     * @private
     */
    onClickOverwriteButton_: function(event) {
      this.overwriting = true;
      this.passwordOverwriteButton.hidden = true;

      this.setPasswordFieldValue_('');
      this.passwordField.readOnly = false;
      this.passwordField.focus();
    },

    /**
     * On-click event handler. Swaps the type of the input field from password
     * to text and back.
     * @private
     */
    onClickShowButton_: function(event) {
      // Prevents committing an edit.
      this.resetInputs();

      if (this.passwordField.type == 'password') {
        // After the user is authenticated, showPassword() will be called.
        PasswordManager.requestShowPassword(this.getOriginalIndex_());
      } else {
        this.hidePassword_();
      }
    },

    /**
     * Get and set the URL for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem[0] || '';
    },
    set url(url) {
      this.dataItem[0] = url;
    },

    /**
     * Get and set the username for the entry.
     * @type {string}
     */
    get username() {
      return this.dataItem[1] || '';
    },
    set username(username) {
      this.dataItem[1] = username;
    },

    /**
     * Get and set the password for the entry.
     * @type {string}
     */
    get password() {
      return this.dataItem[2] || '';
    },
    set password(password) {
      this.dataItem[2] = password;
    },
  };

  /**
   * Creates a new passwords list item for the Add New Entry row.
   * @param {ArrayDataModel} dataModel The data model that contains this item.
   * @constructor
   * @extends {options.passwordManager.PasswordListItem}
   */
  function PasswordAddRowListItem(dataModel) {
    var el = cr.doc.createElement('div');
    el.dataItem = [];
    el.dataModel = dataModel;
    el.__proto__ = PasswordAddRowListItem.prototype;
    el.decorate();

    return el;
  }

  PasswordAddRowListItem.prototype = {
    __proto__: PasswordListItem.prototype,

    /** @override */
    decorate: function() {
      PasswordListItem.prototype.decorate.call(this, false);

      this.urlField.placeholder = loadTimeData.getString(
          'newPasswordUrlFieldPlaceholder');
      this.usernameField.placeholder = loadTimeData.getString(
          'newPasswordUsernameFieldPlaceholder');
      this.passwordField.placeholder = loadTimeData.getString(
          'newPasswordPasswordFieldPlaceholder');

      // Sets the validity of the URL initially.
      this.setUrlValid_(false);
    },

    /** @override */
    createUrlElement: function() {
      var urlEl = this.createEditableTextCell('');
      urlEl.className += ' favicon-cell weakrtl url';

      var urlField = urlEl.querySelector('input');
      urlField.addEventListener('input', this.onUrlInput_.bind(this));
      this.urlField = urlField;

      return urlEl;
    },

    /** @override */
    createUsernameElement: function() {
      var usernameEl = this.createEditableTextCell('');
      usernameEl.className = 'name';

      this.usernameField = usernameEl.querySelector('input');

      return usernameEl;
    },

    /** @override */
    get currentInputIsValid() {
      return this.urlValidityKnown && this.urlIsValid &&
          this.passwordField.value;
    },

    /** @override */
    get hasBeenEdited() {
      return this.urlField.value || this.usernameField.value ||
          this.passwordHasBeenEdited();
    },

    /** @override */
    resetInputs: function() {
      PasswordListItem.prototype.resetInputs.call(this);

      this.urlField.value = '';
      this.usernameField.value = '';

      this.setUrlValid_(false);
    },

    /** @override */
    finishEdit: function() {
      var newUrl = this.urlField.value;
      var newUsername = this.usernameField.value;
      var newPassword = this.passwordField.value;
      this.resetInputs();

      PasswordManager.addPassword(newUrl, newUsername, newPassword);
    },

    /** @override */
    originValidityCheckComplete: function(url, valid) {
      if (url == this.urlField.value)
        this.setUrlValid_(valid);
    },

    /**
     * Updates whether the URL in the input is valid.
     * @param {boolean} valid The validity of the URL.
     * @private
     */
    setUrlValid_: function(valid) {
      this.urlIsValid = valid;
      this.urlValidityKnown = true;
      if (this.urlField) {
        this.urlField.setCustomValidity(valid ?
            '' : loadTimeData.getString('editPasswordInvalidUrlTooltip'));
      }
    },

    /**
     * Called when inputting to a URL input.
     * @param {Event} event The input event.
     * @private
     */
    onUrlInput_: function(event) {
      this.urlValidityKnown = false;
      PasswordManager.checkOriginValidityForAdding(this.urlField.value);
    },
  };

  /**
   * Creates a new PasswordExceptions list item.
   * @param {Array} entry A pair of the form [url, username].
   * @constructor
   * @extends {options.DeletableItem}
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
      urlLabel.className = 'url favicon-cell weakrtl';
      urlLabel.textContent = this.url;

      // The favicon URL is prefixed with "origin/", which essentially removes
      // the URL path past the top-level domain and ensures that a scheme (e.g.,
      // http) is being used. This ensures that the favicon returned is the
      // default favicon for the domain and that the URL has a scheme if none
      // is present in the password manager.
      urlLabel.style.backgroundImage = getFaviconImageSet(
          'origin/' + this.url, 16);
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
   * @extends {options.InlineEditableItemList}
   */
  var PasswordsList = cr.ui.define('list');

  PasswordsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * Whether passwords can be revealed or not.
     * @type {boolean}
     * @private
     */
    showPasswords_: true,

    /** @override */
    decorate: function() {
      InlineEditableItemList.prototype.decorate.call(this);
      Preferences.getInstance().addEventListener(
          'profile.password_manager_allow_show_passwords',
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

    /**
     * @override
     * @param {Array} entry
     */
    createItem: function(entry) {
      if (!entry)
        return new PasswordAddRowListItem(this.dataModel);

      var showPasswords = this.showPasswords_;

      if (loadTimeData.getBoolean('disableShowPasswords'))
        showPasswords = false;

      return new PasswordListItem(this.dataModel, entry, showPasswords);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var item = this.dataModel.item(index);
      if (item && item.length > 3) {
        // The fourth element, if present, is the original index to delete.
        index = item[3];
      }
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

    /**
     * @override
     * @param {Array} entry
     */
    createItem: function(entry) {
      return new PasswordExceptionsListItem(entry);
    },

    /** @override */
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
    PasswordAddRowListItem: PasswordAddRowListItem,
    PasswordExceptionsListItem: PasswordExceptionsListItem,
    PasswordsList: PasswordsList,
    PasswordExceptionsList: PasswordExceptionsList,
  };
});
