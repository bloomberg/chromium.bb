function afterCommit()
{
    try {
        debug("Accessing a committed transaction should throw");
        var store = transaction.objectStore('storeName');
    } catch (e) {
        exc = e;
        shouldBe('exc.code', '9');
    }
    done();
}

function nonExistingKey()
{
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
    emptyResult.onsuccess = unexpectedSuccessCallback;
    emptyResult.onerror = nonExistingKey;
}

function populateObjectStore(objectStore)
{
    result = objectStore.add('myValue', 'myKey');
    result.onsuccess = startTransaction;
}

function createObjectStoreSuccess()
{
    var objectStore = event.result;
    populateObjectStore(objectStore);
}

function openSuccess()
{
    db = event.result;

    deleteAllObjectStores(db);

    result = db.createObjectStore('storeName');
    result.onsuccess = createObjectStoreSuccess;
    result.onerror = unexpectedErrorCallback;
}

function test()
{
    result = indexedDB.open('name', 'description');
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

test();
