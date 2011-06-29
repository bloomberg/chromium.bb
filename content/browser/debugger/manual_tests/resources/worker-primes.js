importScripts('primes.js');

var primes = new Primes();

onmessage = function(event) {
  var p = event.data;
  if (p != parseInt(p))
    throw 'invalid argument';
  postMessage([p, primes.test(p)]);
}
