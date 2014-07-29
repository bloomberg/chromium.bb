// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('main', [
    'mojo/public/js/bindings/connection',
    'chrome/browser/ui/webui/identity_internals/identity_internals.mojom',
], function(connector, browser) {
  'use strict';

  var connection;
  var page;

  /**
   * Creates an identity token item.
   * @param {!Object} tokenInfo Object containing token information.
   * @constructor
   */
  function TokenListItem(tokenInfo) {
    var el = cr.doc.createElement('div');
    el.data_ = tokenInfo;
    el.__proto__ = TokenListItem.prototype;
    el.decorate();
    return el;
  }

  TokenListItem.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.textContent = '';
      this.id = this.data_.access_token;

      var table = this.ownerDocument.createElement('table');
      var tbody = this.ownerDocument.createElement('tbody');
      tbody.appendChild(this.createEntry_(
          'accessToken', this.data_.access_token, 'access-token'));
      tbody.appendChild(this.createEntry_(
          'extensionName', this.data_.extension_name, 'extension-name'));
      tbody.appendChild(this.createEntry_(
          'extensionId', this.data_.extension_id, 'extension-id'));
      tbody.appendChild(this.createEntry_(
          'tokenStatus', this.data_.token_status, 'token-status'));
      tbody.appendChild(this.createEntry_(
          'expirationTime', this.data_.expiration_time, 'expiration-time'));
      tbody.appendChild(this.createEntryForScopes_());
      table.appendChild(tbody);
      var tfoot = this.ownerDocument.createElement('tfoot');
      tfoot.appendChild(this.createButtons_());
      table.appendChild(tfoot);
      this.appendChild(table);
    },

    /**
     * Creates an entry for a single property of the token.
     * @param {string} label An i18n label of the token's property name.
     * @param {string} value A value of the token property.
     * @param {string} accessor Additional class to tag the field for testing.
     * @return {HTMLElement} An HTML element with the property name and value.
     */
    createEntry_: function(label, value, accessor) {
      var row = this.ownerDocument.createElement('tr');
      var labelField = this.ownerDocument.createElement('td');
      labelField.classList.add('label');
      labelField.textContent = loadTimeData.getString(label);
      row.appendChild(labelField);
      var valueField = this.ownerDocument.createElement('td');
      valueField.classList.add('value');
      valueField.classList.add(accessor);
      valueField.textContent = value;
      row.appendChild(valueField);
      return row;
    },

    /**
     * Creates an entry for a list of token scopes.
     * @return {!HTMLElement} An HTML element with scopes.
     */
    createEntryForScopes_: function() {
      var row = this.ownerDocument.createElement('tr');
      var labelField = this.ownerDocument.createElement('td');
      labelField.classList.add('label');
      labelField.textContent = loadTimeData.getString('scopes');
      row.appendChild(labelField);
      var valueField = this.ownerDocument.createElement('td');
      valueField.classList.add('value');
      valueField.classList.add('scope-list');
      this.data_.scopes.forEach(function(scope) {
        valueField.appendChild(this.ownerDocument.createTextNode(scope));
        valueField.appendChild(this.ownerDocument.createElement('br'));
      }, this);
      row.appendChild(valueField);
      return row;
    },

    /**
     * Creates buttons for the token.
     * @return {HTMLElement} An HTML element with actionable buttons for the
     *     token.
     */
    createButtons_: function() {
      var row = this.ownerDocument.createElement('tr');
      var buttonHolder = this.ownerDocument.createElement('td');
      buttonHolder.colSpan = 2;
      buttonHolder.classList.add('token-actions');
      buttonHolder.appendChild(this.createRevokeButton_());
      row.appendChild(buttonHolder);
      return row;
    },

    /**
     * Creates a revoke button with an event sending a revoke token message
     * to the controller.
     * @return {!HTMLButtonElement} The created revoke button.
     * @private
     */
    createRevokeButton_: function() {
      var revokeButton = this.ownerDocument.createElement('button');
      revokeButton.classList.add('revoke-button');
      revokeButton.addEventListener('click', function() {
        var accessToken = this.data_.access_token;
        page.browser_.revokeToken(this.data_.extension_id,
                                  accessToken).then(function() {
                                      tokenList_.removeTokenNode_(accessToken);
                                      if (window.revokeTokenTest)
                                          window.revokeTokenTest();
                                  });
      }.bind(this));
      revokeButton.textContent = loadTimeData.getString('revoke');
      return revokeButton;
    },
  };

  /**
   * Creates a new list of identity tokens.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var TokenList = cr.ui.define('div');

  TokenList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.textContent = '';
      this.showTokenNodes_();
    },

    /**
     * Populates the list of tokens.
     */
    showTokenNodes_: function() {
      this.data_.forEach(function(tokenInfo) {
        this.appendChild(new TokenListItem(tokenInfo));
      }, this);
    },

    /**
     * Removes a token node related to the specifed token ID from both the
     * internals data source as well as the user internface.
     * @param {string} accessToken The id of the token to remove.
     * @private
     */
    removeTokenNode_: function(accessToken) {
      var tokenIndex;
      for (var index = 0; index < this.data_.length; index++) {
        if (this.data_[index].access_token == accessToken) {
          tokenIndex = index;
          break;
        }
      }

      // Remove from the data_ source if token found.
      if (tokenIndex)
        this.data_.splice(tokenIndex, 1);

      // Remove from the user interface.
      var tokenNode = $(accessToken);
      if (tokenNode)
        this.removeChild(tokenNode);
    },
  };

  var tokenList_;

  function InternalsPageImpl(browser) {
    this.browser_ = browser;
    page = this;

    tokenList_ = $('token-list');
    tokenList_.data_ = [];
    tokenList_.__proto__ = TokenList.prototype;
    tokenList_.decorate();

    page.browser_.getTokens().then(function(results) {
        tokenList_.data_ = results.tokens;
        tokenList_.showTokenNodes_();
    });
  }

  InternalsPageImpl.prototype =
      Object.create(browser.InternalsPageStub.prototype);

  return function(handle) {
    connection = new connector.Connection(
        handle,
        InternalsPageImpl,
        browser.IdentityInternalsHandlerMojoProxy);
  };
});
