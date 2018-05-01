// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Stub methods to allow the closure compiler to compile
 * successfully for external dependencies which cannot be included in
 * compiled_resources2.gyp.
 */

/**
 * @constructor
 */
Window.prototype.speechSynthesis = function() {};

/**
 * @type {function(Object)}
 */
Window.prototype.speechSynthesis.speak = function(utterance) {};

/**
 * @constructor
 */
Window.prototype.SpeechSynthesisUtterance = function() {};

/**
 * @typedef {{languageCode: string, name: string, displayLanguage: string,
 *   extensionId: string, id: string}}
 */
let TtsHandlerVoice;

/**
 * @typedef {{name: string, extensionId: string, optionsPage: string}}
 */
let TtsHandlerExtension;
