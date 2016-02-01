// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Takes the |pluginsData| input argument which represents data about the
 * currently installed/running plugins and populates the html jstemplate with
 * that data.
 * @param {Object} pluginsData Detailed info about installed plugins. Same
 *     expected format as returnPluginsData().
 */
function renderTemplate(pluginsData) {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(pluginsData);
  var output = $('pluginTemplate');
  jstProcess(input, output);
}

// Keeps track of whether details have been made visible (expanded) or not.
var tmiModeExpanded = false;

/**
 * @param {boolean} showDetails
 */
function loadShowDetailsFromPrefs(showDetails) {
  tmiModeExpanded = showDetails;
  // TODO(dpapad): Use setAttribute()/removeAttribute() with 'hidden' instead of
  // style.display.
  $('collapse').style.display = showDetails ? 'inline' : 'none';
  $('expand').style.display = showDetails ? 'none' : 'inline';
  document.body.className = showDetails ? 'show-in-tmi-mode' : 'hide-tmi-mode';
}

/**
 * Called by the web_ui_ to re-populate the page with data representing the
 * current state of installed plugins.
 * @param {Object} pluginsData Detailed info about installed plugins. The
 *     template expects each plugin's format to match the following structure to
 *     correctly populate the page:
 *   {
 *     plugins: [
 *       {
 *         name: 'Group Name',
 *         description: 'description',
 *         version: 'version',
 *         update_url: 'http://update/',
 *         critical: true,
 *         enabled_mode: 'enabledByUser',
 *         id: 'plugin-name',
 *         always_allowed: false,
 *         plugin_files: [
 *           {
 *             path: '/foo/bar/baz/MyPlugin.plugin',
 *             name: 'MyPlugin',
 *             version: '1.2,3'
 *             description: 'My plugin',
 *             type: 'BROWSER PLUGIN',
 *             mime_types: [
 *               {
 *                 description: 'Foo Media',
 *                 file_extensions: ['pdf'],
 *                 mime_type: 'application/x-my-foo'
 *               },
 *               {
 *                 description: 'Bar Stuff',
 *                 file_extensions: ['bar', 'baz'],
 *                 mime_type: 'application/my-bar'
 *               }
 *             ],
 *             enabled_mode: 'enabledByUser',
 *           },
 *           {
 *             path: '/tmp/MyFirst.plugin',
 *             name: 'MyFirstPlugin',
 *             version: '3.14r15926',
 *             description: 'My first plugin',
 *             type: 'BROWSER PLUGIN',
 *             mime_types: [
 *               {
 *                 description: 'New Guy Media',
 *                 file_extensions: ['mfp'],
 *                 mime_type: 'application/x-my-first'
 *               },
 *             ],
 *             enabled_mode: 'disabledByUser',
 *           },
 *         ],
 *       },
 *     ],
 *   }
 */
function returnPluginsData(pluginsData) {
  var bodyContainer = $('body-container');
  var body = document.body;

  // Set all page content to be visible so we can measure heights.
  bodyContainer.style.visibility = 'hidden';
  body.className = '';
  var slidables = document.getElementsByClassName('show-in-tmi-mode');
  for (var i = 0; i < slidables.length; i++)
    slidables[i].style.height = 'auto';

  renderTemplate(pluginsData);

  // Add handlers to dynamically created HTML elements.
  var links = document.getElementsByClassName('disable-plugin-link');
  for (var i = 0; i < links.length; i++) {
    links[i].onclick = function() {
      handleEnablePlugin(this, false, false);
      return false;
    };
  }
  links = document.getElementsByClassName('enable-plugin-link');
  for (var i = 0; i < links.length; i++) {
    links[i].onclick = function() {
      handleEnablePlugin(this, true, false);
      return false;
    };
  }
  links = document.getElementsByClassName('disable-group-link');
  for (var i = 0; i < links.length; i++) {
    links[i].onclick = function() {
      handleEnablePlugin(this, false, true);
      return false;
    };
  }
  links = document.getElementsByClassName('enable-group-link');
  for (var i = 0; i < links.length; i++) {
    links[i].onclick = function() {
      handleEnablePlugin(this, true, true);
      return false;
    };
  }
  var checkboxes = document.getElementsByClassName('always-allow');
  for (var i = 0; i < checkboxes.length; i++) {
    checkboxes[i].onclick = function() {
      handleSetPluginAlwaysAllowed(this);
    };
  }

  if (cr.isChromeOS) {
    // Disable some controls for Guest in ChromeOS.
    uiAccountTweaks.UIAccountTweaks.applyGuestSessionVisibility(document);
    // Disable some controls for Public session in ChromeOS.
    uiAccountTweaks.UIAccountTweaks.applyPublicSessionVisibility(document);
  }

  // Make sure the left column (with "Description:", "Location:", etc.) is the
  // same size for all plugins.
  var labels = document.getElementsByClassName('plugin-details-label');
  var maxLabelWidth = 0;
  for (var i = 0; i < labels.length; i++)
    labels[i].style.width = 'auto';
  for (var i = 0; i < labels.length; i++)
    maxLabelWidth = Math.max(maxLabelWidth, labels[i].offsetWidth);
  for (var i = 0; i < labels.length; i++)
    labels[i].style.width = maxLabelWidth + 'px';

  // Explicitly set the height for each element that wants to be "slid" in and
  // out when the tmiModeExpanded is toggled.
  var slidables = document.getElementsByClassName('show-in-tmi-mode');
  for (var i = 0; i < slidables.length; i++)
    slidables[i].style.height = slidables[i].offsetHeight + 'px';

  // Reset visibility of page based on the current tmi mode.
  $('collapse').style.display =
     tmiModeExpanded ? 'inline' : 'none';
  $('expand').style.display =
     tmiModeExpanded ? 'none' : 'inline';
  bodyContainer.style.visibility = 'visible';
  body.className = tmiModeExpanded ?
     'show-tmi-mode-initial' : 'hide-tmi-mode-initial';
}

/**
 * Handles a 'enable' or 'disable' button getting clicked.
 * @param {HTMLElement} node The HTML element for the plugin being changed.
 * @param {boolean} enable Whether to enable or disable the plugin.
 * @param {boolean} isGroup True if we're enabling/disabling a plugin group,
 *     rather than a single plugin.
 */
function handleEnablePlugin(node, enable, isGroup) {
  whenBrowserProxyReady.then(function(browserProxy) {
    if (isGroup)
      browserProxy.setPluginGroupEnabled(node.path, enable);
    else
      browserProxy.setPluginEnabled(node.path, enable);
  });
}

/*
 * Toggles visibility of details.
 */
function toggleTmiMode() {
  tmiModeExpanded = !tmiModeExpanded;

  $('collapse').style.display =
      tmiModeExpanded ? 'inline' : 'none';
  $('expand').style.display =
      tmiModeExpanded ? 'none' : 'inline';

  document.body.className =
      tmiModeExpanded ? 'show-tmi-mode' : 'hide-tmi-mode';

  whenBrowserProxyReady.then(function(browserProxy) {
    browserProxy.saveShowDetailsToPrefs(tmiModeExpanded);
  });
}

function handleSetPluginAlwaysAllowed(el) {
  whenBrowserProxyReady.then(function(browserProxy) {
    browserProxy.setPluginAlwaysAllowed(el.identifier, el.checked);
  });
}

/**
 * @param {Object} plugin An object containing the information about a plugin.
 *     See returnPluginsData() for the format of this object.
 * @return {boolean} Whether the plugin's version should be displayed.
 */
function shouldDisplayPluginVersion(plugin) {
  return !!plugin.version && plugin.version != '0';
}

/**
 * @param {Object} plugin An object containing the information about a plugin.
 *     See returnPluginsData() for the format of this object.
 * @return {boolean} Whether the plugin's description should be displayed.
 */
function shouldDisplayPluginDescription(plugin) {
  // Only display the description if it's not blank and if it's not just the
  // name, version, or combination thereof.
  return plugin.description &&
         plugin.description != plugin.name &&
         plugin.description != plugin.version &&
         plugin.description != 'Version ' + plugin.version &&
         plugin.description != plugin.name + ' ' + plugin.version;
}

/**
 * @param {Object} plugin An object containing the information about a plugin.
 *     See returnPluginsData() for the format of this object.
 * @return {boolean} Whether the plugin is enabled.
 */
function isPluginEnabled(plugin) {
  return plugin.enabled_mode == 'enabledByUser' ||
         plugin.enabled_mode == 'enabledByPolicy';
}

// Unfortunately, we don't have notifications for plugin (list) status changes
// (yet), so in the meanwhile just update regularly.
setInterval(function() {
  whenBrowserProxyReady.then(function(browserProxy) {
    return browserProxy.getPluginsData();
  }).then(returnPluginsData);
}, 30000);

// Add handlers to static HTML elements.
$('collapse').onclick = toggleTmiMode;
$('expand').onclick = toggleTmiMode;
$('details-link').onclick = toggleTmiMode;

// NOTE: Need to keep a reference to the stub here such that it is not garbage
// collected, which causes the pipe to close and future calls from C++ to JS to
// get dropped.
var pluginsPageStub = null;

/**
 * A promise to use for getting a reference to the proxy object allowing
 * communication with the browser side.
 * @type {!Promise}
 */
var whenBrowserProxyReady = new Promise(function(resolve, reject) {
  define([
    'mojo/public/js/bindings',
    'mojo/public/js/core',
    'mojo/public/js/connection',
    'chrome/browser/ui/webui/plugins/plugins.mojom',
    'content/public/renderer/service_provider',
  ], function(bindings, core, connection, pluginsMojom, serviceProvider) {
    var browserProxy = connection.bindHandleToProxy(
        serviceProvider.connectToService(
            pluginsMojom.PluginsHandlerMojo.name),
        pluginsMojom.PluginsHandlerMojo);

    // Perform necessary setup for the C++ side to make calls to JS at will.

    // Connect pipe handle to JS code.
    var pipe = core.createMessagePipe();
    pluginsPageStub = connection.bindHandleToStub(
        pipe.handle0, pluginsMojom.PluginsPageMojo);

    bindings.StubBindings(pluginsPageStub).delegate = {
      __proto__: pluginsMojom.PluginsPageMojo.stubClass.prototype,
      onPluginsUpdated: function(plugins) {
        returnPluginsData({plugins: plugins});
      },
    };

    // Send pipe handle to C++.
    browserProxy.setClientPage(pipe.handle1);

    resolve(browserProxy);
  });
});

/**
 * @type {!Promise} A promise firing when DOMContentLoaded event is received.
 */
var whenDomContentLoaded = new Promise(function(resolve, reject) {
  document.addEventListener('DOMContentLoaded', resolve);
});

Promise.all([
  whenDomContentLoaded,
  whenBrowserProxyReady
]).then(function(results) {
  var browserProxy = results[1];
  browserProxy.getShowDetails().then(function(response) {
    // Set the |tmiModeExpanded| first otherwise the UI flickers when
    // returnPlignsData executes.
    loadShowDetailsFromPrefs(response.show_details);
    return browserProxy.getPluginsData();
  }).then(returnPluginsData);
});
