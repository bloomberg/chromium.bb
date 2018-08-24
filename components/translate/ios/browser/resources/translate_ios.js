// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Installs iOS Translate callbacks on cr.googleTranslate.
 *
 * TODO(crbug.com/659442): Enable checkTypes, checkVars errors for this file.
 * @suppress {checkTypes, checkVars}
 */

(function() {
/**
 * Sets a callback to inform host of the ready state of the translate element.
 */
cr.googleTranslate.readyCallback = function() {
  __gCrWeb.message.invokeOnHost({
      'command': 'translate.ready',
      'timeout': cr.googleTranslate.error,
      'loadTime': cr.googleTranslate.loadTime,
      'readyTime': cr.googleTranslate.readyTime});
}

/**
 * Sets a callback to inform host of the result of translation.
 */
cr.googleTranslate.resultCallback = function() {
  __gCrWeb.message.invokeOnHost({
      'command': 'translate.status',
      'success': !cr.googleTranslate.error,
      'originalPageLanguage': cr.googleTranslate.sourceLang,
      'translationTime': cr.googleTranslate.translationTime});
}

}());  // anonymous function
