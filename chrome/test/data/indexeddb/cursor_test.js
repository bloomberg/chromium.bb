function emptyCursorSuccess()
{
  debug('Empty cursor opened successfully.')
  // TODO(bulach): check that we can iterate the cursor.
  done();
}

function openEmptyCursor()
{
  debug('Opening an empty cursor.');
  keyRange = IDBKeyRange.leftBound('InexistentKey');
  result = objectStore.openCursor(keyRange);
  result.onsuccess = emptyCursorSuccess;
  result.onerror = unexpectedErrorCallback;
}

function cursorSuccess()
{
  debug('Cursor opened successfully.');
  shouldBe("event.result.direction", "0");
  shouldBe("event.result.key", "'myKey'");
  shouldBe("event.result.value", "'myValue'");
  openEmptyCursor();
}

function openCursor(objectStore)
{
  debug('Opening cursor');
  var keyRange = IDBKeyRange.leftBound('myKey');
  var result = objectStore.openCursor(keyRange);
  result.onsuccess = cursorSuccess;
  result.onerror = unexpectedErrorCallback;
}

function dataAddedSuccess()
{
  debug('Data added');
  openCursor(objectStore);
}

function populateObjectStore()
{
  debug('Populating object store');
  objectStore = event.result;
  var result = objectStore.add('myValue', 'myKey');
  result.onsuccess = dataAddedSuccess();
  result.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
  debug('Creating object store');
  var db = event.result;
  var result = db.createObjectStore('test');
  result.onsuccess = populateObjectStore;
  result.onerror = unexpectedErrorCallback;
}

function test()
{
  debug('Connecting to indexedDB');
  var result = indexedDB.open('name', 'description');
  result.onsuccess = openSuccess;
  result.onerror = unexpectedErrorCallback;
}
