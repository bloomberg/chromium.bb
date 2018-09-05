// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox log page.
 *
 */

goog.provide('LogPage');

goog.require('LogStore');
goog.require('TreeLog');
goog.require('Msgs');

/**
 * Class to manage the log page.
 * @constructor
 */
LogPage = function() {};

/**
 * The Background object.
 * @type {Window}
 */
LogPage.backgroundWindow;

/**
 * The LogStore object.
 * @type {LogStore}
 */
LogPage.LogStore;

LogPage.init = function() {
  LogPage.backgroundWindow = chrome.extension.getBackgroundPage();
  LogPage.LogStore = LogPage.backgroundWindow.LogStore.getInstance();

  var clearLogButton = document.getElementById('clearLog');
  clearLogButton.onclick = function(event) {
    LogPage.LogStore.clearLog();
    location.reload();
  };

  var saveLogButton = document.getElementById('saveLog');
  saveLogButton.onclick = LogPage.saveLogEvent;

  var checkboxes = document.getElementsByClassName('log-filter');
  var filterEventListener = function(event) {
    var target = event.target;
    sessionStorage.setItem(target.name, target.checked);
    location.reload();
  };
  for (var i = 0; i < checkboxes.length; i++)
    checkboxes[i].onclick = filterEventListener;

  LogPage.update();
};

/**
 * When saveLog button is clicked this function runs.
 * Save the current log appeared in the page as a plain text.
 * @param {Event} event
 */
LogPage.saveLogEvent = function(event) {
  var outputText = '';
  var logs = document.querySelectorAll('#logList p');
  for (var i = 0; i < logs.length; i++) {
    var logText = [];
    logText.push(logs[i].querySelector('.log-type-tag').textContent);
    logText.push(logs[i].querySelector('.log-time-tag').textContent);
    logText.push(logs[i].querySelector('.log-text').textContent);
    outputText += logText.join(' ') + '\n';
  }

  var a = document.createElement('a');
  var date = new Date();
  a.download =
      [
        'chromevox_logpage', date.getMonth() + 1, date.getDate(),
        date.getHours(), date.getMinutes(), date.getSeconds()
      ].join('_') +
      '.txt';
  a.href = 'data:text/plain; charset=utf-8,' + encodeURI(outputText);
  a.click();
};

/**
 * Update the states of checkboxes and
 * update logs.
 */
LogPage.update = function() {
  var logTypes = LogStore.logTypeStr();
  for (var i = 0; i < logTypes.length; i++) {
    var typeFilter = logTypes[i] + 'Filter';
    var element = document.getElementById(typeFilter);
    /** If sessionStorage is null, set true. */
    if (!sessionStorage.getItem(typeFilter))
      sessionStorage.setItem(typeFilter, true);
    element.checked = (sessionStorage.getItem(typeFilter) == 'true');
  }

  var log = LogPage.LogStore.getLogs();
  LogPage.updateLog(log, document.getElementById('logList'));
};

/**
 * Updates the log section.
 * @param {Array<BaseLog>} log Array of speech.
 * @param {Element} div
 */
LogPage.updateLog = function(log, div) {
  for (var i = 0; i < log.length; i++) {
    if (sessionStorage.getItem(log[i].logType + 'Filter') != 'true')
      continue;

    var p = document.createElement('p');
    var typeName = document.createElement('span');
    typeName.textContent = log[i].logType;
    typeName.className = 'log-type-tag';
    var timeStamp = document.createElement('span');
    timeStamp.textContent = LogPage.formatTimeStamp(log[i].date);
    timeStamp.className = 'log-time-tag';
    var textWrapper = document.createElement('pre');
    textWrapper.textContent = log[i].toString();
    textWrapper.className = 'log-text';

    p.appendChild(typeName);
    p.appendChild(timeStamp);
    p.appendChild(textWrapper);
    div.appendChild(p);
  }
};

/**
 * Format time stamp.
 * In this log, events are dispatched many times in a short time, so
 * milliseconds order time stamp is required.
 * @param {!Date} date
 * @return {!string}
 */
LogPage.formatTimeStamp = function(date) {
  var time = date.getTime();
  time -= date.getTimezoneOffset() * 1000 * 60;
  var timeStr = ('00' + Math.floor(time / 1000 / 60 / 60) % 24).slice(-2) + ':';
  timeStr += ('00' + Math.floor(time / 1000 / 60) % 60).slice(-2) + ':';
  timeStr += ('00' + Math.floor(time / 1000) % 60).slice(-2) + '.';
  timeStr += ('000' + time % 1000).slice(-3);
  return timeStr;
};

document.addEventListener('DOMContentLoaded', function() {
  LogPage.init();
}, false);
