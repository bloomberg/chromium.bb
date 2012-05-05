// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js"></include>
<include src="extension_list.js"></include>
<include src="pack_extension_overlay.js"></include>

// Used for observing function of the backend datasource for this page by
// tests.
var webui_responded_ = false;

cr.define('extensions', function() {
  var ExtensionsList = options.ExtensionsList;

  /**
   * ExtensionSettings class
   * @class
   */
  function ExtensionSettings() {}

  cr.addSingletonGetter(ExtensionSettings);

  ExtensionSettings.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      uber.onContentFrameLoaded();

      measureCheckboxStrings();

      // Set the title.
      var title = loadTimeData.getString('extensionSettings');
      uber.invokeMethodOnParent('setTitle', {title: title});

      // This will request the data to show on the page and will get a response
      // back in returnExtensionsData.
      chrome.send('extensionSettingsRequestExtensionsData');

      $('toggle-dev-on').addEventListener('change',
          this.handleToggleDevMode_.bind(this));
      $('dev-controls').addEventListener('webkitTransitionEnd',
          this.handleDevControlsTransitionEnd_.bind(this));

      // Set up the three dev mode buttons (load unpacked, pack and update).
      $('load-unpacked').addEventListener('click',
          this.handleLoadUnpackedExtension_.bind(this));
      $('pack-extension').addEventListener('click',
          this.handlePackExtension_.bind(this));
      $('update-extensions-now').addEventListener('click',
          this.handleUpdateExtensionNow_.bind(this));

      var packExtensionOverlay = extensions.PackExtensionOverlay.getInstance();
      packExtensionOverlay.initializePage();
    },

    /**
     * Handles the Load Unpacked Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handleLoadUnpackedExtension_: function(e) {
      chrome.send('extensionSettingsLoadUnpackedExtension');

      // TODO(jhawkins): Refactor metrics support out of options and use it
      // in extensions.html.
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_LoadUnpackedExtension']);
    },

    /**
     * Handles the Pack Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handlePackExtension_: function(e) {
      ExtensionSettings.showOverlay($('packExtensionOverlay'));
      chrome.send('coreOptionsUserMetricsAction', ['Options_PackExtension']);
    },

    /**
     * Handles the Update Extension Now button.
     * @param {Event} e Change event.
     * @private
     */
    handleUpdateExtensionNow_: function(e) {
      chrome.send('extensionSettingsAutoupdate');
    },

    /**
     * Handles the Toggle Dev Mode button.
     * @param {Event} e Change event.
     * @private
     */
    handleToggleDevMode_: function(e) {
      if ($('toggle-dev-on').checked) {
        $('dev-controls').hidden = false;
        window.setTimeout(function() {
          $('extension-settings').classList.add('dev-mode');
        }, 0);
      } else {
        $('extension-settings').classList.remove('dev-mode');
      }

      chrome.send('extensionSettingsToggleDeveloperMode');
    },

    /**
     * Called when a transition has ended for #dev-controls.
     * @param {Event} e webkitTransitionEnd event.
     * @private
     */
    handleDevControlsTransitionEnd_: function(e) {
      if (e.propertyName == 'height' &&
          !$('extension-settings').classList.contains('dev-mode')) {
        $('dev-controls').hidden = true;
      }
    },
  };

  /**
   * Called by the dom_ui_ to re-populate the page with data representing
   * the current state of installed extensions.
   */
  ExtensionSettings.returnExtensionsData = function(extensionsData) {
    // We can get called many times in short order, thus we need to
    // be careful to remove the 'finished loading' timeout.
    if (this.loadingTimeout_)
      window.clearTimeout(this.loadingTimeout_);
    document.documentElement.classList.add('loading');
    this.loadingTimeout_ = window.setTimeout(function() {
      document.documentElement.classList.remove('loading');
    }, 0);

    webui_responded_ = true;

    if (extensionsData.extensions.length > 0) {
      // Enforce order specified in the data or (if equal) then sort by
      // extension name (case-insensitive).
      extensionsData.extensions.sort(function(a, b) {
        if (a.order == b.order) {
          a = a.name.toLowerCase();
          b = b.name.toLowerCase();
          return a < b ? -1 : (a > b ? 1 : 0);
        } else {
          return a.order < b.order ? -1 : 1;
        }
      });
    }

    if (extensionsData.developerMode) {
      $('toggle-dev-on').checked = true;
      $('extension-settings').classList.add('dev-mode');
      $('dev-controls').hidden = false;
    } else {
      $('toggle-dev-on').checked = false;
      $('extension-settings').classList.remove('dev-mode');
    }

    $('load-unpacked').disabled = extensionsData.loadUnpackedDisabled;

    ExtensionsList.prototype.data_ = extensionsData;
    var extensionList = $('extension-settings-list');
    ExtensionsList.decorate(extensionList);
  }

  // Indicate that warning |message| has occured for pack of |crx_path| and
  // |pem_path| files.  Ask if user wants override the warning.  Send
  // |overrideFlags| to repeated 'pack' call to accomplish the override.
  ExtensionSettings.askToOverrideWarning =
      function(message, crx_path, pem_path, overrideFlags) {
    var closeAlert = function() {
      ExtensionSettings.showOverlay(null);
    };

    alertOverlay.setValues(
        loadTimeData.getString('packExtensionWarningTitle'),
        message,
        loadTimeData.getString('packExtensionProceedAnyway'),
        loadTimeData.getString('cancel'),
        function() {
          chrome.send('pack', [crx_path, pem_path, overrideFlags]);
          closeAlert();
        },
        closeAlert);
    ExtensionSettings.showOverlay($('alertOverlay'));
  }

  /**
   * Sets the given overlay to show. This hides whatever overlay is currently
   * showing, if any.
   * @param {HTMLElement} node The overlay page to show. If falsey, all overlays
   *     are hidden.
   */
  ExtensionSettings.showOverlay = function(node) {
    var currentlyShowingOverlay =
        document.querySelector('#overlay .page.showing');
    if (currentlyShowingOverlay)
      currentlyShowingOverlay.classList.remove('showing');

    if (node)
      node.classList.add('showing');
    overlay.hidden = !node;
    uber.invokeMethodOnParent(node ? 'beginInterceptingEvents' :
                                     'stopInterceptingEvents');
  }

  /**
   * Utility function to find the width of various UI strings and synchronize
   * the width of relevant spans. This is crucial for making sure the
   * Enable/Enabled checkboxes align, as well as the Developer Mode checkbox.
   */
  function measureCheckboxStrings() {
    var trashWidth = 30;
    var measuringDiv = $('font-measuring-div');
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnabled');
    var pxWidth = measuringDiv.clientWidth + trashWidth;
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnable');
    pxWidth = Math.max(measuringDiv.clientWidth + trashWidth, pxWidth);
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsDeveloperMode');
    pxWidth = Math.max(measuringDiv.clientWidth, pxWidth);

    var style = document.createElement('style');
    style.type = 'text/css';
    style.textContent =
        '.enable-checkbox-text {' +
        '  min-width: ' + (pxWidth - trashWidth) + 'px;' +
        '}' +
        '#dev-toggle span {' +
        '  min-width: ' + pxWidth + 'px;' +
        '}';
    document.querySelector('head').appendChild(style);
  }

  // Export
  return {
    ExtensionSettings: ExtensionSettings
  };
});

var ExtensionSettings = extensions.ExtensionSettings;

window.addEventListener('load', function(e) {
  ExtensionSettings.getInstance().initialize();
});
