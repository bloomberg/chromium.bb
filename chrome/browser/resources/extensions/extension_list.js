// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
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

      // The 'Show Browser Action' button.
      if (extension.enable_show_button) {
        var showButton = node.querySelector('.show-button');
        showButton.addEventListener('click', function(e) {
          chrome.send('extensionSettingsShowButton', [extension.id]);
        });
        showButton.hidden = false;
      }

      // The 'allow in incognito' checkbox.
      var incognito = node.querySelector('.incognito-control');
      incognito.addEventListener('click', function(e) {
        var butterBar = node.querySelector('.butter-bar');
        var checked = e.target.checked;
        butterBar.hidden = !checked || extension.is_hosted_app;
        chrome.send('extensionSettingsEnableIncognito',
                    [extension.id, String(checked)]);
      });
      incognito.querySelector('input').checked = extension.enabledIncognito;

      // The 'allow file:// access' checkbox.
      if (extension.wantsFileAccess) {
        var fileAccess = node.querySelector('.file-access-control');
        fileAccess.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowFileAccess',
                      [extension.id, String(e.target.checked)]);
        });
        fileAccess.querySelector('input').checked = extension.allowFileAccess;
        fileAccess.hidden = false;
      }

      // The 'Options' checkbox.
      var options = node.querySelector('.options-link');
      options.addEventListener('click', function(e) {
        chrome.send('extensionSettingsOptions', [extension.id]);
        e.preventDefault();
      });

      // The 'View in Web Store' checkbox.
      if (extension.homepageUrl) {
        var store = node.querySelector('.store-link');
        store.href = extension.homepageUrl;
        store.hidden = false;
      }

      // The 'Reload' checkbox.
      if (extension.allow_reload) {
        var reload = node.querySelector('.reload-link');
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
        reload.hidden = false;
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
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.addEventListener('click', function(e) {
        chrome.send('extensionSettingsUninstall', [extension.id]);
      });
      node.querySelector('.enable-controls').appendChild(trash);

      // Developer mode details.
      if (this.data_.developerMode) {
        // First we have the id.
        var idLabel = node.querySelector('.extension-id');
        idLabel.textContent = ' ' + extension.id;

        // Then the path, if provided by unpacked extension.
        if (extension.isUnpacked) {
          var loadPath = node.querySelector('.load-path');
          loadPath.hidden = false;
          loadPath.querySelector('span:nth-of-type(2)').textContent =
              ' ' + extension.path;
        }

        // Then the 'managed, cannot uninstall/disable' message.
        if (!extension.mayDisable)
          node.querySelector('.managed-message').hidden = false;

        // Then active views.
        if (extension.views.length > 0) {
          var activeViews = node.querySelector('.active-views');
          activeViews.hidden = false;
          var link = activeViews.querySelector('a');

          for (var i = 0; i < extension.views.length; ++i) {
            var view = extension.views[i];

            var label = view.path + (view.incognito ?
                ' ' + localStrings.getString('viewIncognito') : '');
            link.textContent = label;
            link.addEventListener('click', function(e) {
              // TODO(estade): remove conversion to string?
              chrome.send('extensionSettingsInspect', [
                String(view.renderProcessId),
                String(view.renderViewId)
              ]);
            });

            if (i < extension.views.length - 1) {
              link = link.cloneNode(true);
              activeViews.appendChild(link);
            }
          }
        }

        node.querySelector('.developer-extras').hidden = false;
      }

      this.appendChild(node);
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
