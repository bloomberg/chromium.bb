function Primes() {
  this.primes_ = {};
}

Primes.prototype.test = function(p) {
  for (var divisor in this.primes_) {
    if (p % divisor === 0) return false;
    if (divisor * divisor > p)
      break;
  }
  this.primes_[p] = 1;
  return true;
}
