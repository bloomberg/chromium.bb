import { TestCollection } from '.';

export interface TestTreeModule {
    default: any,
}

export abstract class TestTree {
  treeName: string;

  constructor(treeName: string) {
    this.treeName = treeName;
    (async () => { console.log(await this.subtrees()); })();
  }

  async subtrees(): Promise<TestTreeModule[]> { return []; }

  async traverse(tests: TestCollection, subtreeName: string) {
    const M = await import('./' + subtreeName);
    console.log(new M.default(this.treeName + '/' + subtreeName));
  }
}
