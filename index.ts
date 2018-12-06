import { param } from './params';

type TestDefinitionCallback = (p: object) => void;
interface Test {
  name: string,
  params: param.ParamIterable,
  callback: TestDefinitionCallback,
}

const kValidTestNames = /[a-zA-Z0-9_]+/;
class TestCollection {
  private tests: Test[];
  constructor() {
    this.tests = [];
  }

  add(name: string, params: param.ParamIterable, callback: TestDefinitionCallback) {
    if (!kValidTestNames.test(name)) {
      throw new Error("name must match " + kValidTestNames);
    }
    this.tests.push({ name, params, callback });
  }

  run() {
    for (const test of this.tests) {
      for (const testcase of test.params) {
        console.log('');
        console.log('****', test.name, testcase);
        test.callback(testcase);
      }
    }
  }
}

// example
(() => {
  const tests = new TestCollection();

  tests.add("hello_world",
      new param.Combiner([
        new param.List('x', [1, 2]),
        new param.List('y', ['a', 'b']),
        new param.Unit(),
      ]),
      (p: object) => {
        console.log(p);
      });
  tests.run();
})();
