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
 *            enableExtensionInfoDialog: boolean,
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
 *            installedByCustodian: boolean,
 *            installWarnings: (Array|undefined),
 *            is_hosted_app: boolean,
 *            is_platform_app: boolean,
 *            isFromStore: boolean,
 *            isUnpacked: boolean,
 *            kioskEnabled: boolean,
 *            kioskOnly: boolean,
 *            locationText: string,
 *            managedInstall: boolean,
 *            manifestErrors: (Array<RuntimeError>|undefined),
 *            name: string,
 *            offlineEnabled: boolean,
 *            optionsOpenInTab: boolean,
 *            optionsPageHref: string,
 *            optionsUrl: string,
 *            order: number,
 *            packagedApp: boolean,
 *            path: (string|undefined),
 *            policyText: (string|undefined),
 *            prettifiedPath: (string|undefined),
 *            recommendedInstall: boolean,
 *            runtimeErrors: (Array<RuntimeError>|undefined),
 *            showAllUrls: boolean,
 *            suspiciousInstall: boolean,
 *            terminated: boolean,
 *            updateRequiredByPolicy: boolean,
 *            version: string,
 *            views: Array<{renderViewId: number, renderProcessId: number,
 *                path: string, incognito: boolean,
 *                generatedBackgroundPage: boolean}>,
 *            wantsErrorCollection: boolean,
 *            wantsFileAccess: boolean,
 *            warnings: (Array|undefined)}}
 */
var ExtensionData;

///////////////////////////////////////////////////////////////////////////////
// ExtensionFocusRow:

/**
 * Provides an implementation for a single column grid.
 * @constructor
 * @extends {cr.ui.FocusRow}
 */
function ExtensionFocusRow() {}

/**
 * Decorates |focusRow| so that it can be treated as a ExtensionFocusRow.
 * @param {Element} focusRow The element that has all the columns.
 * @param {Node} boundary Focus events are ignored outside of this node.
 */
ExtensionFocusRow.decorate = function(focusRow, boundary) {
  focusRow.__proto__ = ExtensionFocusRow.prototype;
  focusRow.decorate(boundary);
};

ExtensionFocusRow.prototype = {
  __proto__: cr.ui.FocusRow.prototype,

  /** @override */
  getEquivalentElement: function(element) {
    if (this.focusableElements.indexOf(element) > -1)
      return element;

    // All elements default to another element with the same type.
    var columnType = element.getAttribute('column-type');
    var equivalent = this.querySelector('[column-type=' + columnType + ']');

    if (!equivalent || !this.canAddElement_(equivalent)) {
      var actionLinks = ['options', 'website', 'launch', 'localReload'];
      var optionalControls = ['showButton', 'incognito', 'dev-collectErrors',
                              'allUrls', 'localUrls'];
      var removeStyleButtons = ['trash', 'enterprise'];
      var enableControls = ['terminatedReload', 'repair', 'enabled'];

      if (actionLinks.indexOf(columnType) > -1)
        equivalent = this.getFirstFocusableByType_(actionLinks);
      else if (optionalControls.indexOf(columnType) > -1)
        equivalent = this.getFirstFocusableByType_(optionalControls);
      else if (removeStyleButtons.indexOf(columnType) > -1)
        equivalent = this.getFirstFocusableByType_(removeStyleButtons);
      else if (enableControls.indexOf(columnType) > -1)
        equivalent = this.getFirstFocusableByType_(enableControls);
    }

    // Return the first focusable element if no equivalent type is found.
    return equivalent || this.focusableElements[0];
  },

  /** Updates the list of focusable elements. */
  updateFocusableElements: function() {
    this.focusableElements.length = 0;

    var focusableCandidates = this.querySelectorAll('[column-type]');
    for (var i = 0; i < focusableCandidates.length; ++i) {
      var element = focusableCandidates[i];
      if (this.canAddElement_(element))
        this.addFocusableElement(element);
    }
  },

  /**
   * Get the first focusable element that matches a list of types.
   * @param {Array<string>} types An array of types to match from.
   * @return {?Element} Return the first element that matches a type in |types|.
   * @private
   */
  getFirstFocusableByType_: function(types) {
    for (var i = 0; i < this.focusableElements.length; ++i) {
      var element = this.focusableElements[i];
      if (types.indexOf(element.getAttribute('column-type')) > -1)
        return element;
    }
    return null;
  },

  /**
   * Setup a typical column in the ExtensionFocusRow. A column can be any
   * element and should have an action when clicked/toggled. This function
   * adds a listener and a handler for an event. Also adds the "column-type"
   * attribute to make the element focusable in |updateFocusableElements|.
   * @param {string} query A query to select the element to set up.
   * @param {string} columnType A tag used to identify the column when
   *     changing focus.
   * @param {string} eventType The type of event to listen to.
   * @param {function(Event)} handler The function that should be called
   *     by the event.
   * @private
   */
  setupColumn: function(query, columnType, eventType, handler) {
    var element = this.querySelector(query);
    element.addEventListener(eventType, handler);
    element.setAttribute('column-type', columnType);
  },

  /**
   * @param {Element} element
   * @return {boolean}
   * @private
   */
  canAddElement_: function(element) {
    if (!element || element.disabled)
      return false;

    var developerMode = $('extension-settings').classList.contains('dev-mode');
    if (this.isDeveloperOption_(element) && !developerMode)
      return false;

    for (var el = element; el; el = el.parentElement) {
      if (el.hidden)
        return false;
    }

    return true;
  },

  /**
   * Returns true if the element should only be shown in developer mode.
   * @param {Element} element
   * @return {boolean}
   * @private
   */
  isDeveloperOption_: function(element) {
    return /^dev-/.test(element.getAttribute('column-type'));
  },
};

cr.define('extensions', function() {
  'use strict';

  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var ExtensionList = cr.ui.define('div');

  /**
   * @type {Object<string, number>} A map from extension id to last reloaded
   *     timestamp. The timestamp is recorded when the user click the 'Reload'
   *     link. It is used to refresh the icon of an unpacked extension.
   *     This persists between calls to decorate.
   */
  var extensionReloadedTimestamp = {};

  ExtensionList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Indicates whether an embedded options page that was navigated to through
     * the '?options=' URL query has been shown to the user. This is necessary
     * to prevent showExtensionNodes_ from opening the options more than once.
     * @type {boolean}
     * @private
     */
    optionsShown_: false,

    /** @private {!cr.ui.FocusGrid} */
    focusGrid_: new cr.ui.FocusGrid(),

    /**
     * Indicates whether an uninstall dialog is being shown to prevent multiple
     * dialogs from being displayed.
     * @type {boolean}
     * @private
     */
    uninstallIsShowing_: false,

    /**
     * Necessary to only show the butterbar once.
     * @private {boolean}
     */
    butterbarShown_: false,

    decorate: function() {
      this.showExtensionNodes_();
    },

    getIdQueryParam_: function() {
      return parseQueryParams(document.location)['id'];
    },

    getOptionsQueryParam_: function() {
      return parseQueryParams(document.location)['options'];
    },

    /**
     * Creates or updates all extension items from scratch.
     * @private
     */
    showExtensionNodes_: function() {
      // Remove the rows from |focusGrid_| without destroying them.
      this.focusGrid_.rows.length = 0;

      // Any node that is not updated will be removed.
      var seenIds = [];

      // Iterate over the extension data and add each item to the list.
      this.data_.extensions.forEach(function(extension, i) {
        var nextExt = this.data_.extensions[i + 1];
        var node = $(extension.id);
        seenIds.push(extension.id);

        if (node)
          this.updateNode_(extension, node);
        else
          this.createNode_(extension, nextExt ? $(nextExt.id) : null);
      }, this);

      // Remove extensions that are no longer installed.
      var nodes = document.querySelectorAll('.extension-list-item-wrapper[id]');
      for (var i = 0; i < nodes.length; ++i) {
        var node = nodes[i];
        if (seenIds.indexOf(node.id) < 0) {
          node.parentElement.removeChild(node);
          // Unregister the removed node from events.
          assertInstanceof(node, ExtensionFocusRow).destroy();
        }
      }

      var idToHighlight = this.getIdQueryParam_();
      if (idToHighlight && $(idToHighlight))
        this.scrollToNode_(idToHighlight);

      var idToOpenOptions = this.getOptionsQueryParam_();
      if (idToOpenOptions && $(idToOpenOptions))
        this.showEmbeddedExtensionOptions_(idToOpenOptions, true);

      var noExtensions = this.data_.extensions.length == 0;
      this.classList.toggle('empty-extension-list', noExtensions);
    },

    /** Updates each row's focusable elements without rebuilding the grid. */
    updateFocusableElements: function() {
      var rows = document.querySelectorAll('.extension-list-item-wrapper[id]');
      for (var i = 0; i < rows.length; ++i) {
        assertInstanceof(rows[i], ExtensionFocusRow).updateFocusableElements();
      }
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
     * @param {!ExtensionData} extension A dictionary of extension metadata.
     * @param {?Element} nextNode |node| should be inserted before |nextNode|.
     *     |node| will be appended to the end if |nextNode| is null.
     * @private
     */
    createNode_: function(extension, nextNode) {
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
      ExtensionFocusRow.decorate(node, $('extension-settings-list'));

      var row = assertInstanceof(node, ExtensionFocusRow);
      row.id = extension.id;

      // The 'Show Browser Action' button.
      row.setupColumn('.show-button', 'showButton', 'click', function(e) {
        chrome.send('extensionSettingsShowButton', [extension.id]);
      });

      // The 'allow in incognito' checkbox.
      row.setupColumn('.incognito-control input', 'incognito', 'change',
                      function(e) {
        var butterBar = row.querySelector('.butter-bar');
        var checked = e.target.checked;
        if (!this.butterbarShown_) {
          butterBar.hidden = !checked || extension.is_hosted_app;
          this.butterbarShown_ = !butterBar.hidden;
        } else {
          butterBar.hidden = true;
        }
        chrome.developerPrivate.allowIncognito(extension.id, checked);
      }.bind(this));

      // The 'collect errors' checkbox. This should only be visible if the
      // error console is enabled - we can detect this by the existence of the
      // |errorCollectionEnabled| property.
      row.setupColumn('.error-collection-control input', 'dev-collectErrors',
                      'change', function(e) {
        chrome.send('extensionSettingsEnableErrorCollection',
                    [extension.id, String(e.target.checked)]);
      });

      // The 'allow on all urls' checkbox. This should only be visible if
      // active script restrictions are enabled. If they are not enabled, no
      // extensions should want all urls.
      row.setupColumn('.all-urls-control input', 'allUrls', 'click',
                      function(e) {
        chrome.send('extensionSettingsAllowOnAllUrls',
                    [extension.id, String(e.target.checked)]);
      });

      // The 'allow file:// access' checkbox.
      row.setupColumn('.file-access-control input', 'localUrls', 'click',
                      function(e) {
        chrome.developerPrivate.allowFileAccess(extension.id, e.target.checked);
      });

      // The 'Options' button or link, depending on its behaviour.
      // Set an href to get the correct mouse-over appearance (link,
      // footer) - but the actual link opening is done through chrome.send
      // with a preventDefault().
      row.querySelector('.options-link').href = extension.optionsPageHref;
      row.setupColumn('.options-link', 'options', 'click', function(e) {
        chrome.send('extensionSettingsOptions', [extension.id]);
        e.preventDefault();
      });

      row.setupColumn('.options-button', 'options', 'click', function(e) {
        this.showEmbeddedExtensionOptions_(extension.id, false);
        e.preventDefault();
      }.bind(this));

      // The 'View in Web Store/View Web Site' link.
      row.querySelector('.site-link').setAttribute('column-type', 'website');

      // The 'Permissions' link.
      row.setupColumn('.permissions-link', 'details', 'click', function(e) {
        chrome.send('extensionSettingsPermissions', [extension.id]);
        e.preventDefault();
      });

      // The 'Reload' link.
      row.setupColumn('.reload-link', 'localReload', 'click', function(e) {
        chrome.developerPrivate.reload(extension.id, {failQuietly: true});
        extensionReloadedTimestamp[extension.id] = Date.now();
      });

      // The 'Launch' link.
      row.setupColumn('.launch-link', 'launch', 'click', function(e) {
        chrome.send('extensionSettingsLaunch', [extension.id]);
      });

      // The 'Reload' terminated link.
      row.setupColumn('.terminated-reload-link', 'terminatedReload', 'click',
                      function(e) {
        chrome.send('extensionSettingsReload', [extension.id]);
      });

      // The 'Repair' corrupted link.
      row.setupColumn('.corrupted-repair-button', 'repair', 'click',
                      function(e) {
        chrome.send('extensionSettingsRepair', [extension.id]);
      });

      // The 'Enabled' checkbox.
      row.setupColumn('.enable-checkbox input', 'enabled', 'change',
                      function(e) {
        var checked = e.target.checked;
        // TODO(devlin): What should we do if this fails?
        chrome.management.setEnabled(extension.id, checked);

        // This may seem counter-intuitive (to not set/clear the checkmark)
        // but this page will be updated asynchronously if the extension
        // becomes enabled/disabled. It also might not become enabled or
        // disabled, because the user might e.g. get prompted when enabling
        // and choose not to.
        e.preventDefault();
      });

      // 'Remove' button.
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.title = loadTimeData.getString('extensionUninstall');
      trash.hidden = extension.managedInstall;
      trash.setAttribute('column-type', 'trash');
      trash.addEventListener('click', function(e) {
        trash.classList.add('open');
        trash.classList.toggle('mouse-clicked', e.detail > 0);
        if (this.uninstallIsShowing_)
          return;
        this.uninstallIsShowing_ = true;
        chrome.management.uninstall(extension.id,
                                    {showConfirmDialog: true},
                                    function() {
          // TODO(devlin): What should we do if the uninstall fails?
          this.uninstallIsShowing_ = false;

          if (trash.classList.contains('mouse-clicked'))
            trash.blur();

          if (chrome.runtime.lastError) {
            // The uninstall failed (e.g. a cancel). Allow the trash to close.
            trash.classList.remove('open');
          } else {
            // Leave the trash open if the uninstall succeded. Otherwise it can
            // half-close right before it's removed when the DOM is modified.
          }
        }.bind(this));
      }.bind(this));
      row.querySelector('.enable-controls').appendChild(trash);

      // Developer mode ////////////////////////////////////////////////////////

      // The path, if provided by unpacked extension.
      row.setupColumn('.load-path a:first-of-type', 'dev-loadPath', 'click',
                      function(e) {
        chrome.send('extensionSettingsShowPath', [String(extension.id)]);
        e.preventDefault();
      });

      // Maintain the order that nodes should be in when creating as well as
      // when adding only one new row.
      this.insertBefore(row, nextNode);
      this.updateNode_(extension, row);
    },

    /**
     * Updates an HTML element for the extension metadata given in |extension|.
     * @param {!ExtensionData} extension A dictionary of extension metadata.
     * @param {!ExtensionFocusRow} row The node that is being updated.
     * @private
     */
    updateNode_: function(extension, row) {
      var isActive = extension.enabled && !extension.terminated;

      row.classList.toggle('inactive-extension', !isActive);

      // Hack to keep the closure compiler happy about |remove|.
      // TODO(hcarmona): Remove this hack when the closure compiler is updated.
      var node = /** @type {Element} */ (row);
      node.classList.remove('policy-controlled', 'may-not-modify',
                            'may-not-remove');
      var classes = [];
      if (extension.managedInstall) {
        classes.push('policy-controlled', 'may-not-modify');
      } else if (extension.dependentExtensions.length > 0) {
        classes.push('may-not-remove', 'may-not-modify');
      } else if (extension.recommendedInstall) {
        classes.push('may-not-remove');
      } else if (extension.suspiciousInstall ||
                 extension.corruptInstall ||
                 extension.updateRequiredByPolicy) {
        classes.push('may-not-modify');
      }
      row.classList.add.apply(row.classList, classes);

      row.classList.toggle('extension-highlight',
                           row.id == this.getIdQueryParam_());

      var item = row.querySelector('.extension-list-item');
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

      this.setText_(row, '.extension-title', extension.name);
      this.setText_(row, '.extension-version', extension.version);
      this.setText_(row, '.location-text', extension.locationText);
      this.setText_(row, '.blacklist-text', extension.blacklistText);
      this.setText_(row, '.extension-description', extension.description);

      // The 'Show Browser Action' button.
      this.updateVisibility_(row, '.show-button',
                             isActive && extension.enable_show_button);

      // The 'allow in incognito' checkbox.
      this.updateVisibility_(row, '.incognito-control',
                             isActive && this.data_.incognitoAvailable,
                             function(item) {
        var incognito = item.querySelector('input');
        incognito.disabled = !extension.incognitoCanBeEnabled;
        incognito.checked = extension.enabledIncognito;
      });

      // Hide butterBar if incognito is not enabled for the extension.
      var butterBar = row.querySelector('.butter-bar');
      butterBar.hidden = butterBar.hidden || !extension.enabledIncognito;

      // The 'collect errors' checkbox. This should only be visible if the
      // error console is enabled - we can detect this by the existence of the
      // |errorCollectionEnabled| property.
      this.updateVisibility_(row, '.error-collection-control',
                             isActive && extension.wantsErrorCollection,
                             function(item) {
        item.querySelector('input').checked = extension.errorCollectionEnabled;
      });

      // The 'allow on all urls' checkbox. This should only be visible if
      // active script restrictions are enabled. If they are not enabled, no
      // extensions should want all urls.
      this.updateVisibility_(row, '.all-urls-control',
                             isActive && extension.showAllUrls, function(item) {
        item.querySelector('input').checked = extension.allowAllUrls;
      });

      // The 'allow file:// access' checkbox.
      this.updateVisibility_(row, '.file-access-control',
                             isActive && extension.wantsFileAccess,
                             function(item) {
        item.querySelector('input').checked = extension.allowFileAccess;
      });

      // The 'Options' button or link, depending on its behaviour.
      var optionsEnabled = extension.enabled && !!extension.optionsUrl;
      this.updateVisibility_(row, '.options-link', optionsEnabled &&
                             extension.optionsOpenInTab);
      this.updateVisibility_(row, '.options-button', optionsEnabled &&
                             !extension.optionsOpenInTab);

      // The 'View in Web Store/View Web Site' link.
      var siteLinkEnabled = !!extension.homepageUrl &&
                            !extension.enableExtensionInfoDialog;
      this.updateVisibility_(row, '.site-link', siteLinkEnabled,
                             function(item) {
        item.href = extension.homepageUrl;
        item.textContent = loadTimeData.getString(
            extension.homepageProvided ? 'extensionSettingsVisitWebsite' :
                                         'extensionSettingsVisitWebStore');
      });

      // The 'Reload' link.
      this.updateVisibility_(row, '.reload-link', extension.allow_reload);

      // The 'Launch' link.
      this.updateVisibility_(row, '.launch-link', extension.allow_reload &&
                             extension.is_platform_app);

      // The 'Reload' terminated link.
      var isTerminated = extension.terminated;
      this.updateVisibility_(row, '.terminated-reload-link', isTerminated);

      // The 'Repair' corrupted link.
      var canRepair = !isTerminated && extension.corruptInstall &&
                      extension.isFromStore;
      this.updateVisibility_(row, '.corrupted-repair-button', canRepair);

      // The 'Enabled' checkbox.
      var isOK = !isTerminated && !canRepair;
      this.updateVisibility_(row, '.enable-checkbox', isOK, function(item) {
        var enableCheckboxDisabled = extension.managedInstall ||
                                     extension.suspiciousInstall ||
                                     extension.corruptInstall ||
                                     extension.updateRequiredByPolicy ||
                                     extension.installedByCustodian ||
                                     extension.dependentExtensions.length > 0;
        item.querySelector('input').disabled = enableCheckboxDisabled;
        item.querySelector('input').checked = extension.enabled;
      });

      // Button for extensions controlled by policy.
      var controlNode = row.querySelector('.enable-controls');
      var indicator =
          controlNode.querySelector('.controlled-extension-indicator');
      var needsIndicator = isOK && extension.managedInstall;

      if (needsIndicator && !indicator) {
        indicator = new cr.ui.ControlledIndicator();
        indicator.classList.add('controlled-extension-indicator');
        indicator.setAttribute('controlled-by', 'policy');
        var textPolicy = extension.policyText || '';
        indicator.setAttribute('textpolicy', textPolicy);
        indicator.image.setAttribute('aria-label', textPolicy);
        controlNode.appendChild(indicator);
        indicator.querySelector('div').setAttribute('column-type',
                                                    'enterprise');
      } else if (!needsIndicator && indicator) {
        controlNode.removeChild(indicator);
      }

      // Developer mode ////////////////////////////////////////////////////////

      // First we have the id.
      var idLabel = row.querySelector('.extension-id');
      idLabel.textContent = ' ' + extension.id;

      // Then the path, if provided by unpacked extension.
      this.updateVisibility_(row, '.load-path', extension.isUnpacked,
                             function(item) {
        item.querySelector('a:first-of-type').textContent =
            ' ' + extension.prettifiedPath;
      });

      // Then the 'managed, cannot uninstall/disable' message.
      // We would like to hide managed installed message since this
      // extension is disabled.
      var isRequired = extension.managedInstall || extension.recommendedInstall;
      this.updateVisibility_(row, '.managed-message', isRequired &&
                             !extension.updateRequiredByPolicy);

      // Then the 'This isn't from the webstore, looks suspicious' message.
      this.updateVisibility_(row, '.suspicious-install-message', !isRequired &&
                             extension.suspiciousInstall);

      // Then the 'This is a corrupt extension' message.
      this.updateVisibility_(row, '.corrupt-install-message', !isRequired &&
                             extension.corruptInstall);

      // Then the 'An update required by enterprise policy' message. Note that
      // a force-installed extension might be disabled due to being outdated
      // as well.
      this.updateVisibility_(row, '.update-required-message',
                             extension.updateRequiredByPolicy);

      // The 'following extensions depend on this extension' list.
      var hasDependents = extension.dependentExtensions.length > 0;
      row.classList.toggle('developer-extras', hasDependents);
      this.updateVisibility_(row, '.dependent-extensions-message',
                             hasDependents, function(item) {
        var dependentList = item.querySelector('ul');
        dependentList.textContent = '';
        var dependentTemplate = $('template-collection').querySelector(
            '.dependent-list-item');
        extension.dependentExtensions.forEach(function(elem) {
          var depNode = dependentTemplate.cloneNode(true);
          depNode.querySelector('.dep-extension-title').textContent = elem.name;
          depNode.querySelector('.dep-extension-id').textContent = elem.id;
          dependentList.appendChild(depNode);
        });
      });

      // The active views.
      this.updateVisibility_(row, '.active-views', extension.views.length > 0,
                             function(item) {
        var link = item.querySelector('a');

        // Link needs to be an only child before the list is updated.
        while (link.nextElementSibling)
          item.removeChild(link.nextElementSibling);

        // Link needs to be cleaned up if it was used before.
        link.textContent = '';
        if (link.clickHandler)
          link.removeEventListener('click', link.clickHandler);

        extension.views.forEach(function(view, i) {
          var displayName = view.generatedBackgroundPage ?
              loadTimeData.getString('backgroundPage') : view.path;
          var label = displayName +
              (view.incognito ?
                  ' ' + loadTimeData.getString('viewIncognito') : '') +
              (view.renderProcessId == -1 ?
                  ' ' + loadTimeData.getString('viewInactive') : '');
          link.textContent = label;
          link.clickHandler = function(e) {
            // TODO(estade): remove conversion to string?
            chrome.send('extensionSettingsInspect', [
              String(extension.id),
              String(view.renderProcessId),
              String(view.renderViewId),
              view.incognito
            ]);
          };
          link.addEventListener('click', link.clickHandler);

          if (i < extension.views.length - 1) {
            link = link.cloneNode(true);
            item.appendChild(link);
          }
        });

        var allLinks = item.querySelectorAll('a');
        for (var i = 0; i < allLinks.length; ++i) {
          allLinks[i].setAttribute('column-type', 'dev-activeViews' + i);
        }
      });

      // The extension warnings (describing runtime issues).
      this.updateVisibility_(row, '.extension-warnings', !!extension.warnings,
                             function(item) {
        var warningList = item.querySelector('ul');
        warningList.textContent = '';
        extension.warnings.forEach(function(warning) {
          var li = document.createElement('li');
          warningList.appendChild(li).innerText = warning;
        });
      });

      // If the ErrorConsole is enabled, we should have manifest and/or runtime
      // errors. Otherwise, we may have install warnings. We should not have
      // both ErrorConsole errors and install warnings.
      // Errors.
      this.updateErrors_(row.querySelector('.manifest-errors'),
                         'dev-manifestErrors', extension.manifestErrors);
      this.updateErrors_(row.querySelector('.runtime-errors'),
                         'dev-runtimeErrors', extension.runtimeErrors);

      // Install warnings.
      this.updateVisibility_(row, '.install-warnings',
                             !!extension.installWarnings, function(item) {
        var installWarningList = item.querySelector('ul');
        installWarningList.textContent = '';
        if (extension.installWarnings) {
          extension.installWarnings.forEach(function(warning) {
            var li = document.createElement('li');
            li.innerText = warning.message;
            installWarningList.appendChild(li);
          });
        }
      });

      if (location.hash.substr(1) == extension.id) {
        // Scroll beneath the fixed header so that the extension is not
        // obscured.
        var topScroll = row.offsetTop - $('page-header').offsetHeight;
        var pad = parseInt(window.getComputedStyle(row, null).marginTop, 10);
        if (!isNaN(pad))
          topScroll -= pad / 2;
        setScrollTopForDocument(document, topScroll);
      }

      row.updateFocusableElements();
      this.focusGrid_.addRow(row);
    },

    /**
     * Updates an element's textContent.
     * @param {Element} node Ancestor of the element specified by |query|.
     * @param {string} query A query to select an element in |node|.
     * @param {string} textContent
     * @private
     */
    setText_: function(node, query, textContent) {
      node.querySelector(query).textContent = textContent;
    },

    /**
     * Updates an element's visibility and calls |shownCallback| if it is
     * visible.
     * @param {Element} node Ancestor of the element specified by |query|.
     * @param {string} query A query to select an element in |node|.
     * @param {boolean} visible Whether the element should be visible or not.
     * @param {function(Element)=} opt_shownCallback Callback if the element is
     *     visible. The element passed in will be the element specified by
     *     |query|.
     * @private
     */
    updateVisibility_: function(node, query, visible, opt_shownCallback) {
      var item = assert(node.querySelector(query));
      item.hidden = !visible;
      if (visible && opt_shownCallback)
        opt_shownCallback(item);
    },

    /**
     * Updates an element to show a list of errors.
     * @param {Element} panel An element to hold the errors.
     * @param {string} columnType A tag used to identify the column when
     *     changing focus.
     * @param {Array<RuntimeError>|undefined} errors The errors to be displayed.
     * @private
     */
    updateErrors_: function(panel, columnType, errors) {
      // TODO(hcarmona): Look into updating the ExtensionErrorList rather than
      // rebuilding it every time.
      panel.hidden = !errors || errors.length == 0;
      panel.textContent = '';

      if (panel.hidden)
        return;

      var errorList =
          new extensions.ExtensionErrorList(assertInstanceof(errors, Array));

      panel.appendChild(errorList);

      var list = errorList.getErrorListElement();
      if (list)
        list.setAttribute('column-type', columnType + 'list');

      var button = errorList.getToggleElement();
      if (button)
        button.setAttribute('column-type', columnType + 'button');
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
        return extension.enabled && extension.id == extensionId;
      })[0];

      if (!extension)
        return;

      if (scroll)
        this.scrollToNode_(extensionId);

      // Add the options query string. Corner case: the 'options' query string
      // will clobber the 'id' query string if the options link is clicked when
      // 'id' is in the URL, or if both query strings are in the URL.
      uber.replaceState({}, '?options=' + extensionId);

      var overlay = extensions.ExtensionOptionsOverlay.getInstance();
      var shownCallback = function() {
        // This overlay doesn't get focused automatically as <extensionoptions>
        // is created after the overlay is shown.
        if (cr.ui.FocusOutlineManager.forDocument(document).visible)
          overlay.setInitialFocus();
      };
      overlay.setExtensionAndShowOverlay(extensionId, extension.name,
                                         extension.icon, shownCallback);
      this.optionsShown_ = true;

      var self = this;
      $('overlay').addEventListener('cancelOverlay', function f() {
        self.optionsShown_ = false;
        $('overlay').removeEventListener('cancelOverlay', f);
      });

      // TODO(dbeam): why do we need to focus <extensionoptions> before and
      // after its showing animation? Makes very little sense to me.
      overlay.setInitialFocus();
    },
  };

  return {
    ExtensionList: ExtensionList
  };
});
