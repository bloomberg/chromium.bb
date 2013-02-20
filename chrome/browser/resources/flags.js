// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */

/**
 * Takes the |flagsExperimentsData| input argument which represents data about
 * the currently available experiments and populates the html jstemplate
 * with that data. It expects an object structure like the above.
 * @param {Object} flagsExperimentsData Information about available experiments.
 *     See returnFlagsExperiments() for the structure of this object.
 */
function renderTemplate(flagsExperimentsData) {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(flagsExperimentsData);
  var output = $('flagsExperimentTemplate');
  jstProcess(input, output);

  // Add handlers to dynamically created HTML elements.
  var elements = document.getElementsByClassName('experiment-select');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onchange = function() {
      handleSelectChoiceExperiment(this, this.selectedIndex);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-disable-link');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = function() {
      handleEnableExperiment(this, false);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-enable-link');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = function() {
      handleEnableExperiment(this, true);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-restart-button');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = restartBrowser;
  }

  $('experiment-reset-all').onclick = resetAllFlags;
}

/**
 * Asks the C++ FlagsDOMHandler to get details about the available experiments
 * and return detailed data about the configuration. The FlagsDOMHandler
 * should reply to returnFlagsExperiments() (below).
 */
function requestFlagsExperimentsData() {
  chrome.send('requestFlagsExperiments');
}

/**
 * Asks the C++ FlagsDOMHandler to restart the browser (restoring tabs).
 */
function restartBrowser() {
  chrome.send('restartBrowser');
}

/**
 * Reset all flags to their default values and refresh the UI.
 */
function resetAllFlags() {
  // Asks the C++ FlagsDOMHandler to reset all flags to default values.
  chrome.send('resetAllFlags');
  requestFlagsExperimentsData();
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of installed experiments.
 * @param {Object} flagsExperimentsData Information about available experiments
 *     in the following format:
 *   {
 *     flagsExperiments: [
 *       {
 *         internal_name: 'Experiment ID string',
 *         name: 'Experiment Name',
 *         description: 'description',
 *         // enabled is only set if the experiment is single valued.
 *         enabled: true,
 *         // choices is only set if the experiment has multiple values.
 *         choices: [
 *           {
 *             internal_name: 'Experiment ID string',
 *             description: 'description',
 *             selected: true
 *           }
 *         ],
 *         supported: true,
 *         supported_platforms: [
 *           'Mac',
 *           'Linux'
 *         ],
 *       }
 *     ],
 *     needsRestart: false
 *   }
 */
function returnFlagsExperiments(flagsExperimentsData) {
  var bodyContainer = $('body-container');
  renderTemplate(flagsExperimentsData);
  bodyContainer.style.visibility = 'visible';
}

/**
 * Handles a 'enable' or 'disable' button getting clicked.
 * @param {HTMLElement} node The node for the experiment being changed.
 * @param {boolean} enable Whether to enable or disable the experiment.
 */
function handleEnableExperiment(node, enable) {
  // Tell the C++ FlagsDOMHandler to enable/disable the experiment.
  chrome.send('enableFlagsExperiment', [String(node.internal_name),
                                        String(enable)]);
  requestFlagsExperimentsData();
}

/**
 * Invoked when the selection of a multi-value choice is changed to the
 * specified index.
 * @param {HTMLElement} node The node for the experiment being changed.
 * @param {number} index The index of the option that was selected.
 */
function handleSelectChoiceExperiment(node, index) {
  // Tell the C++ FlagsDOMHandler to enable the selected choice.
  chrome.send('enableFlagsExperiment',
              [String(node.internal_name) + '@' + index, 'true']);
  requestFlagsExperimentsData();
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestFlagsExperimentsData);

