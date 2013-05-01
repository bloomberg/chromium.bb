// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  cr.define('cr.translateInternals', function() {

    /**
     * Initializes UI and sends a message to the browser for
     * initialization.
     */
    function initialize() {
      cr.ui.decorate('tabbox', cr.ui.TabBox);
      chrome.send('requestInfo');
    }

    /**
     * Creates a new LI element with a button to dismiss the item.
     * @param {string} text lable of the LI element.
     * @param {Function} func callback  when the button is clicked.
     */
    function createLIWithDismissingButton(text, func) {
      var span = document.createElement('span');
      span.textContent = text;

      var li = document.createElement('li');
      li.appendChild(span);

      var button = document.createElement('button');
      button.textContent = 'X';
      button.addEventListener('click', function(e) {
        e.preventDefault();
        func();
      }, false);

      li.appendChild(button);
      return li;
    }

    /**
     * Handles the message of 'prefsUpdated' from the browser.
     * @param {Object} detail The object which represents pref values.
     */
    function onPrefsUpdated(detail) {
      var ul;
      ul = document.querySelector('#prefs-language-blacklist ul');
      ul.innerHTML = '';

      if ('translate_language_blacklist' in detail) {
        var langs = detail['translate_language_blacklist'];

        langs.forEach(function(langCode) {
          var langName = templateData['language-' + langCode];
          var text = langCode + ' (' + langName + ')';

          var li = createLIWithDismissingButton(text, function() {
            chrome.send('removePrefItem',
                        ['language_blacklist', langCode]);
          });
          ul.appendChild(li);
        });
      }

      ul = document.querySelector('#prefs-site-blacklist ul');
      ul.innerHTML = '';

      if ('translate_site_blacklist' in detail) {
        var sites = detail['translate_site_blacklist'];

        sites.forEach(function(site) {
          var li = createLIWithDismissingButton(site, function() {
            chrome.send('removePrefItem', ['site_blacklist', site]);
          });
          ul.appendChild(li);
        });
      }

      ul = document.querySelector('#prefs-whitelists ul');
      ul.innerHTML = '';

      if ('translate_whitelists' in detail) {
        var pairs = detail['translate_whitelists'];

        Object.keys(pairs).forEach(function(fromLangCode) {
          var toLangCode = pairs[fromLangCode];
          var fromLangName = templateData['language-' + fromLangCode];
          var toLangName = templateData['language-' + toLangCode];
          var text = fromLangCode + ' (' + fromLangName + ') \u2192 ' +
              toLangCode + ' (' + toLangName + ')';

          var li = createLIWithDismissingButton(text, function() {
            chrome.send('removePrefItem',
                        ['whitelists', fromLangCode, toLangCode]);
          });
          ul.appendChild(li);
        });
      }

      var p = document.querySelector('#prefs-dump p');
      var content = JSON.stringify(detail, null, 2);
      p.textContent = content;
    }

    /**
     * The callback entry point from the browser. This function will be
     * called by the browser.
     * @param {string} message The name of the sent message
     * @param {Object} detail The argument of the sent message
     */
    function messageHandler(message, detail) {
      switch (message) {
        case 'prefsUpdated':
          cr.translateInternals.onPrefsUpdated(detail);
          break;
        default:
          console.error('Unknown message:', message);
          break;
      }
    }

    return {
      initialize: initialize,
      messageHandler: messageHandler,
      onPrefsUpdated: onPrefsUpdated
    };
  });

  /**
   * The entry point of the UI.
   */
  function main() {
    cr.doc.addEventListener('DOMContentLoaded',
                            cr.translateInternals.initialize);
  }

  main();
})();
