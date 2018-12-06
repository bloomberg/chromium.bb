import {
  ParamIterable,
} from './params';

type TestDefinitionCallback = (p: object) => void;
interface Test {
  name: string,
  params: ParamIterable,
  callback: TestDefinitionCallback,
}

const kValidTestNames = /[a-zA-Z0-9_]+/;
export class TestCollection {
  private tests: Test[];
  constructor() {
    this.tests = [];
  }

  add(name: string, params: ParamIterable, callback: TestDefinitionCallback) {
    if (!kValidTestNames.test(name)) {
      throw new Error("name must match " + kValidTestNames);
    }
    this.tests.push({ name, params, callback });
  }

  async run() {
    for (const test of this.tests) {
      for (const testcase of test.params) {
        console.log('');
        console.log('****', test.name, testcase);
        await test.callback(testcase);
      }
    }
  }
}