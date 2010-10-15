function indexErrorExpected()
{
  debug('Existing index triggered on error as expected.');
  // TODO(bulach): do something useful with the index itself. Right now this
  // test only exercises the plumbing.
  done();
}

function indexSuccess()
{
  debug('Index created successfully.');
  try {
    result = objectStore.createIndex('myIndex', 'aKey', true);
    fail('Re-creating an index must throw an exception');
  } catch (e) {
    indexErrorExpected();
  }
}

function createIndex(expect_error)
{
  debug('Creating an index.');
  try {
    result = objectStore.createIndex('myIndex', 'aKey', true);
    indexSuccess();
  } catch (e) {
    unexpectedErrorCallback();
  }
}

function dataAddedSuccess()
{
  debug('Data added');
  createIndex(false);
}

function populateObjectStore()
{
  debug('Populating object store');
  deleteAllObjectStores(db);
  window.objectStore = db.createObjectStore('test');
  var myValue = {'aKey': 21, 'aValue': '!42'};
  var result = objectStore.add(myValue, 0);
  result.onsuccess = dataAddedSuccess;
  result.onerror = unexpectedErrorCallback;
}

function setVersion()
{
  debug('setVersion');
  window.db = event.result;
  var result = db.setVersion('new version');
  result.onsuccess = populateObjectStore;
  result.onerror = unexpectedErrorCallback;
}

function test()
{
  debug('Connecting to indexedDB');
  var result = webkitIndexedDB.open('name', 'description');
  result.onsuccess = setVersion;
  result.onerror = unexpectedErrorCallback;
}
