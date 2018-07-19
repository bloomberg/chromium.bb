// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities for UMA metrics.

/**
 * @constructor
 */
let MetricsUtils = function() {};

/**
 * Defines an enumeration metric. The |EVENT_COUNT| must be kept in sync
 * with the number of enum values for each metric in
 * tools/metrics/histograms/enums.xml.
 * @typedef {{EVENT_COUNT: number, METRIC_NAME: string}}
 */
MetricsUtils.EnumerationMetric;

/**
 * CrosSelectToSpeakStartSpeechMethod enums.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
MetricsUtils.StartSpeechMethod = {
  MOUSE: 0,
  KEYSTROKE: 1,
};

/**
 * Constants for the start speech method metric,
 * CrosSelectToSpeakStartSpeechMethod.
 * @type {MetricsUtils.EnumerationMetric}
 */
MetricsUtils.START_SPEECH_METHOD_METRIC = {
  EVENT_COUNT: Object.keys(MetricsUtils.StartSpeechMethod).length,
  METRIC_NAME: 'Accessibility.CrosSelectToSpeak.StartSpeechMethod'
};

/**
 * CrosSelectToSpeakStateChangeEvent enums.
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
MetricsUtils.StateChangeEvent = {
  START_SELECTION: 0,
  CANCEL_SPEECH: 1,
  CANCEL_SELECTION: 2,
};

/**
 * Constants for the state change metric, CrosSelectToSpeakStateChangeEvent.
 * @type {MetricsUtils.EnumerationMetric}
 */
MetricsUtils.STATE_CHANGE_METRIC = {
  EVENT_COUNT: Object.keys(MetricsUtils.StateChangeEvent).length,
  METRIC_NAME: 'Accessibility.CrosSelectToSpeak.StateChangeEvent'
};

/**
 * The start speech metric name.
 * @type {string}
 */
MetricsUtils.START_SPEECH_METRIC =
    'Accessibility.CrosSelectToSpeak.StartSpeech';

/**
 * The cancel speech metric name.
 * @type {string}
 */
MetricsUtils.CANCEL_SPEECH_METRIC =
    'Accessibility.CrosSelectToSpeak.CancelSpeech';

/**
 * The speech pitch histogram metric name.
 * @type {string}
 */
MetricsUtils.SPEECH_PITCH_METRIC =
    'Accessibility.CrosSelectToSpeak.SpeechPitch';

/**
 * The speech rate histogram metric name.
 * @type {string}
 */
MetricsUtils.SPEECH_RATE_METRIC = 'Accessibility.CrosSelectToSpeak.SpeechRate';

/**
 * The word highlighting metric name.
 * @type {string}
 */
MetricsUtils.WORD_HIGHLIGHTING_METRIC =
    'Accessibility.CrosSelectToSpeak.WordHighlighting';


/**
 * Records a cancel event if speech was in progress.
 * @param {boolean} speaking Whether speech was in progress
 * @public
 */
MetricsUtils.recordCancelIfSpeaking = function(speaking) {
  if (speaking) {
    MetricsUtils.recordCancelEvent_();
  }
};

/**
 * Converts the speech rate into an enum based on
 * tools/metrics/histograms/enums.xml.
 * These values are persisted to logs. Entries should not be
 * renumbered and numeric values should never be reused.
 * @param {number} speechRate The current speech rate.
 * @return {number} The current speech rate as an int for metrics.
 * @private
 */
MetricsUtils.speechRateToSparceHistogramInt_ = function(speechRate) {
  return speechRate * 100;
};

/**
 * Converts the speech pitch into an enum based on
 * tools/metrics/histograms/enums.xml.
 * These values are persisted to logs. Entries should not be
 * renumbered and numeric values should never be reused.
 * @param {number} speechPitch The current speech pitch.
 * @return {number} The current speech pitch as an int for metrics.
 * @private
 */
MetricsUtils.speechPitchToSparceHistogramInt_ = function(speechPitch) {
  return speechPitch * 100;
};

/**
 * Records an event that Select-to-Speak has begun speaking.
 * @param {number} method The CrosSelectToSpeakStartSpeechMethod enum
 *    that reflects how this event was triggered by the user.
 * @param {number} speechRate The current speech rate.
 * @param {number} speechPitch The current speech pitch.
 * @param {boolean} wordHighlightingEnabled If word highlighting is enabled.
 * @public
 */
MetricsUtils.recordStartEvent = function(
    method, speechRate, speechPitch, wordHighlightingEnabled) {
  chrome.metricsPrivate.recordUserAction(MetricsUtils.START_SPEECH_METRIC);
  chrome.metricsPrivate.recordSparseValue(
      MetricsUtils.SPEECH_RATE_METRIC,
      MetricsUtils.speechRateToSparceHistogramInt_(speechRate));
  chrome.metricsPrivate.recordSparseValue(
      MetricsUtils.SPEECH_PITCH_METRIC,
      MetricsUtils.speechPitchToSparceHistogramInt_(speechPitch));
  chrome.metricsPrivate.recordBoolean(
      MetricsUtils.WORD_HIGHLIGHTING_METRIC, wordHighlightingEnabled);
  chrome.metricsPrivate.recordEnumerationValue(
      MetricsUtils.START_SPEECH_METHOD_METRIC.METRIC_NAME, method,
      MetricsUtils.START_SPEECH_METHOD_METRIC.EVENT_COUNT);
};

/**
 * Records an event that Select-to-Speak speech has been canceled.
 * @private
 */
MetricsUtils.recordCancelEvent_ = function() {
  chrome.metricsPrivate.recordUserAction(MetricsUtils.CANCEL_SPEECH_METRIC);
};

/**
 * Records a user-requested state change event from a given state.
 * @param {number} changeType
 * @public
 */
MetricsUtils.recordSelectToSpeakStateChangeEvent = function(changeType) {
  chrome.metricsPrivate.recordEnumerationValue(
      MetricsUtils.STATE_CHANGE_METRIC.METRIC_NAME, changeType,
      MetricsUtils.STATE_CHANGE_METRIC.EVENT_COUNT);
};
