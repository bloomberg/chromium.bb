// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Code to execute before Closure's base.js.
 *
 * @author dmazzoni@google.com (Dominic Mazzoni)
 */

/**
 * Tell Closure to load JavaScript code from the extension root directory.
 * @type {boolean}
 */
window.CLOSURE_BASE_PATH = chrome.extension.getURL('/closure/');

/**
 * Tell Closure not to load deps.js; it's included by manifest.json already.
 * @type {boolean}
 */
window.CLOSURE_NO_DEPS = true;

/**
 * Array of urls that should be included next, in order.
 * @type {Array}
 * @private
 */
window.queue_ = [];

/**
 * Custom function for importing ChromeVox scripts.
 * @param {string} src The JS file to import.
 * @return {boolean} Whether the script was imported.
 */
window.CLOSURE_IMPORT_SCRIPT = function(src) {
  // Only run our version of the import script
  // when trying to inject ChromeVox scripts.
  if (src.indexOf('chrome-extension://') == 0) {
    if (!goog.inHtmlDocument_() ||
        goog.dependencies_.written[src]) {
      return false;
    }
    goog.dependencies_.written[src] = true;
    function loadNextScript() {
      if (goog.global.queue_.length == 0)
        return;

      var src = goog.global.queue_[0];

      if (window.CLOSURE_USE_EXT_MESSAGES) {
        var relativeSrc = src.substr(src.indexOf('closure/..') + 11);
        chrome.extension.sendMessage(
            {'srcFile': relativeSrc},
            function(response) {
              try {
                eval(response['code']);
              } catch (e) {
                console.error('Script error: ' + e + ' in ' + src);
              }
              goog.global.queue_ = goog.global.queue_.slice(1);
              loadNextScript();
            });
        return;
      }
      window.console.log('Using XHR');

      // Load the script by fetching its source and running 'eval' on it
      // directly, with a magic comment that makes Chrome treat it like it
      // loaded normally. Wait until it's fetched before loading the
      // next script.
      var xhr = new XMLHttpRequest();
      var url = src + '?' + new Date().getTime();
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          var scriptText = xhr.responseText;
          // Add a magic comment to the bottom of the file so that
          // Chrome knows the name of the script in the JavaScript debugger.
          scriptText += '\n//# sourceURL=' + src + '\n';
          eval(scriptText);
          goog.global.queue_ = goog.global.queue_.slice(1);
          loadNextScript();
        }
      };
      xhr.open('GET', url, false);
      xhr.send(null);
    }
    goog.global.queue_.push(src);
    if (goog.global.queue_.length == 1) {
      loadNextScript();
    }
    return true;
  } else {
    return goog.writeScriptTag_(src);
  }
};
