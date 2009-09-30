function d(arg) {
  if (arg) {
    debugger;
  }
  var y = fact(10);
  return y;
}

function fact(n) {
  var r = 1;
  while (n > 1) {
    r *= n;
    --n;
  }
  return r;
}
