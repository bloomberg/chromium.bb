// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  request = indexedDB.open('open-close-version-test1');
  request.onsuccess = openTest2;
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
}

function openTest2(event) {
  db = event.target.result;
  debug("Ensure that the existing leveldb files are used. If they are not, " +
        "this script will create a new database that has no object stores");
  shouldBe("db.objectStoreNames.length", "1");
  shouldBeEqualToString("typeof db.version", "string");
  request = indexedDB.open('open-close-version-test2');
  request.onsuccess = done;
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
}
