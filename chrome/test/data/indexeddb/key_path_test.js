debug("Test IndexedDB's KeyPath.");

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
    keyRange = IDBKeyRange.leftBound("myKey" + count);
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

function createObjectStoreSuccess()
{
    objectStore = event.result;
    count = 0;
    populateObjectStore();
}

function openSuccess()
{
    debug("Creating object store");
    var db = event.result;
    result = db.createObjectStore('test', 'keyPath');
    result.onsuccess = createObjectStoreSuccess;
    result.onerror = unexpectedErrorCallback;
}

function test()
{
    debug("Opening IndexedDB");
    result = indexedDB.open('name', 'description');
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

var successfullyParsed = true;
