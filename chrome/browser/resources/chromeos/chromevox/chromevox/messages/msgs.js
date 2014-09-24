// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Defines methods related to retrieving translated messages.
 */

goog.provide('cvox.Msgs');

/**
 * @constructor
 */
cvox.Msgs = function() {
  /**
   * @type {Object.<string, string>}
   * @private
   */
  this.localeNameDict_ = null;
};


/**
 * The namespace for all Chromevox messages.
 * @type {string}
 * @const
 * @private
 */
cvox.Msgs.NAMESPACE_ = 'chromevox_';


/**
 * Return the current locale.
 * @return {string} The locale.
 */
cvox.Msgs.prototype.getLocale = function() {
  return chrome.i18n.getMessage('locale');
};


/**
 * Returns the message with the given message id from the ChromeVox namespace.
 *
 * If we can't find a message, throw an exception.  This allows us to catch
 * typos early.
 *
 * @param {string} messageId The id.
 * @param {Array.<string>=} opt_subs Substitution strings.
 * @return {string} The message.
 */
cvox.Msgs.prototype.getMsg = function(messageId, opt_subs) {
  var message = chrome.i18n.getMessage(
      cvox.Msgs.NAMESPACE_ + messageId, opt_subs);
  if (message == undefined || message == '') {
    throw new Error('Invalid ChromeVox message id: ' + messageId);
  }
  return message;
};


/**
 * Processes an HTML DOM the text of "i18n" elements with translated messages.
 * This function expects HTML elements with a i18n clean and a msgid attribute.
 *
 * @param {Node} root The root node where the translation should be performed.
 */
cvox.Msgs.prototype.addTranslatedMessagesToDom = function(root) {
  var elts = root.querySelectorAll('.i18n');
  for (var i = 0; i < elts.length; i++) {
    var msgid = elts[i].getAttribute('msgid');
    if (!msgid) {
      throw new Error('Element has no msgid attribute: ' + elts[i]);
    }
    elts[i].textContent = this.getMsg(msgid);
    elts[i].classList.add('i18n-processed');
  }
};


/**
 * Retuns a number formatted correctly.
 *
 * @param {number} num The number.
 * @return {string} The number in the correct locale.
 */
cvox.Msgs.prototype.getNumber = function(num) {
  return '' + num;
};

/**
 * Gets a localized display name for a locale.
 * NOTE: Only a subset of locale identifiers are supported.  See the
 * |CHROMEVOX_LOCALE_DICT| message.
 * @param {string} locale On the form |ll| or |ll_CC|, where |ll| is
 *     the language code and |CC| the country code.
 * @return {string} The display name.
 */
cvox.Msgs.prototype.getLocaleDisplayName = function(locale) {
  if (!this.localeNameDict_) {
    this.localeNameDict_ = /** @type {Object.<string, string>} */(
        JSON.parse(this.getMsg('locale_dict')));
  }
  var name = this.localeNameDict_[locale];
  if (!name) {
    throw Error('Unsupported locale identifier: ' + locale);
  }
  return name;
};
