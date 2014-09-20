// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="extension_error.js">

/**
 * The type of the extension data object. The definition is based on
 * chrome/browser/ui/webui/extensions/extension_basic_info.cc
 * and
 * chrome/browser/ui/webui/extensions/extension_settings_handler.cc
 *     ExtensionSettingsHandler::CreateExtensionDetailValue()
 * @typedef {{allow_reload: boolean,
 *            allowAllUrls: boolean,
 *            allowFileAccess: boolean,
 *            blacklistText: string,
 *            corruptInstall: boolean,
 *            dependentExtensions: Array,
 *            description: string,
 *            detailsUrl: string,
 *            enable_show_button: boolean,
 *            enabled: boolean,
 *            enabledIncognito: boolean,
 *            errorCollectionEnabled: (boolean|undefined),
 *            hasPopupAction: boolean,
 *            homepageProvided: boolean,
 *            homepageUrl: string,
 *            icon: string,
 *            id: string,
 *            incognitoCanBeEnabled: boolean,
 *            installWarnings: (Array|undefined),
 *            is_hosted_app: boolean,
 *            is_platform_app: boolean,
 *            isFromStore: boolean,
 *            isUnpacked: boolean,
 *            kioskEnabled: boolean,
 *            kioskOnly: boolean,
 *            locationText: string,
 *            managedInstall: boolean,
 *            manifestErrors: (Array.<RuntimeError>|undefined),
 *            name: string,
 *            offlineEnabled: boolean,
 *            optionsUrl: string,
 *            order: number,
 *            packagedApp: boolean,
 *            path: (string|undefined),
 *            prettifiedPath: (string|undefined),
 *            runtimeErrors: (Array.<RuntimeError>|undefined),
 *            suspiciousInstall: boolean,
 *            terminated: boolean,
 *            version: string,
 *            views: Array.<{renderViewId: number, renderProcessId: number,
 *                path: string, incognito: boolean,
 *                generatedBackgroundPage: boolean}>,
 *            wantsAllUrls: boolean,
 *            wantsErrorCollection: boolean,
 *            wantsFileAccess: boolean,
 *            warnings: (Array|undefined)}}
 */
var ExtensionData;

cr.define('options', function() {
  'use strict';

  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var ExtensionsList = cr.ui.define('div');

  /**
   * @type {Object.<string, boolean>} A map from extension id to a boolean
   *     indicating whether the incognito warning is showing. This persists
   *     between calls to decorate.
   */
  var butterBarVisibility = {};

  /**
   * @type {Object.<string, number>} A map from extension id to last reloaded
   *     timestamp. The timestamp is recorded when the user click the 'Reload'
   *     link. It is used to refresh the icon of an unpacked extension.
   *     This persists between calls to decorate.
   */
  var extensionReloadedTimestamp = {};

  ExtensionsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Indicates whether an embedded options page that was navigated to through
     * the '?options=' URL query has been shown to the user. This is necessary
     * to prevent showExtensionNodes_ from opening the options more than once.
     * @type {boolean}
     * @private
     */
    optionsShown_: false,

    /** @override */
    decorate: function() {
      this.textContent = '';

      this.showExtensionNodes_();
    },

    getIdQueryParam_: function() {
      return parseQueryParams(document.location)['id'];
    },

    getOptionsQueryParam_: function() {
      return parseQueryParams(document.location)['options'];
    },

    /**
     * Creates all extension items from scratch.
     * @private
     */
    showExtensionNodes_: function() {
      // Iterate over the extension data and add each item to the list.
      this.data_.extensions.forEach(this.createNode_, this);

      var idToHighlight = this.getIdQueryParam_();
      if (idToHighlight && $(idToHighlight))
        this.scrollToNode_(idToHighlight);

      var idToOpenOptions = this.getOptionsQueryParam_();
      if (idToOpenOptions && $(idToOpenOptions))
        this.showEmbeddedExtensionOptions_(idToOpenOptions, true);

      if (this.data_.extensions.length == 0)
        this.classList.add('empty-extension-list');
      else
        this.classList.remove('empty-extension-list');
    },

    /**
     * Scrolls the page down to the extension node with the given id.
     * @param {string} extensionId The id of the extension to scroll to.
     * @private
     */
    scrollToNode_: function(extensionId) {
      // Scroll offset should be calculated slightly higher than the actual
      // offset of the element being scrolled to, so that it ends up not all
      // the way at the top. That way it is clear that there are more elements
      // above the element being scrolled to.
      var scrollFudge = 1.2;
      var scrollTop = $(extensionId).offsetTop - scrollFudge *
          $(extensionId).clientHeight;
      setScrollTopForDocument(document, scrollTop);
    },

    /**
     * Synthesizes and initializes an HTML element for the extension metadata
     * given in |extension|.
     * @param {ExtensionData} extension A dictionary of extension metadata.
     * @private
     */
    createNode_: function(extension) {
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
      node.id = extension.id;

      if (!extension.enabled || extension.terminated)
        node.classList.add('inactive-extension');

      if (extension.managedInstall ||
          extension.dependentExtensions.length > 0) {
        node.classList.add('may-not-modify');
        node.classList.add('may-not-remove');
      } else if (extension.suspiciousInstall || extension.corruptInstall) {
        node.classList.add('may-not-modify');
      }

      var idToHighlight = this.getIdQueryParam_();
      if (node.id == idToHighlight)
        node.classList.add('extension-highlight');

      var item = node.querySelector('.extension-list-item');
      // Prevent the image cache of extension icon by using the reloaded
      // timestamp as a query string. The timestamp is recorded when the user
      // clicks the 'Reload' link. http://crbug.com/159302.
      if (extensionReloadedTimestamp[extension.id]) {
        item.style.backgroundImage =
            'url(' + extension.icon + '?' +
            extensionReloadedTimestamp[extension.id] + ')';
      } else {
        item.style.backgroundImage = 'url(' + extension.icon + ')';
      }

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      var version = node.querySelector('.extension-version');
      version.textContent = extension.version;

      var locationText = node.querySelector('.location-text');
      locationText.textContent = extension.locationText;

      var blacklistText = node.querySelector('.blacklist-text');
      blacklistText.textContent = extension.blacklistText;

      var description = document.createElement('span');
      description.textContent = extension.description;
      node.querySelector('.extension-description').appendChild(description);

      // The 'Show Browser Action' button.
      if (extension.enable_show_button) {
        var showButton = node.querySelector('.show-button');
        showButton.addEventListener('click', function(e) {
          chrome.send('extensionSettingsShowButton', [extension.id]);
        });
        showButton.hidden = false;
      }

      // The 'allow in incognito' checkbox.
      node.querySelector('.incognito-control').hidden =
          !this.data_.incognitoAvailable;
      var incognito = node.querySelector('.incognito-control input');
      incognito.disabled = !extension.incognitoCanBeEnabled;
      incognito.checked = extension.enabledIncognito;
      if (!incognito.disabled) {
        incognito.addEventListener('change', function(e) {
          var checked = e.target.checked;
          butterBarVisibility[extension.id] = checked;
          butterBar.hidden = !checked || extension.is_hosted_app;
          chrome.send('extensionSettingsEnableIncognito',
                      [extension.id, String(checked)]);
        });
      }
      var butterBar = node.querySelector('.butter-bar');
      butterBar.hidden = !butterBarVisibility[extension.id];

      // The 'collect errors' checkbox. This should only be visible if the
      // error console is enabled - we can detect this by the existence of the
      // |errorCollectionEnabled| property.
      if (extension.wantsErrorCollection) {
        node.querySelector('.error-collection-control').hidden = false;
        var errorCollection =
            node.querySelector('.error-collection-control input');
        errorCollection.checked = extension.errorCollectionEnabled;
        errorCollection.addEventListener('change', function(e) {
          chrome.send('extensionSettingsEnableErrorCollection',
                      [extension.id, String(e.target.checked)]);
        });
      }

      // The 'allow on all urls' checkbox. This should only be visible if
      // active script restrictions are enabled. If they are not enabled, no
      // extensions should want all urls.
      if (extension.wantsAllUrls) {
        var allUrls = node.querySelector('.all-urls-control');
        allUrls.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowOnAllUrls',
                      [extension.id, String(e.target.checked)]);
        });
        allUrls.querySelector('input').checked = extension.allowAllUrls;
        allUrls.hidden = false;
      }

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

      // The 'Options' link.
      if (extension.enabled && extension.optionsUrl) {
        var options = node.querySelector('.options-link');
        options.addEventListener('click', function(e) {
          if (!extension.optionsOpenInTab) {
            this.showEmbeddedExtensionOptions_(extension.id, false);
          } else {
            chrome.send('extensionSettingsOptions', [extension.id]);
          }
          e.preventDefault();
        }.bind(this));
        options.hidden = false;
      }

      // The 'Permissions' link.
      var permissions = node.querySelector('.permissions-link');
      permissions.addEventListener('click', function(e) {
        chrome.send('extensionSettingsPermissions', [extension.id]);
        e.preventDefault();
      });

      // The 'View in Web Store/View Web Site' link.
      if (extension.homepageUrl) {
        var siteLink = node.querySelector('.site-link');
        siteLink.href = extension.homepageUrl;
        siteLink.textContent = loadTimeData.getString(
                extension.homepageProvided ? 'extensionSettingsVisitWebsite' :
                                             'extensionSettingsVisitWebStore');
        siteLink.hidden = false;
      }

      if (extension.allow_reload) {
        // The 'Reload' link.
        var reload = node.querySelector('.reload-link');
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
          extensionReloadedTimestamp[extension.id] = Date.now();
        });
        reload.hidden = false;

        if (extension.is_platform_app) {
          // The 'Launch' link.
          var launch = node.querySelector('.launch-link');
          launch.addEventListener('click', function(e) {
            chrome.send('extensionSettingsLaunch', [extension.id]);
          });
          launch.hidden = false;
        }
      }

      if (extension.terminated) {
        var terminatedReload = node.querySelector('.terminated-reload-link');
        terminatedReload.hidden = false;
        terminatedReload.onclick = function() {
          chrome.send('extensionSettingsReload', [extension.id]);
        };
      } else if (extension.corruptInstall && extension.isFromStore) {
        var repair = node.querySelector('.corrupted-repair-button');
        repair.hidden = false;
        repair.onclick = function() {
          chrome.send('extensionSettingsRepair', [extension.id]);
        };
      } else {
        // The 'Enabled' checkbox.
        var enable = node.querySelector('.enable-checkbox');
        enable.hidden = false;
        var enableCheckboxDisabled = extension.managedInstall ||
                                     extension.suspiciousInstall ||
                                     extension.corruptInstall ||
                                     extension.dependentExtensions.length > 0;
        enable.querySelector('input').disabled = enableCheckboxDisabled;

        if (!enableCheckboxDisabled) {
          enable.addEventListener('click', function(e) {
            // When e.target is the label instead of the checkbox, it doesn't
            // have the checked property and the state of the checkbox is
            // left unchanged.
            var checked = e.target.checked;
            if (checked == undefined)
              checked = !e.currentTarget.querySelector('input').checked;
            chrome.send('extensionSettingsEnable',
                        [extension.id, checked ? 'true' : 'false']);

            // This may seem counter-intuitive (to not set/clear the checkmark)
            // but this page will be updated asynchronously if the extension
            // becomes enabled/disabled. It also might not become enabled or
            // disabled, because the user might e.g. get prompted when enabling
            // and choose not to.
            e.preventDefault();
          });
        }

        enable.querySelector('input').checked = extension.enabled;
      }

      // 'Remove' button.
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.title = loadTimeData.getString('extensionUninstall');
      trash.addEventListener('click', function(e) {
        butterBarVisibility[extension.id] = false;
        chrome.send('extensionSettingsUninstall', [extension.id]);
      });
      node.querySelector('.enable-controls').appendChild(trash);

      // Developer mode ////////////////////////////////////////////////////////

      // First we have the id.
      var idLabel = node.querySelector('.extension-id');
      idLabel.textContent = ' ' + extension.id;

      // Then the path, if provided by unpacked extension.
      if (extension.isUnpacked) {
        var loadPath = node.querySelector('.load-path');
        loadPath.hidden = false;
        var pathLink = loadPath.querySelector('a:nth-of-type(1)');
        pathLink.textContent = ' ' + extension.prettifiedPath;
        pathLink.addEventListener('click', function(e) {
          chrome.send('extensionSettingsShowPath', [String(extension.id)]);
          e.preventDefault();
        });
      }

      // Then the 'managed, cannot uninstall/disable' message.
      if (extension.managedInstall) {
        node.querySelector('.managed-message').hidden = false;
      } else {
        if (extension.suspiciousInstall) {
          // Then the 'This isn't from the webstore, looks suspicious' message.
          node.querySelector('.suspicious-install-message').hidden = false;
        }
        if (extension.corruptInstall) {
          // Then the 'This is a corrupt extension' message.
          node.querySelector('.corrupt-install-message').hidden = false;
        }
      }

      if (extension.dependentExtensions.length > 0) {
        var dependentMessage =
            node.querySelector('.dependent-extensions-message');
        dependentMessage.hidden = false;
        var dependentList = dependentMessage.querySelector('ul');
        var dependentTemplate = $('template-collection').querySelector(
            '.dependent-list-item');
        extension.dependentExtensions.forEach(function(elem) {
          var depNode = dependentTemplate.cloneNode(true);
          depNode.querySelector('.dep-extension-title').textContent = elem.name;
          depNode.querySelector('.dep-extension-id').textContent = elem.id;
          dependentList.appendChild(depNode);
        });
      }

      // Then active views.
      if (extension.views.length > 0) {
        var activeViews = node.querySelector('.active-views');
        activeViews.hidden = false;
        var link = activeViews.querySelector('a');

        extension.views.forEach(function(view, i) {
          var displayName = view.generatedBackgroundPage ?
              loadTimeData.getString('backgroundPage') : view.path;
          var label = displayName +
              (view.incognito ?
                  ' ' + loadTimeData.getString('viewIncognito') : '') +
              (view.renderProcessId == -1 ?
                  ' ' + loadTimeData.getString('viewInactive') : '');
          link.textContent = label;
          link.addEventListener('click', function(e) {
            // TODO(estade): remove conversion to string?
            chrome.send('extensionSettingsInspect', [
              String(extension.id),
              String(view.renderProcessId),
              String(view.renderViewId),
              view.incognito
            ]);
          });

          if (i < extension.views.length - 1) {
            link = link.cloneNode(true);
            activeViews.appendChild(link);
          }
        });
      }

      // The extension warnings (describing runtime issues).
      if (extension.warnings) {
        var panel = node.querySelector('.extension-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.warnings.forEach(function(warning) {
          list.appendChild(document.createElement('li')).innerText = warning;
        });
      }

      // If the ErrorConsole is enabled, we should have manifest and/or runtime
      // errors. Otherwise, we may have install warnings. We should not have
      // both ErrorConsole errors and install warnings.
      if (extension.manifestErrors) {
        var panel = node.querySelector('.manifest-errors');
        panel.hidden = false;
        panel.appendChild(new extensions.ExtensionErrorList(
            extension.manifestErrors));
      }
      if (extension.runtimeErrors) {
        var panel = node.querySelector('.runtime-errors');
        panel.hidden = false;
        panel.appendChild(new extensions.ExtensionErrorList(
            extension.runtimeErrors));
      }
      if (extension.installWarnings) {
        var panel = node.querySelector('.install-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.installWarnings.forEach(function(warning) {
          var li = document.createElement('li');
          li.innerText = warning.message;
          list.appendChild(li);
        });
      }

      this.appendChild(node);
      if (location.hash.substr(1) == extension.id) {
        // Scroll beneath the fixed header so that the extension is not
        // obscured.
        var topScroll = node.offsetTop - $('page-header').offsetHeight;
        var pad = parseInt(window.getComputedStyle(node, null).marginTop, 10);
        if (!isNaN(pad))
          topScroll -= pad / 2;
        setScrollTopForDocument(document, topScroll);
      }
    },

    /**
     * Opens the extension options overlay for the extension with the given id.
     * @param {string} extensionId The id of extension whose options page should
     *     be displayed.
     * @param {boolean} scroll Whether the page should scroll to the extension
     * @private
     */
    showEmbeddedExtensionOptions_: function(extensionId, scroll) {
      if (this.optionsShown_)
        return;

      // Get the extension from the given id.
      var extension = this.data_.extensions.filter(function(extension) {
        return extension.id == extensionId;
      })[0];

      if (!extension)
        return;

      if (scroll)
        this.scrollToNode_(extensionId);
      // Add the options query string. Corner case: the 'options' query string
      // will clobber the 'id' query string if the options link is clicked when
      // 'id' is in the URL, or if both query strings are in the URL.
      uber.replaceState({}, '?options=' + extensionId);

      extensions.ExtensionOptionsOverlay.getInstance().
          setExtensionAndShowOverlay(extensionId,
                                     extension.name,
                                     extension.icon);

      this.optionsShown_ = true;
      $('overlay').addEventListener('cancelOverlay', function() {
        this.optionsShown_ = false;
      }.bind(this));
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
