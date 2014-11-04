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
  ALWAYS: 'always',
  TRAINING: 'training'
};

/**
 * MediaStream open success/errors to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaMediaStreamOpenResult = {
  SUCCESS: 0,
  UNKNOWN: 1,
  NOT_SUPPORTED: 2,
  PERMISSION_DENIED: 3,
  CONSTRAINT_NOT_SATISFIED: 4,
  OVERCONSTRAINED: 5,
  NOT_FOUND: 6,
  ABORT: 7,
  SOURCE_UNAVAILABLE: 8,
  PERMISSION_DISMISSED: 9,
  INVALID_STATE: 10,
  DEVICES_NOT_FOUND: 11,
  INVALID_SECURITY_ORIGIN: 12,
  MAX: 12
};

/**
 * UMA metrics.
 * DO NOT change these enum values.
 * @enum {string}
 */
var UmaMetrics = {
  TRIGGER: 'Hotword.HotwordTrigger',
  MEDIA_STREAM_RESULT: 'Hotword.HotwordMediaStreamResult',
  NACL_PLUGIN_LOAD_RESULT: 'Hotword.HotwordNaClPluginLoadResult',
  NACL_MESSAGE_TIMEOUT: 'Hotword.HotwordNaClMessageTimeout'
};

/**
 * Message waited for by NaCl plugin, to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaNaClMessageTimeout = {
  REQUEST_MODEL: 0,
  MODEL_LOADED: 1,
  READY_FOR_AUDIO: 2,
  STOPPED: 3,
  HOTWORD_DETECTED: 4,
  MS_CONFIGURED: 5,
  MAX: 5
};

/**
 * NaCl plugin load success/errors to be reported via UMA.
 * DO NOT remove or renumber values in this enum. Only add new ones.
 * @enum {number}
 */
var UmaNaClPluginLoadResult = {
  SUCCESS: 0,
  UNKNOWN: 1,
  CRASH: 2,
  NO_MODULE_FOUND: 3,
  MAX: 3
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
  TimeoutMs: TimeoutMs,
  UmaMediaStreamOpenResult: UmaMediaStreamOpenResult,
  UmaMetrics: UmaMetrics,
  UmaNaClMessageTimeout: UmaNaClMessageTimeout,
  UmaNaClPluginLoadResult: UmaNaClPluginLoadResult
};

});
