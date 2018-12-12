import {
  TestCollection,
  TestTree,
  TestTreeModule,
} from '../framework';

class TestTreeRoot extends TestTree {
  async subtrees(): Promise<TestTreeModule[]> {
    return [
      await import('./b'),
    ];
  }
}

(async () => {
  const tests = new TestCollection();
  const root = new TestTreeRoot('webgpu');
})();