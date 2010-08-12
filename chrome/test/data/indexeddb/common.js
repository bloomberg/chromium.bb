function debug(message)
{
  document.getElementById('status').innerHTML += '<br/>' + message;
}

function pass()
{
  debug('PASS');
  document.location = '#pass';
}

function fail(message)
{
  debug(message);
  document.location = '#fail';
}

function getLog()
{
  return "" + document.getElementById('status').innerHTML;
}

function unexpectedErrorCallback()
{
  fail('unexpectedErrorCallback');
}

