interface ParamIteratorNext {
  done: boolean,
  value: any,
}
interface ParamIterator {
  next(): ParamIteratorNext
}
interface Params {
  [Symbol.iterator](): ParamIterator
}

class ParamList implements Params {
  private list: any[];
  constructor(list: any[]) {
    this.list = list;
  }
  [Symbol.iterator](): ParamIterator {
    return this.list[Symbol.iterator];
  }
}

class ParamCombiner implements Params {
  private paramses: Params[];
  constructor(paramses: Params[]) {
    this.paramses = paramses;
  }
  * [Symbol.iterator](): ParamIterator {
  }
}

type TestInstantiation = () => void;
type TestDefinitionCallback = (p: object) => void;
const kValidTestNames = /[a-zA-Z0-9 _-]+/;
class TestCollection {
  /*private*/ tests: TestInstantiation[];
  constructor() {
    this.tests = [];
  }
  add(name: string, params: Params, callback: TestDefinitionCallback) {
    if (!kValidTestNames.test(name)) {
      throw new Error("name may contain only [as");
    }
    for (const p of params) {
      this.tests.push(() => callback(p));
    }
  }
}

// example
(() => {
  const tests = new TestCollection();
  tests.add("hello world", , (p) => {
    console.log(p);
  });
  console.log(tests.tests);
})();