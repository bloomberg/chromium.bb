// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function afterCommit()
{
    try {
        debug("Accessing a committed transaction should throw");
        var store = transaction.objectStore('storeName');
    } catch (e) {
        exc = e;
        shouldBe('exc.code', 'webkitIDBDatabaseException.NOT_ALLOWED_ERR');
    }
    done();
}

function nonExistingKey()
{
    shouldBe("event.result", "undefined");
    window.setTimeout('afterCommit()', 0);
}

function gotValue()
{
    value = event.result;
    shouldBeEqualToString('value', 'myValue');
}

function startTransaction()
{
    debug("Using get in a transaction");
    transaction = db.transaction();
    //transaction.onabort = unexpectedErrorCallback;
    store = transaction.objectStore('storeName');
    shouldBeEqualToString("store.name", "storeName");
    result = store.get('myKey');
    result.onsuccess = gotValue;
    result.onerror = unexpectedErrorCallback;

    var emptyResult = store.get('nonExistingKey');
    emptyResult.onsuccess = nonExistingKey;
    emptyResult.onerror = unexpectedErrorCallback;
}

function populateObjectStore()
{
    deleteAllObjectStores(db);
    window.objectStore = db.createObjectStore('storeName');
    var result = objectStore.add('myValue', 'myKey');
    result.onsuccess = startTransaction;
    result.onerror = unexpectedErrorCallback;
}

function setVersion()
{
    window.db = event.result;
    var result = db.setVersion('new version');
    result.onsuccess = populateObjectStore;
    result.onerror = unexpectedErrorCallback;
}

function test()
{
    result = webkitIndexedDB.open('name');
    result.onsuccess = setVersion;
    result.onerror = unexpectedErrorCallback;
}
