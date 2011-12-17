// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  'use strict';

  /**
   * A lookup helper function to find the first node that has an id (starting
   * at |node| and going up the parent chain).
   * @param {Element} node The node to start looking at.
   */
  function findIdNode(node) {
    while (node && !node.id) {
      node = node.parentNode;
    }
    return node;
  }

  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionsList = cr.ui.define('div');

  var handlersInstalled = false;

  /**
   * @type {Object.<string, boolean>} A map from extension id to a boolean
   *     indicating whether its details section is expanded. This persists
   *     between calls to decorate.
   */
  var showingDetails = {};

  /**
   * @type {Object.<string, boolean>} A map from extension id to a boolean
   *     indicating whether the incognito warning is showing. This persists
   *     between calls to decorate.
   */
  var showingWarning = {};

  ExtensionsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.initControlsAndHandlers_();

      this.textContent = '';

      this.showExtensionNodes_();
    },

    /**
     * Initializes the controls (toggle section and button) and installs
     * handlers.
     * @private
     */
    initControlsAndHandlers_: function() {
      // Make sure developer mode section is set correctly as per saved setting.
      var toggleButton = $('toggle-dev-on');
      var toggleSection = $('dev');
      if (this.data_.developerMode) {
        toggleSection.classList.add('dev-open');
        toggleSection.classList.remove('dev-closed');
        toggleButton.checked = true;
      } else {
        toggleSection.classList.remove('dev-open');
        toggleSection.classList.add('dev-closed');
      }
    },

    /**
     * Creates all extension items from scratch.
     * @private
     */
     showExtensionNodes_: function() {
      // Iterate over the extension data and add each item to the list.
      var list = this;
      this.data_.extensions.forEach(this.createNode_.bind(this));
     },

     /**
      * Synthesizes and initializes an HTML element for the extension metadata
      * given in |extension|.
      * @param {Object} extension A dictionary of extension metadata.
      * @private
      */
     createNode_: function(extension) {
      var template = document.querySelector('#extension-item-template');
      var node = template.cloneNode(true);
      node.hidden = false;
      node.id = extension.id;

      if (!extension.enabled)
        node.classList.add('disabled-extension');

      var item = node.querySelector('.extension-list-item');
      item.style.backgroundImage = 'url(' + extension.icon + ')';

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      var version = node.querySelector('.extension-version');
      version.textContent = extension.version;

      var description = node.querySelector('.extension-description');
      description.textContent = extension.description;

      // The 'allow in incognito' checkbox.
      var incognito = node.querySelector('.incognito-checkbox');
      incognito.addEventListener('click', function(e) {
        var butterBar = node.querySelector('.butter-bar');
        var checked = e.target.checked;
        butterBar.hidden = !checked || extension.is_hosted_app;
        chrome.send('extensionSettingsEnableIncognito',
                    [extension.id, String(checked)]);
      });
      incognito.querySelector('input').checked = extension.enabledIncognito;

      // The 'allow file:// access' checkbox.
      var fileAccess = node.querySelector('.file-access-checkbox');
      if (extension.wantsFileAccess) {
        fileAccess.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowFileAccess',
                      [extension.id, String(e.target.checked)]);
        });
        fileAccess.querySelector('input').checked = extension.allowFileAccess;
      } else {
        fileAccess.hidden = true;
      }

      // The 'Options' checkbox.
      var options = node.querySelector('.options-link');
      options.addEventListener('click', function(e) {
        chrome.send('extensionSettingsOptions', [extension.id]);
        e.preventDefault();
      });

      // The 'View in Web Store' checkbox.
      var store = node.querySelector('.store-link');
      if (extension.homepageUrl)
        store.href = extension.homepageUrl;
      else
        store.hidden = true;

      // The 'Reload' checkbox.
      var reload = node.querySelector('.reload-link');
      if (extension.allow_reload) {
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
      } else {
        reload.hidden = true;
      }

      if (!extension.terminated) {
        // The 'Enabled' checkbox.
        var enable = node.querySelector('.enable-checkbox');
        enable.hidden = false;
        enable.addEventListener('click', function(e) {
          chrome.send('extensionSettingsEnable',
                      [extension.id, e.target.checked ? 'true' : 'false']);
          chrome.send('extensionSettingsRequestExtensionsData');
        });
        enable.querySelector('input').checked = extension.enabled;

        // Make sure the checkbox doesn't move around when its value changes.
        var enableText = node.querySelector('.enable-checkbox-text');
        enableText.style.minWidth = 0.8 * Math.max(
            localStrings.getString('extensionSettingsEnabled').length,
            localStrings.getString('extensionSettingsEnable').length) + 'em';
      } else {
        var terminated_reload = node.querySelector('.terminated-reload-link');
        terminated_reload.hidden = false;
        terminated_reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
      }

      // 'Remove' button.
      var button = node.querySelector('.remove-button');
      button.addEventListener('click', function(e) {
        chrome.send('extensionSettingsUninstall', [extension.id]);
      });

      this.appendChild(node);
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
