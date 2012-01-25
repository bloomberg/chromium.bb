// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.indexedDB = window.indexedDB || window.webkitIndexedDB;

function unexpectedErrorCallback()
{
  document.title = 'fail - unexpected error callback';
}
function unexpectedAbortCallback()
{
  document.title = 'fail - unexpected abort callback';
}
function unexpectedCompleteCallback()
{
  document.title = 'fail - unexpected complete callback';
}

function test() {
  if (document.location.hash === '#tab1') {
    prepareDatabase(0, function () { doSetVersion(1); });
  } else if (document.location.hash === '#tab2') {
    doSetVersion(2);
  } else {
    document.title = 'fail';
  }
}

function prepareDatabase(version, callback) {
  // Prepare the database, then exit normally

  var delreq = window.indexedDB.deleteDatabase('version-change-blocked');
  delreq.onerror = unexpectedErrorCallback;
  delreq.onsuccess = function() {
    var openreq = window.indexedDB.open('version-change-blocked');
    openreq.onerror = unexpectedErrorCallback;
    openreq.onsuccess = function(e) {
      var db = openreq.result;
      var setverreq = db.setVersion(String(version));
      setverreq.onerror = unexpectedErrorCallback;
      setverreq.onsuccess = function(e) {
        var transaction = setverreq.result;
        transaction.onabort = unexpectedAbortCallback;
        transaction.oncomplete = function (e) {
          db.close();
          callback();
        };
      };
    };
  };
}

function doSetVersion(version) {
  // Open the database and try a setVersion
  var openreq = window.indexedDB.open('version-change-blocked');
  openreq.onerror = unexpectedErrorCallback;
  openreq.onsuccess = function(e) {
    window.db = openreq.result;
    var setverreq = window.db.setVersion(String(version));
    setverreq.onerror = unexpectedErrorCallback;
    setverreq.onblocked = function(e) {
      document.title = 'setVersion(' + version + ') blocked';
    };
    setverreq.onsuccess = function(e) {
      document.title = 'setVersion(' + version + ') complete';
    };
  };
}
