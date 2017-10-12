// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */

/**
 * Takes the |experimentalFeaturesData| input argument which represents data
 * about all the current feature entries and populates the html jstemplate with
 * that data. It expects an object structure like the above.
 * @param {Object} experimentalFeaturesData Information about all experiments.
 *     See returnFlagsExperiments() for the structure of this object.
 */
function renderTemplate(experimentalFeaturesData) {
  // This is the javascript code that processes the template:
  jstProcess(new JsEvalContext(experimentalFeaturesData), $('flagsTemplate'));

  // Add handlers to dynamically created HTML elements.
  var elements = document.getElementsByClassName('experiment-select');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onchange = function() {
      handleSelectExperimentalFeatureChoice(this, this.selectedIndex);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-disable-link');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = function() {
      handleEnableExperimentalFeature(this, false);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-enable-link');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = function() {
      handleEnableExperimentalFeature(this, true);
      return false;
    };
  }

  elements = document.getElementsByClassName('experiment-restart-button');
  for (var i = 0; i < elements.length; ++i) {
    elements[i].onclick = restartBrowser;
  }

  // Tab panel selection.
  var tabEls = document.getElementsByClassName('tab');
  for (var i = 0; i < tabEls.length; ++i) {
    tabEls[i].addEventListener('click', function(e) {
      e.preventDefault();
      for (var j= 0; j < tabEls.length; ++j) {
        tabEls[j].parentNode.classList.toggle('selected', tabEls[j] == this);
      }
    });
  }

  $('experiment-reset-all').onclick = resetAllFlags;

  highlightReferencedFlag();
  var search = FlagSearch.getInstance();
  search.init();
}

/**
 * Highlight an element associated with the page's location's hash. We need to
 * fake fragment navigation with '.scrollIntoView()', since the fragment IDs
 * don't actually exist until after the template code runs; normal navigation
 * therefore doesn't work.
 */
function highlightReferencedFlag() {
  if (window.location.hash) {
    var el = document.querySelector(window.location.hash);
    if (el && !el.classList.contains('referenced')) {
      // Unhighlight whatever's highlighted.
      if (document.querySelector('.referenced'))
        document.querySelector('.referenced').classList.remove('referenced');
      // Highlight the referenced element.
      el.classList.add('referenced');

      // Switch to unavailable tab if the flag is in this section.
      if ($('tab-content-unavailable').contains(el)) {
        $('tab-available').parentNode.classList.remove('selected');
        $('tab-unavailable').parentNode.classList.add('selected');
      }
      el.scrollIntoView();
    }
  }
}

/**
 * Asks the C++ FlagsDOMHandler to get details about the available experimental
 * features and return detailed data about the configuration. The
 * FlagsDOMHandler should reply to returnFlagsExperiments() (below).
 */
function requestExperimentalFeaturesData() {
  chrome.send('requestExperimentalFeatures');
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
  requestExperimentalFeaturesData();
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of all experimental features.
 * @param {Object} experimentalFeaturesData Information about all experimental
 *    features in the following format:
 *   {
 *     supportedFeatures: [
 *       {
 *         internal_name: 'Feature ID string',
 *         name: 'Feature name',
 *         description: 'Description',
 *         // enabled and default are only set if the feature is single valued.
 *         // enabled is true if the feature is currently enabled.
 *         // is_default is true if the feature is in its default state.
 *         enabled: true,
 *         is_default: false,
 *         // choices is only set if the entry has multiple values.
 *         choices: [
 *           {
 *             internal_name: 'Experimental feature ID string',
 *             description: 'description',
 *             selected: true
 *           }
 *         ],
 *         supported_platforms: [
 *           'Mac',
 *           'Linux'
 *         ],
 *       }
 *     ],
 *     unsupportedFeatures: [
 *       // Mirrors the format of |supportedFeatures| above.
 *     ],
 *     needsRestart: false,
 *     showBetaChannelPromotion: false,
 *     showDevChannelPromotion: false,
 *     showOwnerWarning: false
 *   }
 */
function returnExperimentalFeatures(experimentalFeaturesData) {
  var bodyContainer = $('body-container');
  renderTemplate(experimentalFeaturesData);

  if (experimentalFeaturesData.showBetaChannelPromotion)
    $('channel-promo-beta').hidden = false;
  else if (experimentalFeaturesData.showDevChannelPromotion)
    $('channel-promo-dev').hidden = false;

  bodyContainer.style.visibility = 'visible';
  var ownerWarningDiv = $('owner-warning');
  if (ownerWarningDiv)
    ownerWarningDiv.hidden = !experimentalFeaturesData.showOwnerWarning;
}

/**
 * Handles a 'enable' or 'disable' button getting clicked.
 * @param {HTMLElement} node The node for the experiment being changed.
 * @param {boolean} enable Whether to enable or disable the experiment.
 */
function handleEnableExperimentalFeature(node, enable) {
  // Tell the C++ FlagsDOMHandler to enable/disable the experiment.
  chrome.send('enableExperimentalFeature', [String(node.internal_name),
                                            String(enable)]);
  requestExperimentalFeaturesData();
}

/**
 * Invoked when the selection of a multi-value choice is changed to the
 * specified index.
 * @param {HTMLElement} node The node for the experiment being changed.
 * @param {number} index The index of the option that was selected.
 */
function handleSelectExperimentalFeatureChoice(node, index) {
  // Tell the C++ FlagsDOMHandler to enable the selected choice.
  chrome.send('enableExperimentalFeature',
              [String(node.internal_name) + '@' + index, 'true']);
  requestExperimentalFeaturesData();
}

/**
 * Handles in page searching. Matches against the experiment flag name.
 */
var FlagSearch = function() {
  FlagSearch.instance_ = this;

  this.experiments_ = [];
  this.unavailableExperiments_ = [];

  this.searchBox_ = $('search');
  this.noMatchMsg_ = document.querySelectorAll('.no-match');

  this.searchIntervalId_ = null;
  this.initialized = false;
};

// Delay in ms following a keypress, before a search is made.
FlagSearch.SEARCH_DEBOUNCE_TIME_MS = 150;

/**
 * Get the singleton instance of FlagSearch.
 * @return {Object} Instance of FlagSearch.
 */
FlagSearch.getInstance = function() {
  if (FlagSearch.instance_) {
    return FlagSearch.instance_;
  } else {
    return new FlagSearch();
  }
};

FlagSearch.prototype = {
  /**
   * Initialises the in page search. Adding searchbox listeners and
   * collates the permalinks used for string matching.
   */
  init: function() {
    this.experiments_ =
        document.querySelectorAll('#tab-content-available .permalink');
    this.unavailableExperiments_ =
        document.querySelectorAll('#tab-content-unavailable .permalink');

    if (!this.initialized) {
      this.searchBox_.addEventListener('keyup', this.debounceSearch.bind(this));
      document.querySelector('.clear-search').addEventListener('click',
          this.clearSearch.bind(this));

      window.addEventListener('keyup', function(e) {
          if (e.key == '/') {
            this.searchBox_.focus();
          }
      }.bind(this));
      this.searchBox_.focus();
      this.initialized = true;
    }
  },

  /**
   * Clears a search showing all experiments.
   */
  clearSearch: function() {
    this.searchBox_.value = '';
    this.doSearch();
  },

  /**
   * Reset existing highlights on an element.
   * @param {HTMLElement} el The element to remove all highlighted mark up on.
   * @param {string} flag The flag name to reset the element's textContent to.
   */
  resetHighlights: function(el, flag) {
    if (el.children) {
      el.textContent = flag;
    }
  },

  /**
   * Highlights the search term within the permalink flag name.
   * @param {string} searchTerm Search term user entered.
   * @param {HTMLElement} el The permalink node where the search tern occurs.
   * @return {boolean} Whether there was a match.
   */
  highlightMatches: function(searchTerm, el) {
    // Experiment container.
    var parentEl = el.parentNode.parentNode.parentNode;
    var flag = el.textContent.toLowerCase();
    var match = flag.indexOf(searchTerm);

    parentEl.classList.toggle('hidden', match == -1);

    if (match == -1) {
      this.resetHighlights(el, flag);
      return false;
    }

    if (searchTerm != '') {
      // Clear all nodes.
      el.textContent = '';

      if (match > 0) {
        var textNodePrefix =
            document.createTextNode(flag.substring(0, match));
        el.appendChild(textNodePrefix);
      }

      var matchEl = document.createElement('mark');
      matchEl.textContent = flag.substr(match, searchTerm.length);
      el.appendChild(matchEl);

      var matchSuffix = flag.substring(match + searchTerm.length);
      if (matchSuffix) {
        var textNodeSuffix = document.createTextNode(matchSuffix);
        el.appendChild(textNodeSuffix);
      }
    } else {
      this.resetHighlights(el, flag);
    }
    return true;
  },

  /**
   * Performs a search against the permalinks.
   * @param {Event} e
   */
  doSearch: function(e) {
    // Replace spaces with hyphens as flag names don't have spaces.
    var searchTerm =
        this.searchBox_.value.trim().toLowerCase().replace(/\s/, '-');
    var matches = 0;
    var unavailableMatches = 0;

    if (searchTerm || searchTerm == '') {
      if (searchTerm) {
        document.body.classList.add('searching');
      }
      // Available experiments
      for (var i = 0, j = this.experiments_.length; i < j; i++) {
        if (this.highlightMatches(searchTerm, this.experiments_[i])) {
          matches++;
        }
      }
      this.noMatchMsg_[0].classList.toggle('hidden', matches);

      // Unavailable experiments
      for (var i = 0, j = this.unavailableExperiments_.length; i < j; i++) {
        if (this.highlightMatches(searchTerm,
            this.unavailableExperiments_[i])) {
          unavailableMatches++;
        }
      }
      this.noMatchMsg_[1].classList.toggle('hidden', unavailableMatches);
    }

    this.searchIntervalId_ = null;
  },

  /**
   * Debounces the search to improve performance and prevent too many searches
   * from being initiated.
   * @param {Event} e
   */
  debounceSearch: function(e) {
    // Don't search if the search term did not change.
    if (this.searchValue_ == this.searchBox_.value) {
      return;
    }

    if (this.searchIntervalId_) {
      clearTimeout(this.searchIntervalId_);
    }
    this.searchIntervalId_ = setTimeout(this.doSearch.bind(this),
        FlagSearch.SEARCH_DEBOUNCE_TIME_MS);
  }
};

// Get and display the data upon loading.
document.addEventListener('DOMContentLoaded', requestExperimentalFeaturesData);

// Update the highlighted flag when the hash changes.
window.addEventListener('hashchange', highlightReferencedFlag);
