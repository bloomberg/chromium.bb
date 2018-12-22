import { ParamIterable, ParamIterator } from "./index.js";

export function poptions(name: string, values: any[]) { return new POptions(name, values); }

class POptions implements ParamIterable {
  private name: string;
  private values: any[];

  constructor(name: string, values: any[]) {
    this.name = name;
    this.values = values;
  }

  public * [Symbol.iterator](): ParamIterator {
    for (const value of this.values) {
      yield {[this.name]: value};
    }
  }
}
