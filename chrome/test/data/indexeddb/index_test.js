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
  createIndex(true);
}

function createIndex(expect_error)
{
  debug('Creating an index.');
  result = objectStore.createIndex('myIndex', 'aKey', true);
  if (expect_error) {
    result.onsuccess = unexpectedErrorCallback;
    result.onerror = indexErrorExpected;
  } else {
    result.onsuccess = indexSuccess;
    result.onerror = unexpectedErrorCallback;
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
  objectStore = event.result;
  var myValue = {'aKey': 21, 'aValue': '!42'};
  var result = objectStore.add(myValue, 0);
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
