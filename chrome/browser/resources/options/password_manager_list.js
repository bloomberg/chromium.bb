// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.passwordManager', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var List = cr.ui.List;

  // The following constants should be synchronized with the constants in
  // chrome/browser/ui/webui/options/password_manager_handler.cc.
  /** @const */ var ORIGIN_FIELD = 'origin';
  /** @const */ var SHOWN_URL_FIELD = 'shownUrl';
  /** @const */ var IS_ANDROID_URI_FIELD = 'isAndroidUri';
  /** @const */ var IS_SECURE_FIELD = 'isSecure';
  /** @const */ var USERNAME_FIELD = 'username';
  /** @const */ var PASSWORD_FIELD = 'password';
  /** @const */ var FEDERATION_FIELD = 'federation';
  /** @const */ var ORIGINAL_INDEX_FIELD = 'index';

  /**
   * Creates a new passwords list item.
   * @param {cr.ui.ArrayDataModel} dataModel The data model that contains this
   *     item.
   * @param {Object} entry A dictionary of data on new list item. When the
   *     list has been filtered, one more element [index] may be present.
   * @param {boolean} showPasswords If true, add a button to the element to
   *     allow the user to reveal the saved password.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function PasswordListItem(dataModel, entry, showPasswords) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.dataModel = dataModel;
    el.__proto__ = PasswordListItem.prototype;
    el.showPasswords_ = showPasswords;
    el.decorate();

    return el;
  }

  /**
   * Returns title for password's origin. If the origin is  Android URI, returns
   * the origin as it is. Removes the scheme if the url is insecure and removes
   * trailing punctuation symbols.
   * @param {Object} item A dictionary of data on the list item.
   * @return {string} The title for password's origin.
   */
  function getTitleForPasswordOrigin(item) {
    var title = item.url;
    if (item.isAndroidUri)
      return title;
    if (!item.isSecure) {
      var ind = title.indexOf('://');
      if (ind >= 0) {
        title = title.substring(ind + 3);
      }
    }
    // Since the direction is switched to RTL, punctuation symbols appear on the
    // left side, that is wrong. So, just remove trailing punctuation symbols.
    title = title.replace(/[^A-Za-z0-9]+$/, '');
    return title;
  }

  /**
   * Helper function that creates an HTML element for displaying the origin of
   * saved password.
   * @param {Object} item A dictionary of data on the list item.
   * @param {Element} urlDiv div-element that will enclose the created
   *     element.
   * @return {Element} The element for displaying password origin.
   */
  function createUrlLink(item, urlDiv) {
    var urlLink;
    if (!item.isAndroidUri) {
      urlLink = item.ownerDocument.createElement('a');
      urlLink.href = item.url;
      urlLink.setAttribute('target', '_blank');
      urlLink.textContent = item.shownUrl.split('').reverse().join('');

      urlDiv.classList.add('left-elided-url');
    } else {
      urlLink = item.ownerDocument.createElement('span');
      urlLink.textContent = item.shownUrl;
    }
    urlLink.addEventListener('focus', function() {
      item.handleFocus();
    }.bind(item));
    return urlLink;
  }

  PasswordListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The URL of the site.
      var urlDiv = this.ownerDocument.createElement('div');
      urlDiv.className = 'favicon-cell url';
      urlDiv.setAttribute('title', getTitleForPasswordOrigin(this));
      urlDiv.style.backgroundImage = getFaviconImageSet(
          'origin/' + this.url, 16);

      this.urlLink = createUrlLink(this, urlDiv);
      urlDiv.appendChild(this.urlLink);

      this.contentElement.appendChild(urlDiv);

      // The stored username.
      var usernameDiv = this.ownerDocument.createElement('div');
      usernameDiv.className = 'name';
      usernameDiv.title = this.username;
      this.contentElement.appendChild(usernameDiv);
      var usernameInput = this.ownerDocument.createElement('input');
      usernameInput.type = 'text';
      usernameInput.className = 'inactive-item';
      usernameInput.readOnly = true;
      usernameInput.value = this.username;
      usernameInput.addEventListener('focus', function() {
        this.handleFocus();
      }.bind(this));
      usernameDiv.appendChild(usernameInput);
      this.usernameField = usernameInput;

      if (this.federation) {
        // The federation.
        var federationDiv = this.ownerDocument.createElement('div');
        federationDiv.className = 'federation';
        federationDiv.textContent = this.federation;
        federationDiv.title = this.federation;
        this.contentElement.appendChild(federationDiv);
      } else {
        // The stored password.
        var passwordInputDiv = this.ownerDocument.createElement('div');
        passwordInputDiv.className = 'password';

        // The password input field.
        var passwordInput = this.ownerDocument.createElement('input');
        passwordInput.type = 'password';
        passwordInput.className = 'inactive-item';
        passwordInput.readOnly = true;
        passwordInput.value = this.showPasswords_ ? this.password : '********';
        passwordInputDiv.appendChild(passwordInput);
        passwordInput.addEventListener('focus', function() {
          this.handleFocus();
        }.bind(this));
        this.passwordField = passwordInput;

        // The show/hide button.
        if (this.showPasswords_) {
          var button = this.ownerDocument.createElement('button');
          button.hidden = true;
          button.className = 'list-inline-button custom-appearance';
          button.textContent = loadTimeData.getString('passwordShowButton');
          button.addEventListener('click', this.onClick_.bind(this), true);
          button.addEventListener('mousedown', function(event) {
            // Don't focus on this button by mousedown.
            event.preventDefault();
            // Don't handle list item selection. It causes focus change.
            event.stopPropagation();
          }, false);
          button.addEventListener('focus', function() {
            this.handleFocus();
          }.bind(this));
          passwordInputDiv.appendChild(button);
          this.passwordShowButton = button;
        }
        this.contentElement.appendChild(passwordInputDiv);
      }
      this.setFocusable_(false);
    },

    /** @override */
    selectionChanged: function() {
      var usernameInput = this.usernameField;
      var passwordInput = this.passwordField;
      var button = this.passwordShowButton;

      this.setFocusable_(this.selected);

      if (this.selected) {
        usernameInput.classList.remove('inactive-item');
        if (button) {
          passwordInput.classList.remove('inactive-item');
          button.hidden = false;
          passwordInput.focus();
        } else {
          usernameInput.focus();
        }
      } else {
        usernameInput.classList.add('inactive-item');
        if (button) {
          passwordInput.classList.add('inactive-item');
          button.hidden = true;
        }
      }
    },

    /**
     * Set the focusability of this row.
     * @param {boolean} focusable
     * @private
     */
    setFocusable_: function(focusable) {
      var tabIndex = focusable ? 0 : -1;
      this.urlLink.tabIndex = tabIndex;
      this.usernameField.tabIndex = tabIndex;
      this.closeButtonElement.tabIndex = tabIndex;
      if (this.passwordShowButton)
        this.passwordField.tabIndex = tabIndex;
    },

    /**
     * Reveals the plain text password of this entry.
     */
    showPassword: function(password) {
      this.passwordField.value = password;
      this.passwordField.type = 'text';

      var button = this.passwordShowButton;
      if (button)
        button.textContent = loadTimeData.getString('passwordHideButton');
    },

    /**
     * Hides the plain text password of this entry.
     */
    hidePassword: function() {
      this.passwordField.type = 'password';

      var button = this.passwordShowButton;
      if (button)
        button.textContent = loadTimeData.getString('passwordShowButton');
    },

    /**
     * Get the original index of this item in the data model.
     * @return {number} The index.
     * @private
     */
    getOriginalIndex_: function() {
      var index = this.dataItem[ORIGINAL_INDEX_FIELD];
      return index ? index : this.dataModel.indexOf(this.dataItem);
    },

    /**
     * On-click event handler. Swaps the type of the input field from password
     * to text and back.
     * @private
     */
    onClick_: function(event) {
      if (this.passwordField.type == 'password') {
        // After the user is authenticated, showPassword() will be called.
        PasswordManager.requestShowPassword(this.getOriginalIndex_());
      } else {
        this.hidePassword();
      }
    },

    /**
     * Get and set the URL for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem[ORIGIN_FIELD];
    },
    set url(url) {
      this.dataItem[ORIGIN_FIELD] = url;
    },

    /**
     * Get and set the shown url for the entry.
     * @type {string}
     */
    get shownUrl() {
      return this.dataItem[SHOWN_URL_FIELD];
    },
    set shownUrl(shownUrl) {
      this.dataItem[SHOWN_URL_FIELD] = shownUrl;
    },

    /**
     * Get and set whether the origin is Android URI.
     * @type {boolean}
     */
    get isAndroidUri() {
      return this.dataItem[IS_ANDROID_URI_FIELD];
    },
    set isAndroidUri(isAndroidUri) {
      this.dataItem[IS_ANDROID_URI_FIELD] = isAndroidUri;
    },

    /**
     * Get and set whether the origin uses secure scheme.
     * @type {boolean}
     */
    get isSecure() {
      return this.dataItem[IS_SECURE_FIELD];
    },
    set isSecure(isSecure) {
      this.dataItem[IS_SECURE_FIELD] = isSecure;
    },

    /**
     * Get and set the username for the entry.
     * @type {string}
     */
    get username() {
      return this.dataItem[USERNAME_FIELD];
    },
    set username(username) {
      this.dataItem[USERNAME_FIELD] = username;
    },

    /**
     * Get and set the password for the entry.
     * @type {string}
     */
    get password() {
      return this.dataItem[PASSWORD_FIELD];
    },
    set password(password) {
      this.dataItem[PASSWORD_FIELD] = password;
    },

    /**
     * Get and set the federation for the entry.
     * @type {string}
     */
    get federation() {
      return this.dataItem[FEDERATION_FIELD];
    },
    set federation(federation) {
      this.dataItem[FEDERATION_FIELD] = federation;
    },
  };

  /**
   * Creates a new PasswordExceptions list item.
   * @param {Object} entry A dictionary of data on new list item.
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
      var urlDiv = this.ownerDocument.createElement('div');
      urlDiv.className = 'favicon-cell url';
      urlDiv.setAttribute('title', getTitleForPasswordOrigin(this));
      urlDiv.style.backgroundImage = getFaviconImageSet(
        'origin/' + this.url, 16);

      this.urlLink = createUrlLink(this, urlDiv);
      urlDiv.appendChild(this.urlLink);

      this.contentElement.appendChild(urlDiv);
    },

    /** @override */
    selectionChanged: function() {
      if (this.selected) {
        this.setFocusable_(true);
        this.urlLink.focus();
      } else {
        this.setFocusable_(false);
      }
    },

    /**
     * Set the focusability of this row.
     * @param {boolean} focusable
     * @private
     */
    setFocusable_: function(focusable) {
      var tabIndex = focusable ? 0 : -1;
      this.urlLink.tabIndex = tabIndex;
      this.closeButtonElement.tabIndex = tabIndex;
    },

    /**
     * Get the url for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem[ORIGIN_FIELD];
    },
    set url(url) {
      this.dataItem[ORIGIN_FIELD] = url;
    },

    /**
     * Get and set the shown url for the entry.
     * @type {string}
     */
    get shownUrl() {
      return this.dataItem[SHOWN_URL_FIELD];
    },
    set shownUrl(shownUrl) {
      this.dataItem[SHOWN_URL_FIELD] = shownUrl;
    },

    /**
     * Get and set whether the origin is Android URI.
     * @type {boolean}
     */
    get isAndroidUri() {
      return this.dataItem[IS_ANDROID_URI_FIELD];
    },
    set isAndroidUri(isAndroidUri) {
      this.dataItem[IS_ANDROID_URI_FIELD] = isAndroidUri;
    },

    /**
     * Get and set whether the origin uses secure scheme.
     * @type {boolean}
     */
    get isSecure() {
      return this.dataItem[IS_SECURE_FIELD];
    },
    set isSecure(isSecure) {
      this.dataItem[IS_SECURE_FIELD] = isSecure;
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {options.DeletableItemList}
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

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      Preferences.getInstance().addEventListener(
          'profile.password_manager_allow_show_passwords',
          this.onPreferenceChanged_.bind(this));
      this.addEventListener('focus', this.onFocus_.bind(this));
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
      var showPasswords = this.showPasswords_;

      if (loadTimeData.getBoolean('disableShowPasswords'))
        showPasswords = false;

      return new PasswordListItem(this.dataModel, entry, showPasswords);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var item = this.dataModel.item(index);
      if (item && item[ORIGINAL_INDEX_FIELD] != undefined) {
        // The fifth element, if present, is the original index to delete.
        index = item[ORIGINAL_INDEX_FIELD];
      }
      PasswordManager.removeSavedPassword(index);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

    /**
     * Will make to first row focusable if none are selected. This makes it
     * possible to tab into the rows without pressing up/down first.
     * @param {Event} e The focus event.
     * @private
     */
    onFocus_: function(e) {
      if (!this.selectedItem && this.items)
        this.items[0].setFocusable_(true);
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {options.DeletableItemList}
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
    PasswordExceptionsListItem: PasswordExceptionsListItem,
    PasswordsList: PasswordsList,
    PasswordExceptionsList: PasswordExceptionsList,
    ORIGIN_FIELD: ORIGIN_FIELD,
    SHOWN_URL_FIELD: SHOWN_URL_FIELD,
    IS_SECURE_FIELD: IS_SECURE_FIELD,
    USERNAME_FIELD: USERNAME_FIELD,
    PASSWORD_FIELD: PASSWORD_FIELD,
    FEDERATION_FIELD: FEDERATION_FIELD,
    ORIGINAL_INDEX_FIELD: ORIGINAL_INDEX_FIELD
  };
});
