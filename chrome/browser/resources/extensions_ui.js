// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */
var extensionDataFormat = {
  'developerMode': false,
  'extensions': [
    {
      'id': '0000000000000000000000000000000000000000',
      'name': 'Google Chrome',
      'description': 'Extension long format description',
      'version': '1.0.231',
      'mayDisable': 'true',
      'enabled': 'true',
      'terminated': 'false',
      'enabledIncognito': 'false',
      'wantsFileAccess': 'false',
      'allowFileAccess': 'false',
      'allow_reload': true,
      'is_hosted_app': false,
      'order': 1,
      'options_url': 'options.html',
      'enable_show_button': false,
      'icon': 'relative-path-to-icon.png',
      // TODO(aa): It would be nice to also render what type of view each one
      // is, like 'toolstrip', 'background', etc. Eventually, if we can also
      // debug and inspect content scripts, maybe we don't need to list the
      // components, just the views.
      'views': [
        {
          'path': 'toolstrip.html',
          'renderViewId': 1,
          'renderProcessId': 1,
          'incognito': false
        },
        {
          'path': 'background.html',
          'renderViewId': 2,
          'renderProcessId': 1,
          'incognito': false
        }
      ]
    },
    {
      'id': '0000000000000000000000000000000000000001',
      'name': 'RSS Subscriber',
      'description': 'Extension long format description',
      'version': '1.0.231',
      'mayDisable': 'true',
      'enabled': 'true',
      'terminated': 'false',
      'enabledIncognito': 'false',
      'wantsFileAccess': 'false',
      'allowFileAccess': 'false',
      'allow_reload': false,
      'is_hosted_app': false,
      'order': 2,
      'icon': '',
      "hasPopupAction": false
    }
  ]
};

// Keeps track of whether the developer mode subsection has been made visible
// (expanded) or not.
var devModeExpanded = false;

/**
 * Toggles the devModeExpanded, and notifies the c++ WebUI to toggle the
 * extensions.ui.developer_mode which saved in the preferences.
 */
function toggleDevModeExpanded() {
  devModeExpanded = !devModeExpanded;
  chrome.send('toggleDeveloperMode', []);
}

/**
 * Takes the |extensionsData| input argument which represents data about the
 * currently installed/running extensions and populates the html jstemplate with
 * that data. It expects an object structure like the above.
 * @param {Object} extensionsData Detailed info about installed extensions
 */
function renderTemplate(extensionsData) {
  // Sort by order specified in the data or (if equal) then sort by
  // extension name.
  var locale = new v8Locale(window.navigator.language);
  var coll = locale.createCollator();
  extensionsData.extensions.sort(function(a, b) {
    if (a.order == b.order) {
      return coll.compare(a.name, b.name);
    }
    return a.order < b.order ? -1 : 1;
  });

  // This is the javascript code that processes the template:
  var input = new JsEvalContext(extensionsData);
  var output = $('extensionTemplate');
  jstProcess(input, output);

  // Hook up the handlers now that the template is processed.
  $('load-extension').addEventListener('click', loadExtension);
  $('show-pack-dialog').addEventListener('click', showPackDialog);
  $('auto-update').addEventListener('click', autoUpdate);

  addHandlerByClass('developer-mode-image', 'click', toggleDeveloperMode);
  addHandlerByClass('developer-mode-link', 'click', toggleDeveloperMode);
  addHandlerByClass('extension-path', 'click', selectExtensionPath);
  addHandlerByClass('private-key-path', 'click', selectPrivateKeyPath);
  addHandlerByClass('pack-extension', 'click', packExtension);
  addHandlerByClass('hide-pack-dialog', 'click', hidePackDialog);
  addHandlerByClass('options-url', 'click', handleOptions);
  addHandlerByClass('reload-extension', 'click', handleReloadExtension);
  addHandlerByClass('disable-extension', 'click', handleDisableExtension);
  addHandlerByClass('enable-extension', 'click', handleEnableExtension);
  addHandlerByClass('uninstall-extension', 'click', handleUninstallExtension);
  addHandlerByClass('show-button', 'click', handleShowButton);
  addHandlerByClass('inspect-message', 'click', handleInspectMessage);
  addHandlerByClass('toggle-incognito', 'change',
                    handleToggleExtensionIncognito);
  addHandlerByClass('file-access', 'change',
                    handleToggleAllowFileAccess);
}

/**
 * Asks the C++ ExtensionDOMHandler to get details about the installed
 * extensions and return detailed data about the configuration. The
 * ExtensionDOMHandler should reply to returnExtensionsData() (below).
 */
function requestExtensionsData() {
  chrome.send('requestExtensionsData', []);
}

// Used for observing function of the backend datasource for this page by
// tests.
var webui_responded_ = false;

// Used to only do some work the first time we render.
var rendered_once_ = false;

/**
 * Called by the web_ui_ to re-populate the page with data representing
 * the current state of installed extensions.
 */
function returnExtensionsData(extensionsData){
  webui_responded_ = true;
  devModeExpanded = extensionsData.developerMode;

  var bodyContainer = $('body-container');
  var body = document.body;

  // Set all page content to be visible so we can measure heights.
  bodyContainer.style.visibility = 'hidden';
  body.className = '';
  var slidables = document.getElementsByClassName('showInDevMode');
  for (var i = 0; i < slidables.length; i++)
    slidables[i].style.height = 'auto';

  renderTemplate(extensionsData);

  // Explicitly set the height for each element that wants to be 'slid' in and
  // out when the devModeExpanded is toggled.
  var slidables = document.getElementsByClassName('showInDevMode');
  for (var i = 0; i < slidables.length; i++)
    slidables[i].style.height = slidables[i].offsetHeight + 'px';

  // Hide all the incognito warnings that are attached to the wrong extension
  // ID, which can happen when an extension is added or removed.
  var warnings = document.getElementsByClassName('incognitoWarning');
  for (var i = 0; i < warnings.length; i++) {
    var extension = warnings[i];
    while (extension.className != "extension")
      extension = extension.parentNode;

    if (extension.extensionId != warnings[i].attachedExtensionId) {
      warnings[i].style.display = "none";
      warnings[i].style.opacity = "0";
    }
  }

  // Reset visibility of page based on the current dev mode.
  $('collapse').style.display = devModeExpanded ? 'inline' : 'none';
  $('expand').style.display = devModeExpanded ? 'none' : 'inline';
  bodyContainer.style.visibility = 'visible';
  body.className = devModeExpanded ?
     'showDevModeInitial' : 'hideDevModeInitial';

  if (rendered_once_)
    return;

  // Blech, JSTemplate always inserts the strings as text, which is usually a
  // feature, but in this case it contains HTML that we want to be rendered.
  var elm = $('try-gallery');
  if (elm)
    elm.innerHTML = elm.textContent;

  elm = $('get-moar-extensions');
  if (elm)
    elm.innerHTML = elm.textContent;

  rendered_once_ = true;
}

/**
 * Tell the C++ ExtensionDOMHandler to inspect the page detailed in |viewData|.
 */
function handleInspectMessage(event) {
  // TODO(aa): This is ghetto, but WebUIBindings doesn't support sending
  // anything other than arrays of strings, and this is all going to get
  // replaced with V8 extensions soon anyway.
  chrome.send('inspect', [
    String(this.extensionView.renderProcessId),
    String(this.extensionView.renderViewId)
  ]);
  event.preventDefault();
}

/**
 * Handles a 'reload' link getting clicked.
 */
function handleReloadExtension(event) {
  // Tell the C++ ExtensionDOMHandler to reload the extension.
  chrome.send('reload', [this.extensionId]);
  event.preventDefault();
}

/**
 * Handles a 'disable' link getting clicked.
 */
function handleDisableExtension(event) {
  sendEnableExtension(this, false);
  event.preventDefault();
}

/**
 * Handles an 'enable' link getting clicked.
 */
function handleEnableExtension(event) {
  sendEnableExtension(this, true);
  event.preventDefault();
}

/**
 * Peforms the actual work to enable or disable an extension.
 */
function sendEnableExtension(node, enable) {
  // Tell the C++ ExtensionDOMHandler to reload the extension.
  chrome.send('enable', [node.extensionId, String(enable)]);
  requestExtensionsData();
}

/**
 * Handles the 'enableIncognito' checkbox getting changed.
 */
function handleToggleExtensionIncognito(event) {
  var warning = this;

  while (warning.className != "extension")
    warning = warning.parentNode;

  warning = warning.getElementsByClassName("incognitoWarning")[0];
  if (!this.checked) {
    warning.style.display = "none";
    warning.style.opacity = "0";
  } else {
    warning.attachedExtensionId = this.extensionId;
    warning.style.display = "block";

    // Must set the opacity later. Otherwise, the fact that the display is
    // changing causes the animation to not happen.
    window.setTimeout(function() {
      warning.style.opacity = "1";
    }, 0);
  }

  chrome.send('enableIncognito', [this.extensionId, String(this.checked)]);
  event.preventDefault();
}

/**
 * Handles the 'allowFileAccess' checkbox getting changed.
 */
function handleToggleAllowFileAccess(event) {
  chrome.send('allowFileAccess', [this.extensionId, String(this.checked)]);
  event.preventDefault();
}

/**
 * Handles an 'uninstall' link getting clicked.
 */
function handleUninstallExtension(event) {
  chrome.send('uninstall', [this.extensionId]);
  event.preventDefault();
}

/**
 * Handles an 'options' link getting clicked.
 */
function handleOptions(event) {
  chrome.send('options', [this.extensionId]);
  event.preventDefault();
}

/**
* Handles a 'show button' link getting clicked.
*/
function handleShowButton(event) {
  chrome.send('showButton', [this.extensionId]);
  event.preventDefault();
}

/**
* Utility function which asks the C++ to show a platform-specific file select
* dialog, and fire |callback| with the |filePath| that resulted. |selectType|
* can be either 'file' or 'folder'. |operation| can be 'load', 'packRoot',
* or 'pem' which are signals to the C++ to do some operation-specific
* configuration.
*/
function showFileDialog(selectType, operation, callback) {
  handleFilePathSelected = function(filePath) {
    callback(filePath);
    handleFilePathSelected = function() {};
  };

  chrome.send('selectFilePath', [selectType, operation]);
}

/**
 * Handles the "Load extension..." button being pressed.
 */
function loadExtension() {
  showFileDialog('folder', 'load', function(filePath) {
    chrome.send('load', [String(filePath)]);
  });
}

/**
 * Handles the "Pack extension..." button being pressed.
 */
function packExtension() {
  var extensionPath = $('extensionPathText').value;
  var privateKeyPath = $('privateKeyPath').value;
  chrome.send('pack', [extensionPath, privateKeyPath]);
}

/**
 * Shows to modal HTML pack dialog.
 */
function showPackDialog() {
  $('dialogBackground').style.display = '-webkit-box';
}

/**
 * Hides the pack dialog.
 */
function hidePackDialog() {
  $('dialogBackground').style.display = 'none'
}

/*
 * Toggles visibility of the developer mode.
 */
function toggleDeveloperMode() {
  toggleDevModeExpanded();

  $('collapse').style.display = devModeExpanded ? 'inline' : 'none';
  $('expand').style.display = devModeExpanded ? 'none' : 'inline';

  document.body.className =
      devModeExpanded ? 'showDevMode' : 'hideDevMode';
}

/**
 * Pop up a select dialog to capture the extension path.
 */
function selectExtensionPath() {
  showFileDialog('folder', 'packRoot', function(filePath) {
    $('extensionPathText').value = filePath;
  });
}

/**
 * Pop up a select dialog to capture the private key path.
 */
function selectPrivateKeyPath() {
  showFileDialog('file', 'pem', function(filePath) {
    $('privateKeyPath').value = filePath;
  });
}

/**
 * Handles the "Update extensions now" button being pressed.
 */
function autoUpdate() {
  chrome.send('autoupdate', []);
}

/**
 * Convenience routine to hook up nodes duplicated by jstemplate to
 * event handlers using the class attribute to identify the set of nodes.
 */
function addHandlerByClass(name, event, handler) {
  var elements = document.getElementsByClassName(name);
  for (var i = 0; i < elements.length; ++i) {
    elements[i].addEventListener(event, handler);
  }
}

document.addEventListener('DOMContentLoaded', requestExtensionsData);
