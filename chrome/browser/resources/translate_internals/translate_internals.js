// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  cr.define('cr.translateInternals', function() {

    var detectionLogs_ = null;

    function detectionLogs() {
      if (detectionLogs_ === null)
        detectionLogs_ = [];
      return detectionLogs_;
    }

    /**
     * Initializes UI and sends a message to the browser for
     * initialization.
     */
    function initialize() {
      cr.ui.decorate('tabbox', cr.ui.TabBox);
      chrome.send('requestInfo');

      var button = $('detection-logs-dump');
      button.addEventListener('click', onDetectionLogsDump);
    }

    /**
     * Creates a new LI element with a button to dismiss the item.
     *
     * @param {string} text The lable of the LI element.
     * @param {Function} func Callback called when the button is clicked.
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
     * Formats the language name to a human-readable text. For example, if
     * |langCode| is 'en', this may return 'en (English)'.
     *
     * @param {string} langCode ISO 639 language code.
     * @return {string} The formatted string.
     */
    function formatLanguageCode(langCode) {
      var key = 'language-' + langCode;
      if (key in templateData) {
        var langName = templateData[key];
        return langCode + ' (' + langName + ')';
      }

      return langCode;
    }

    /**
     * Formats the error type to a human-readable text.
     *
     * @param {string} error Translation error type from the browser.
     * @return {string} The formatted string.
     */
    function formatTranslateErrorsType(error) {
      // This list is from chrome/common/translate_errors.h.  If this header
      // file is updated, the below list also should be updated.
      var errorStrs = {
        0: 'None',
        1: 'Network',
        2: 'Initialization Error',
        3: 'Unknown Language',
        4: 'Unsupported Language',
        5: 'Identical Languages',
        6: 'Translation Error',
      };

      if (error < 0 || errorStrs.length <= error) {
        console.error('Invalid error code:', error);
        return 'Invalid Error Code';
      }
      return errorStrs[error];
    }

    /**
     * Handles the message of 'prefsUpdated' from the browser.
     *
     * @param {Object} detail the object which represents pref values.
     */
    function onPrefsUpdated(detail) {
      var ul;
      ul = document.querySelector('#prefs-language-blacklist ul');
      ul.innerHTML = '';

      if ('translate_language_blacklist' in detail) {
        var langs = detail['translate_language_blacklist'];

        langs.forEach(function(langCode) {
          var text = formatLanguageCode(langCode);

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
          var text = formatLanguageCode(fromLangCode) + ' \u2192 ' +
              formatLanguageCode(toLangCode);

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
     * Addes '0's to |number| as a string. |width| is length of the string
     * including '0's.
     *
     * @param {string} number The number to be converted into a string.
     * @param {number} width The width of the returned string.
     * @return {string} The formatted string.
     */
    function padWithZeros(number, width) {
      var numberStr = number.toString();
      var restWidth = width - numberStr.length;
      if (restWidth <= 0)
        return numberStr;

      return Array(restWidth + 1).join('0') + numberStr;
    }

    /**
     * Formats |date| as a Date object into a string. The format is like
     * '2006-01-02 15:04:05'.
     *
     * @param {Date} date Date to be formatted.
     * @return {string} The formatted string.
     */
    function formatDate(date) {
      var year = date.getFullYear();
      var month = date.getMonth() + 1;
      var day = date.getDay();
      var hour = date.getHours();
      var minute = date.getMinutes();
      var second = date.getSeconds();

      var yearStr = padWithZeros(year, 4);
      var monthStr = padWithZeros(month, 2);
      var dayStr = padWithZeros(day, 2);
      var hourStr = padWithZeros(hour, 2);
      var minuteStr = padWithZeros(minute, 2);
      var secondStr = padWithZeros(second, 2);

      var str = yearStr + '-' + monthStr + '-' + dayStr + ' ' +
          hourStr + ':' + minuteStr + ':' + secondStr;

      return str;
    }

    /**
     * Returns a new TD element.
     *
     * @param {string} content The text content of the element.
     * @param {string} className The class name of the element.
     * @return {string} The new TD element.
     */
    function createTD(content, className) {
      var td = document.createElement('td');
      td.textContent = content;
      td.className = className;
      return td;
    }

    /**
     * Handles the message of 'languageDetectionInfoAdded' from the
     * browser.
     *
     * @param {Object} detail The object which represents the logs.
     */
    function onLanguageDetectionInfoAdded(detail) {
      cr.translateInternals.detectionLogs().push(detail);

      var tr = document.createElement('tr');

      var date = new Date(detail['time']);
      [
        createTD(formatDate(date), 'detection-logs-time'),
        createTD(detail['url'], 'detection-logs-url'),
        createTD(formatLanguageCode(detail['content_language']),
                 'detection-logs-content-language'),
        createTD(formatLanguageCode(detail['cld_language']),
                 'detection-logs-cld-language'),
        createTD(detail['is_cld_reliable'],
                 'detection-logs-is-cld-reliable'),
        createTD(formatLanguageCode(detail['language']),
                 'detection-logs-language'),
        createTD(formatLanguageCode(detail['content']),
                 'detection-logs-content'),
      ].forEach(function(td) {
        tr.appendChild(td);
      });

      // TD (and TR) can't use the CSS property 'max-height', so DIV
      // in the content is needed.
      var contentTD = tr.querySelector('.detection-logs-content');
      var div = document.createElement('div');
      div.textContent = contentTD.textContent;
      contentTD.textContent = '';
      contentTD.appendChild(div);

      var tbody = $('detection-logs').getElementsByTagName('tbody')[0];
      tbody.appendChild(tr);
    }

    /**
     * Handles the message of 'translateErrorDetailsAdded' from the
     * browser.
     *
     * @param {Object} details The object which represents the logs.
     */
    function onTranslateErrorDetailsAdded(details) {
      var tr = document.createElement('tr');

      var errorStr = details['error'] + ': ' +
          formatTranslateErrorsType(details['error']);
      [
        createTD(formatDate(new Date(details['time'])),
                 'error-logs-time'),
        createTD(details['url'], 'error-logs-url'),
        createTD(errorStr, 'error-logs-error'),
      ].forEach(function(td) {
        tr.appendChild(td);
      });

      var tbody = $('error-logs').getElementsByTagName('tbody')[0];
      tbody.appendChild(tr);
    }

    /**
     * The callback entry point from the browser. This function will be
     * called by the browser.
     *
     * @param {string} message The name of the sent message.
     * @param {Object} detail The argument of the sent message.
     */
    function messageHandler(message, detail) {
      switch (message) {
        case 'languageDetectionInfoAdded':
          cr.translateInternals.onLanguageDetectionInfoAdded(detail);
          break;
        case 'prefsUpdated':
          cr.translateInternals.onPrefsUpdated(detail);
          break;
        case 'translateErrorDetailsAdded':
          cr.translateInternals.onTranslateErrorDetailsAdded(detail);
          break;
        default:
          console.error('Unknown message:', message);
          break;
      }
    }

    /**
     * The callback of button#detetion-logs-dump.
     */
    function onDetectionLogsDump() {
      var data = JSON.stringify(cr.translateInternals.detectionLogs());
      var blob = new Blob([data], {'type': 'text/json'});
      var url = webkitURL.createObjectURL(blob);
      var filename = 'translate_internals_detect_logs_dump.json';

      var a = document.createElement('a');
      a.setAttribute('href', url);
      a.setAttribute('download', filename);

      var event = document.createEvent('MouseEvent');
      event.initMouseEvent('click', true, true, window, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, null);
      a.dispatchEvent(event);
    }

    return {
      detectionLogs: detectionLogs,
      initialize: initialize,
      messageHandler: messageHandler,
      onLanguageDetectionInfoAdded: onLanguageDetectionInfoAdded,
      onPrefsUpdated: onPrefsUpdated,
      onTranslateErrorDetailsAdded: onTranslateErrorDetailsAdded,
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
