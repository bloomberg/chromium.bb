import { Logger } from "./logger";
import { ParamIterable } from "./params";

interface ITestTreeModule {
  name: string;
  description: string;
  subtrees?: Array<Promise<ITestTreeModule>>;
  add?: (tree: TestTree) => void;
}

type PTestFn = (log: Logger, param: object) => (Promise<void> | void);
type TestFn = (log: Logger) => (Promise<void> | void);
type Test = () => Promise<void>;
interface ILeaf {
  name: string;
  run: (log: Logger) => Promise<void>;
}

export class TestTree {
  public static async trunk(modules: Array<Promise<ITestTreeModule>>): Promise<TestTree> {
    const trunk = new TestTree("", "");
    for (const m of modules) {
      await trunk.recurse(await m);
    }
    return trunk;
  }

  public name: string;
  public description: string;
  public subtrees: TestTree[];
  public tests: ILeaf[];

  private constructor(name: string, description: string) {
    this.name = name;
    this.description = description.trim();
    this.subtrees = [];
    this.tests = [];
  }

  public ptest(name: string, params: ParamIterable, fn: PTestFn): void {
    this.tests.push({name, run: async (log: Logger) => {
      for (const p of params) {
        log.push(JSON.stringify(p));
        await fn(log, p);
        log.pop();
      }
    }});
  }

  public test(name: string, fn: TestFn): void {
    this.tests.push({name, run: async (log: Logger) => {
      await fn(log);
    }});
  }

  public async * iterate(log: Logger): AsyncIterable<Test> {
    const subtrees = await Promise.all(this.subtrees);
    for (const t of this.tests) {
      log.push(t.name);
      yield () => t.run(log);
      log.pop();
    }
    for (const st of subtrees) {
      log.push(st.name);
      // tslint:disable-next-line:no-console
      yield* st.iterate(log);
      log.pop();
    }
  }

  private async recurse(m: ITestTreeModule): Promise<void> {
    const tt = new TestTree(m.name, m.description);
    if (m.subtrees) {
      for (const st of m.subtrees) {
        await tt.recurse(await st);
      }
    }
    if (m.add) {
      m.add(tt);
    }
    this.subtrees.push(tt);
  }
}
