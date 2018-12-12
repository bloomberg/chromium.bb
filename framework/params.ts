export type ParamIterable = Iterable<object>;
export type ParamIterator = IterableIterator<object>;

export function punit() { return new PUnit(); }
class PUnit implements ParamIterable {
  * [Symbol.iterator](): ParamIterator {
    yield {};
  }
}

export function poptions(name: string, values: any[]) { return new POptions(name, values); }
class POptions implements ParamIterable {
  private name: string;
  private values: any[];

  constructor(name: string, values: any[]) {
    this.name = name;
    this.values = values;
  }

  * [Symbol.iterator](): ParamIterator {
    for (const value of this.values) {
      yield {[this.name]: value};
    }
  }
}

export function pcombine(params: ParamIterable[]) { return new PCombine(params); }
export class PCombine implements ParamIterable {
  private params: ParamIterable[];

  constructor(params: ParamIterable[]) {
    this.params = params;
  }

  [Symbol.iterator](): ParamIterator {
    return PCombine.cartesian(this.params);
  }

  private static merge(a: object, b: object): object {
    for (const key of Object.keys(a)) {
      if (b.hasOwnProperty(key)) {
        throw new Error("Duplicate key: " + key);
      }
    }
    return Object.assign({}, a, b);
  }

  private static * cartesian(iters: Iterable<object>[]): ParamIterator {
    if (iters.length === 0) {
      yield* new PUnit();
      return;
    }
    if (iters.length === 1) {
      yield* iters[0];
      return;
    }
    const [as, ...rest] = iters;
    for (let a of as) {
      for (let b of PCombine.cartesian(rest)) {
        yield PCombine.merge(a, b);
      }
    }
  }
}
