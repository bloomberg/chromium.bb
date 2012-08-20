// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  if (document.location.hash === '#tab1') {
    prepareDatabase(0, function () { doSetVersion(1); });
  } else if (document.location.hash === '#tab2') {
    doSetVersion(2);
  } else {
    result('fail - unexpected hash');
  }
}

function prepareDatabase(version, callback)
{
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

function doSetVersion(version)
{
  // Open the database and try a setVersion
  var openreq = window.indexedDB.open('version-change-blocked');
  openreq.onerror = unexpectedErrorCallback;
  openreq.onsuccess = function(e) {
    window.db = openreq.result;
    var setverreq = window.db.setVersion(String(version));
    setverreq.onerror = unexpectedErrorCallback;
    setverreq.onblocked = function(e) {
      result('setVersion(' + version + ') blocked');
    };
    setverreq.onsuccess = function(e) {
      result('setVersion(' + version + ') complete');
    };
  };
}
