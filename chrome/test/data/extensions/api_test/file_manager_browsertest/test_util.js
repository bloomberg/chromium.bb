// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Sends a test message.
 * @param {Object} message Message to be sent. It is converted into JSON string
 *     before sending.
 * @return {Promise} Promise to be fulfilled with a returned value.
 */
function sendTestMessage(message) {
  return new Promise(function(fulfill) {
    chrome.test.sendMessage(JSON.stringify(message), fulfill);
  });
}

/**
 * Returns promise to be fulfilled after the given milliseconds.
 * @param {number} time Time in milliseconds.
 */
function wait(time) {
  return new Promise(function(callback) {
    setTimeout(callback, time);
  });
}

/**
 * Interval milliseconds between checks of repeatUntil.
 * @type {number}
 * @const
 */
var REPEAT_UNTIL_INTERVAL = 200;

/**
 * Interval milliseconds between log output of repeatUntil.
 * @type {number}
 * @const
 */
var LOG_INTERVAL = 3000;

/**
 * Returns a pending marker. See also the repeatUntil function.
 * @param {string} message Pending reason including %s, %d, or %j markers. %j
 *     format an object as JSON.
 * @param {Array.<*>} var_args Values to be assigined to %x markers.
 * @return {Object} Object which returns true for the expression: obj instanceof
 *     pending.
 */
function pending(message, var_args) {
  var index = 1;
  var args = arguments;
  var formattedMessage = message.replace(/%[sdj]/g, function(pattern) {
    var arg = args[index++];
    switch(pattern) {
      case '%s': return String(arg);
      case '%d': return Number(arg);
      case '%j': return JSON.stringify(arg);
      default: return pattern;
    }
  });
  var pendingMarker = Object.create(pending.prototype);
  pendingMarker.message = formattedMessage;
  return pendingMarker;
};

/**
 * Waits until the checkFunction returns a value but a pending marker.
 * @param {function():*} checkFunction Function to check a condition. It can
 *     return a pending marker created by a pending function.
 * @return {Promise} Promise to be fulfilled with the return value of
 *     checkFunction when the checkFunction reutrns a value but a pending
 *     marker.
 */
function repeatUntil(checkFunction) {
  var logTime = Date.now() + LOG_INTERVAL;
  var step = function() {
    return Promise.resolve(checkFunction()).then(function(result) {
      if (result instanceof pending) {
        if (Date.now() > logTime) {
          console.log(result.message);
          logTime += LOG_INTERVAL;
        }
        return wait(REPEAT_UNTIL_INTERVAL).then(step);
      } else {
        return result;
      }
    });
  };
  return step();
};

/**
 * Adds the givin entries to the target volume(s).
 * @param {Array.<string>} volumeNames Names of target volumes.
 * @param {Array.<TestEntryInfo>} entries List of entries to be added.
 * @param {function(boolean)=} opt_callback Callback function to be passed the
 *     result of function. The argument is true on success.
 * @return {Promise} Promise to be fulfilled when the entries are added.
 */
function addEntries(volumeNames, entries, opt_callback) {
  if (volumeNames.length == 0) {
    callback(true);
    return;
  }
  var volumeResultPromises = volumeNames.map(function(volume) {
    return sendTestMessage({
      name: 'addEntries',
      volume: volume,
      entries: entries
    }).then(function(result) {
      if (result !== "onEntryAdded")
        return Promise.reject('Failed to add entries to ' + volume + '.');
    });
  });
  var resultPromise = Promise.all(volumeResultPromises);
  if (opt_callback) {
    resultPromise.then(opt_callback.bind(null, true),
                       opt_callback.bind(null, false));
  }
  return resultPromise;
};

/**
 * @enum {string}
 * @const
 */
var EntryType = Object.freeze({
  FILE: 'file',
  DIRECTORY: 'directory'
});

/**
 * @enum {string}
 * @const
 */
var SharedOption = Object.freeze({
  NONE: 'none',
  SHARED: 'shared'
});

/**
 * @enum {string}
 */
var RootPath = Object.seal({
  DOWNLOADS: '/must-be-filled-in-test-setup',
  DRIVE: '/must-be-filled-in-test-setup',
});

/**
 * File system entry information for tests.
 *
 * @param {EntryType} type Entry type.
 * @param {string} sourceFileName Source file name that provides file contents.
 * @param {string} targetName Name of entry on the test file system.
 * @param {string} mimeType Mime type.
 * @param {SharedOption} sharedOption Shared option.
 * @param {string} lastModifiedTime Last modified time as a text to be shown in
 *     the last modified column.
 * @param {string} nameText File name to be shown in the name column.
 * @param {string} sizeText Size text to be shown in the size column.
 * @param {string} typeText Type name to be shown in the type column.
 * @constructor
 */
function TestEntryInfo(type,
                       sourceFileName,
                       targetPath,
                       mimeType,
                       sharedOption,
                       lastModifiedTime,
                       nameText,
                       sizeText,
                       typeText) {
  this.type = type;
  this.sourceFileName = sourceFileName || '';
  this.targetPath = targetPath;
  this.mimeType = mimeType || '';
  this.sharedOption = sharedOption;
  this.lastModifiedTime = lastModifiedTime;
  this.nameText = nameText;
  this.sizeText = sizeText;
  this.typeText = typeText;
  Object.freeze(this);
};

TestEntryInfo.getExpectedRows = function(entries) {
  return entries.map(function(entry) { return entry.getExpectedRow(); });
};

/**
 * Obtains a expected row contents of the file in the file list.
 */
TestEntryInfo.prototype.getExpectedRow = function() {
  return [this.nameText, this.sizeText, this.typeText, this.lastModifiedTime];
};

/**
 * Filesystem entries used by the test cases.
 * @type {Object.<string, TestEntryInfo>}
 * @const
 */
var ENTRIES = {
  hello: new TestEntryInfo(
      EntryType.FILE, 'text.txt', 'hello.txt',
      'text/plain', SharedOption.NONE, 'Sep 4, 1998, 12:34 PM',
      'hello.txt', '51 bytes', 'Plain text'),

  world: new TestEntryInfo(
      EntryType.FILE, 'video.ogv', 'world.ogv',
      'text/plain', SharedOption.NONE, 'Jul 4, 2012, 10:35 AM',
      'world.ogv', '59 KB', 'OGG video'),

  unsupported: new TestEntryInfo(
      EntryType.FILE, 'random.bin', 'unsupported.foo',
      'application/x-foo', SharedOption.NONE, 'Jul 4, 2012, 10:36 AM',
      'unsupported.foo', '8 KB', 'FOO file'),

  desktop: new TestEntryInfo(
      EntryType.FILE, 'image.png', 'My Desktop Background.png',
      'text/plain', SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'My Desktop Background.png', '272 bytes', 'PNG image'),

  image2: new TestEntryInfo(
      EntryType.FILE, 'image2.png', 'image2.png',
      'image/png', SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'image2.png', '272 bytes', 'PNG image'),

  image3: new TestEntryInfo(
      EntryType.FILE, 'image3.jpg', 'image3.jpg',
      'image/jpg', SharedOption.NONE, 'Jan 18, 2038, 1:02 AM',
      'image3.jpg', '272 bytes', 'JPEG image'),

  beautiful: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'Beautiful Song.ogg',
      'text/plain', SharedOption.NONE, 'Nov 12, 2086, 12:00 PM',
      'Beautiful Song.ogg', '14 KB', 'OGG audio'),

  photos: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'photos',
      null, SharedOption.NONE, 'Jan 1, 1980, 11:59 PM',
      'photos', '--', 'Folder'),

  testDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Document',
      'application/vnd.google-apps.document',
      SharedOption.NONE, 'Apr 10, 2013, 4:20 PM',
      'Test Document.gdoc', '--', 'Google document'),

  testSharedDocument: new TestEntryInfo(
      EntryType.FILE, null, 'Test Shared Document',
      'application/vnd.google-apps.document',
      SharedOption.SHARED, 'Mar 20, 2013, 10:40 PM',
      'Test Shared Document.gdoc', '--', 'Google document'),

  newlyAdded: new TestEntryInfo(
      EntryType.FILE, 'music.ogg', 'newly added file.ogg',
      'audio/ogg', SharedOption.NONE, 'Sep 4, 1998, 12:00 AM',
      'newly added file.ogg', '14 KB', 'OGG audio'),

  directoryA: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'A', '--', 'Folder'),

  directoryB: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'B', '--', 'Folder'),

  directoryC: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'A/B/C',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'C', '--', 'Folder'),

  directoryD: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'D',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'D', '--', 'Folder'),

  directoryE: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'D/E',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'E', '--', 'Folder'),

  directoryF: new TestEntryInfo(
      EntryType.DIRECTORY, null, 'D/E/F',
      null, SharedOption.NONE, 'Jan 1, 2000, 1:00 AM',
      'F', '--', 'Folder'),

  zipArchive: new TestEntryInfo(
      EntryType.FILE, 'archive.zip', 'archive.zip',
      'application/x-zip', SharedOption.NONE, 'Jan 1, 2014, 1:00 AM',
      'archive.zip', '533 bytes', 'Zip archive')
};
