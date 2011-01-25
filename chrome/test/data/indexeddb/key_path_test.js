// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function cursorSuccess()
{
    debug("Cursor opened successfully.")
    // FIXME: check that we can iterate the cursor.
    shouldBe("event.result.direction", "0");
    shouldBe("event.result.key", "'myKey' + count");
    shouldBe("event.result.value.keyPath", "'myKey' + count");
    shouldBe("event.result.value.value", "'myValue' + count");
    if (++count >= 5)
        done();
    else
        openCursor();
}

function openCursor()
{
    debug("Opening cursor #" + count);
    keyRange = webkitIDBKeyRange.lowerBound("myKey" + count);
    result = objectStore.openCursor(keyRange);
    result.onsuccess = cursorSuccess;
    result.onerror = unexpectedErrorCallback;
}

function populateObjectStore()
{
    debug("Populating object store #" + count);
    obj = {'keyPath': 'myKey' + count, 'value': 'myValue' + count};
    result = objectStore.add(obj);
    result.onerror = unexpectedErrorCallback;
    if (++count >= 5) {
        count = 0;
        result.onsuccess = openCursor;
    } else {
        result.onsuccess = populateObjectStore;
    }
}

function createObjectStore()
{
    debug('createObjectStore');
    deleteAllObjectStores(db);
    window.objectStore = db.createObjectStore('test', {keyPath: 'keyPath'});
    count = 0;
    populateObjectStore();
}

function setVersion()
{
    debug('setVersion');
    window.db = event.result;
    var result = db.setVersion('new version');
    result.onsuccess = createObjectStore;
    result.onerror = unexpectedErrorCallback;
}

function test()
{
    debug("Test IndexedDB's KeyPath.");
    debug("Opening IndexedDB");
    result = webkitIndexedDB.open('name');
    result.onsuccess = setVersion;
    result.onerror = unexpectedErrorCallback;
}

var successfullyParsed = true;
