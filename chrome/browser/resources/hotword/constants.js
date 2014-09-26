// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword.constants', function() {
'use strict';

/**
 * Hotword data shared module extension's ID.
 * @const {string}
 */
var SHARED_MODULE_ID = 'lccekmodgklaepjeofjdjpbminllajkg';

/**
 * Path to shared module data.
 * @const {string}
 */
var SHARED_MODULE_ROOT = '_modules/' + SHARED_MODULE_ID;

/**
 * Time to wait for expected messages, in milliseconds.
 * @enum {number}
 */
var TimeoutMs = {
  SHORT: 200,
  NORMAL: 500,
  LONG: 2000
};

/**
 * The URL of the files used by the plugin.
 * @enum {string}
 */
var File = {
  RECOGNIZER_CONFIG: 'hotword.data',
};

/**
 * Errors emitted by the NaClManager.
 * @enum {string}
 */
var Error = {
  NACL_CRASH: 'nacl_crash',
  TIMEOUT: 'timeout',
};

/**
 * Event types supported by NaClManager.
 * @enum {string}
 */
var Event = {
  READY: 'ready',
  TRIGGER: 'trigger',
  ERROR: 'error',
};

/**
 * Messages for communicating with the NaCl recognizer plugin. These must match
 * constants in <google3>/hotword_plugin.c
 * @enum {string}
 */
var NaClPlugin = {
  RESTART: 'r',
  SAMPLE_RATE_PREFIX: 'h',
  MODEL_PREFIX: 'm',
  STOP: 's',
  REQUEST_MODEL: 'model',
  MODEL_LOADED: 'model_loaded',
  READY_FOR_AUDIO: 'audio',
  STOPPED: 'stopped',
  HOTWORD_DETECTED: 'hotword',
  MS_CONFIGURED: 'ms_configured'
};

/**
 * Source of a hotwording session request.
 * @enum {string}
 */
var SessionSource = {
  LAUNCHER: 'launcher'
};

/**
 * The browser UI language.
 * @const {string}
 */
var UI_LANGUAGE = (chrome.i18n && chrome.i18n.getUILanguage) ?
      chrome.i18n.getUILanguage() : '';

return {
  SHARED_MODULE_ID: SHARED_MODULE_ID,
  SHARED_MODULE_ROOT: SHARED_MODULE_ROOT,
  TimeoutMs: TimeoutMs,
  File: File,
  Error: Error,
  Event: Event,
  NaClPlugin: NaClPlugin,
  SessionSource: SessionSource,
  UI_LANGUAGE: UI_LANGUAGE
};

});
