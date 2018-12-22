import { ParamIterable, ParamIterator } from "./index.js";

export function punit() { return new PUnit(); }
class PUnit implements ParamIterable {
  public * [Symbol.iterator](): ParamIterator {
    yield {};
  }
}
