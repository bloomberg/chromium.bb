export const name = "withtests_withsubtrees";
export const description = `
TestTree test: with tests, with subtrees
`;
export const subtrees = [
  import("./sub1"),
  import("./sub2"),
];

import {
  TestTree,
} from "framework";

export function add(tree: TestTree) {
  tree.test("test1", (log) => {
    log.expect(true);
  });
  tree.test("test2", (log) => {
    log.expect(true);
  });
}
