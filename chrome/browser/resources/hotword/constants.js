// Copyright 2014 The Chromium Authors. All rights reserved.
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
 * Name used by the content scripts to create communications Ports.
 * @const {string}
 */
var CLIENT_PORT_NAME = 'chwcpn';

/**
 * The field name to specify the command among pages.
 * @const {string}
 */
var COMMAND_FIELD_NAME = 'cmd';

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
 * Messages sent from the injected scripts to the Google page.
 * @enum {string}
 */
var CommandToPage = {
  HOTWORD_VOICE_TRIGGER: 'vt',
  HOTWORD_STARTED: 'hs',
  HOTWORD_ENDED: 'hd',
  HOTWORD_TIMEOUT: 'ht',
  HOTWORD_ERROR: 'he'
};

/**
 * Messages sent from the Google page to the injected scripts
 * and then passed to the extension.
 * @enum {string}
 */
var CommandFromPage = {
  SPEECH_START: 'ss',
  SPEECH_END: 'se',
  SPEECH_RESET: 'sr',
  SHOWING_HOTWORD_START: 'shs',
  SHOWING_ERROR_MESSAGE: 'sem',
  SHOWING_TIMEOUT_MESSAGE: 'stm',
  CLICKED_RESUME: 'hcc',
  CLICKED_RESTART: 'hcr',
  CLICKED_DEBUG: 'hcd',
  WAKE_UP_HELPER: 'wuh'
};

/**
 * Source of a hotwording session request.
 * @enum {string}
 */
var SessionSource = {
  LAUNCHER: 'launcher',
  NTP: 'ntp',
  ALWAYS: 'always'
};

/**
 * The browser UI language.
 * @const {string}
 */
var UI_LANGUAGE = (chrome.i18n && chrome.i18n.getUILanguage) ?
      chrome.i18n.getUILanguage() : '';

return {
  CLIENT_PORT_NAME: CLIENT_PORT_NAME,
  COMMAND_FIELD_NAME: COMMAND_FIELD_NAME,
  SHARED_MODULE_ID: SHARED_MODULE_ID,
  SHARED_MODULE_ROOT: SHARED_MODULE_ROOT,
  UI_LANGUAGE: UI_LANGUAGE,
  CommandToPage: CommandToPage,
  CommandFromPage: CommandFromPage,
  Error: Error,
  Event: Event,
  File: File,
  NaClPlugin: NaClPlugin,
  SessionSource: SessionSource,
  TimeoutMs: TimeoutMs
};

});
