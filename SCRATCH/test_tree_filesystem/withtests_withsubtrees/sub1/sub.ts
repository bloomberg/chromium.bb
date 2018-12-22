export const name = "sub";
export const description = ``;

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
