export const name = "test_tree_filesystem";
export const description = `
Unit tests for TestTree's filesystem structure.
`;

import {
  StringLogger,
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("all", async (log) => {
    const mytree = await TestTree.trunk([
      import("./notests_nosubtrees"),
      import("./withtests_nosubtrees"),
      import("./withtests_withsubtrees"),
    ]);

    const mylog = new StringLogger();
    for await (const t of mytree.iterate(mylog)) {
      await t();
    }
    log.log(mylog.getContents());
    log.expect(mylog.getContents() === `\
* notests_nosubtrees
* withtests_nosubtrees
  * test1
    | PASS
  * test2
    | PASS
* withtests_withsubtrees
  * test1
    | PASS
  * test2
    | PASS
  * sub1
    * test1
      | PASS
    * test2
      | PASS
    * sub
      * test1
        | PASS
      * test2
        | PASS
  * sub2
    * test1
      | PASS
    * test2
      | PASS
    * sub
      * test1
        | PASS
      * test2
        | PASS`);
  });
}
