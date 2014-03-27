# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojo_parser
import unittest


class MojoParserTest(unittest.TestCase):
  """Tests mojo_parser (in particular, Parse())."""

  def testTrivialValidSource(self):
    """Tests a trivial, but valid, .mojom source."""
    source = """\
// This is a comment.

module my_module {
}
"""
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"),
                      [("MODULE", "my_module", None)])

  def testSourceWithCrLfs(self):
    """Tests a .mojom source with CR-LFs instead of LFs."""
    source = "// This is a comment.\r\n\r\nmodule my_module {\r\n}\r\n";
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"),
                      [("MODULE", "my_module", None)])

  def testUnexpectedEOF(self):
    """Tests a "truncated" .mojom source."""
    source = """\
// This is a comment.

module my_module {
"""
    with self.assertRaisesRegexp(
        mojo_parser.ParseError,
        r"^my_file\.mojom: Error: Unexpected end of file$"):
      mojo_parser.Parse(source, "my_file.mojom")

  def testSimpleStruct(self):
    """Tests a simple .mojom source that just defines a struct."""
    source ="""\
module my_module {

struct MyStruct {
  int32 a;
  double b;
};

}  // module my_module
"""
    # Note: Output as pretty-printed on failure by the test harness.
    expected = \
[('MODULE',
  'my_module',
  [('STRUCT',
    'MyStruct',
    None,
    [('FIELD', 'int32', 'a', None, None),
     ('FIELD', 'double', 'b', None, None)])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)

  def testEnumExpressions(self):
    """Tests an enum with values calculated using simple expressions."""
    source = """\
module my_module {

enum MyEnum {
  MY_ENUM_1 = 1,
  MY_ENUM_2 = 1 + 1,
  MY_ENUM_3 = 1 * 3,
  MY_ENUM_4 = 2 * (1 + 1),
  MY_ENUM_5 = 1 + 2 * 2
};

}  // my_module
"""
    expected = \
[('MODULE',
  'my_module',
  [('ENUM',
    'MyEnum',
    [('ENUM_FIELD', 'MY_ENUM_1', ('EXPRESSION', ['1'])),
     ('ENUM_FIELD', 'MY_ENUM_2', ('EXPRESSION', ['1', '+', '1'])),
     ('ENUM_FIELD', 'MY_ENUM_3', ('EXPRESSION', ['1', '*', '3'])),
     ('ENUM_FIELD',
      'MY_ENUM_4',
      ('EXPRESSION',
       ['2', '*', '(', ('EXPRESSION', ['1', '+', '1']), ')'])),
     ('ENUM_FIELD',
      'MY_ENUM_5',
      ('EXPRESSION', ['1', '+', '2', '*', '2']))])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)


if __name__ == "__main__":
  unittest.main()
