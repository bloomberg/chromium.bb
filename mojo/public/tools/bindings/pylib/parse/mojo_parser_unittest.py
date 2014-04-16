# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojo_lexer
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
    source = """\
module my_module {

struct MyStruct {
  int32 a;
  double b;
};

}  // module my_module
"""
    expected = \
[('MODULE',
  'my_module',
  [('STRUCT',
    'MyStruct',
    None,
    [('FIELD', 'int32', 'a', None, None),
     ('FIELD', 'double', 'b', None, None)])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)

  def testSimpleStructWithoutModule(self):
    """Tests a simple struct without an enclosing module."""
    source = """\
struct MyStruct {
  int32 a;
  double b;
};
"""
    expected = \
[('MODULE',
  '',
  [('STRUCT',
    'MyStruct',
    None,
    [('FIELD', 'int32', 'a', None, None),
     ('FIELD', 'double', 'b', None, None)])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)

  def testMissingModuleName(self):
    """Tests an (invalid) .mojom with a missing module name."""
    source1 = """\
// Missing module name.
module {
struct MyStruct {
  int32 a;
};
}
"""
    with self.assertRaisesRegexp(
        mojo_parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '{':\nmodule {$"):
      mojo_parser.Parse(source1, "my_file.mojom")

    # Another similar case, but make sure that line-number tracking/reporting
    # is correct.
    source2 = """\
module
// This line intentionally left unblank.

{
}
"""
    with self.assertRaisesRegexp(
        mojo_parser.ParseError,
        r"^my_file\.mojom:4: Error: Unexpected '{':\n{$"):
      mojo_parser.Parse(source2, "my_file.mojom")

  def testEnumExpressions(self):
    """Tests an enum with values calculated using simple expressions."""
    source = """\
module my_module {

enum MyEnum {
  MY_ENUM_1 = 1,
  MY_ENUM_2 = 1 + 1,
  MY_ENUM_3 = 1 * 3,
  MY_ENUM_4 = 2 * (1 + 1),
  MY_ENUM_5 = 1 + 2 * 2,
  MY_ENUM_6 = -6 / -2,
  MY_ENUM_7 = 3 | (1 << 2),
  MY_ENUM_8 = 16 >> 1,
  MY_ENUM_9 = 1 ^ 15 & 8,
  MY_ENUM_10 = 110 % 100,
  MY_ENUM_MINUS_1 = ~0
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
      ('EXPRESSION', ['1', '+', '2', '*', '2'])),
     ('ENUM_FIELD',
      'MY_ENUM_6',
      ('EXPRESSION',
       ['-', ('EXPRESSION', ['6', '/', '-', ('EXPRESSION', ['2'])])])),
     ('ENUM_FIELD',
      'MY_ENUM_7',
      ('EXPRESSION',
       ['3', '|', '(', ('EXPRESSION', ['1', '<<', '2']), ')'])),
     ('ENUM_FIELD', 'MY_ENUM_8', ('EXPRESSION', ['16', '>>', '1'])),
     ('ENUM_FIELD',
      'MY_ENUM_9',
      ('EXPRESSION', ['1', '^', '15', '&', '8'])),
     ('ENUM_FIELD', 'MY_ENUM_10', ('EXPRESSION', ['110', '%', '100'])),
     ('ENUM_FIELD',
      'MY_ENUM_MINUS_1',
      ('EXPRESSION', ['~', ('EXPRESSION', ['0'])]))])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)

  def testNoConditionals(self):
    """Tests that ?: is not allowed."""
    source = """\
module my_module {

enum MyEnum {
  MY_ENUM_1 = 1 ? 2 : 3
};

}  // my_module
"""
    with self.assertRaisesRegexp(
        mojo_lexer.LexError,
        r"^my_file\.mojom:4: Error: Illegal character '\?'$"):
      mojo_parser.Parse(source, "my_file.mojom")

  def testSimpleOrdinals(self):
    """Tests that (valid) ordinal values are scanned correctly."""
    source = """\
module my_module {

// This isn't actually valid .mojom, but the problem (missing ordinals) should
// be handled at a different level.
struct MyStruct {
  int32 a0 @0;
  int32 a1 @1;
  int32 a2 @2;
  int32 a9 @9;
  int32 a10 @10;
  int32 a11 @11;
  int32 a29 @29;
  int32 a1234567890 @1234567890;
};

}  // module my_module
"""
    expected = \
[('MODULE',
  'my_module',
  [('STRUCT',
    'MyStruct',
    None,
    [('FIELD', 'int32', 'a0', '@0', None),
     ('FIELD', 'int32', 'a1', '@1', None),
     ('FIELD', 'int32', 'a2', '@2', None),
     ('FIELD', 'int32', 'a9', '@9', None),
     ('FIELD', 'int32', 'a10', '@10', None),
     ('FIELD', 'int32', 'a11', '@11', None),
     ('FIELD', 'int32', 'a29', '@29', None),
     ('FIELD', 'int32', 'a1234567890', '@1234567890', None)])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidOrdinals(self):
    """Tests that (lexically) invalid ordinals are correctly detected."""
    source1 = """\
module my_module {

struct MyStruct {
  int32 a_missing @;
};

}  // module my_module
"""
    with self.assertRaisesRegexp(
        mojo_lexer.LexError,
        r"^my_file\.mojom:4: Error: Missing ordinal value$"):
      mojo_parser.Parse(source1, "my_file.mojom")

    source2 = """\
module my_module {

struct MyStruct {
  int32 a_octal @01;
};

}  // module my_module
"""
    with self.assertRaisesRegexp(
        mojo_lexer.LexError,
        r"^my_file\.mojom:4: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      mojo_parser.Parse(source2, "my_file.mojom")

    source3 = """\
module my_module { struct MyStruct { int32 a_invalid_octal @08; }; }
"""
    with self.assertRaisesRegexp(
        mojo_lexer.LexError,
        r"^my_file\.mojom:1: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      mojo_parser.Parse(source3, "my_file.mojom")

    source4 = """\
module my_module { struct MyStruct { int32 a_hex @0x1aB9; }; }
"""
    with self.assertRaisesRegexp(
        mojo_lexer.LexError,
        r"^my_file\.mojom:1: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      mojo_parser.Parse(source4, "my_file.mojom")

    source5 = """\
module my_module { struct MyStruct { int32 a_hex @0X0; }; }
"""
    with self.assertRaisesRegexp(
        mojo_lexer.LexError,
        r"^my_file\.mojom:1: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      mojo_parser.Parse(source5, "my_file.mojom")

  def testNestedNamespace(self):
    """Tests nested namespaces work."""
    source = """\
module my.mod {

struct MyStruct {
  int32 a;
};

}  // module my.mod
"""
    expected = \
[('MODULE',
  'my.mod',
  [('STRUCT',
    'MyStruct',
    None,
    [('FIELD', 'int32', 'a', None, None)])])]
    self.assertEquals(mojo_parser.Parse(source, "my_file.mojom"), expected)


if __name__ == "__main__":
  unittest.main()
