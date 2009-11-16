function fib(n) {
  return n < 2 ? 1 : fib(n - 1) + fib(n - 2);
}

function eternal_fib() {
  var started = Date.now();
  while(true) {
    fib(20);
    // Make page responsive by making a break every 100 ms.
    if (Date.now() - started >= 100) {
      setTimeout(eternal_fib, 0);
      return;
    }
  }
}

function run_fib() {
  // Let the page do initial rendering, then go.
  setTimeout(eternal_fib, 200);
}
