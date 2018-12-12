import { TestTree, TestTreeModule } from '../../framework';

export default class extends TestTree {
  async subtrees(): Promise<TestTreeModule[]> {
    return [
      await import('./c'),
    ];
  }
}