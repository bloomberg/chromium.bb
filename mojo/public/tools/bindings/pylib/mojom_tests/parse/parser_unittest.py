# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import os.path
import sys
import unittest

# Disable lint check for finding modules:
# pylint: disable=F0401

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path

try:
  imp.find_module("mojom")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove("pylib"), "pylib"))
import mojom.parse.ast as ast
import mojom.parse.lexer as lexer
import mojom.parse.parser as parser


class ParserTest(unittest.TestCase):
  """Tests |parser.Parse()|."""

  def testTrivialValidSource(self):
    """Tests a trivial, but valid, .mojom source."""

    source = """\
        // This is a comment.

        module my_module {
        }
        """
    expected = [('MODULE', ('IDENTIFIER', 'my_module'), None, None)]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testSourceWithCrLfs(self):
    """Tests a .mojom source with CR-LFs instead of LFs."""

    source = "// This is a comment.\r\n\r\nmodule my_module {\r\n}\r\n"
    expected = [('MODULE', ('IDENTIFIER', 'my_module'), None, None)]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testUnexpectedEOF(self):
    """Tests a "truncated" .mojom source."""

    source = """\
        // This is a comment.

        module my_module {
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom: Error: Unexpected end of file$"):
      parser.Parse(source, "my_file.mojom")

  def testCommentLineNumbers(self):
    """Tests that line numbers are correctly tracked when comments are
    present."""

    source1 = """\
        // Isolated C++-style comments.

        // Foo.
        asdf1
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:4: Error: Unexpected 'asdf1':\n *asdf1$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        // Consecutive C++-style comments.
        // Foo.
          // Bar.

        struct Yada {  // Baz.
        // Quux.
          int32 x;
        };

        asdf2
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:10: Error: Unexpected 'asdf2':\n *asdf2$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        /* Single-line C-style comments. */
        /* Foobar. */

        /* Baz. */
        asdf3
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:5: Error: Unexpected 'asdf3':\n *asdf3$"):
      parser.Parse(source3, "my_file.mojom")

    source4 = """\
        /* Multi-line C-style comments.
        */
        /*
        Foo.
        Bar.
        */

        /* Baz
           Quux. */
        asdf4
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:10: Error: Unexpected 'asdf4':\n *asdf4$"):
      parser.Parse(source4, "my_file.mojom")


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
          ('IDENTIFIER', 'my_module'),
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int32', 'a', ast.Ordinal(None), None),
             ('FIELD', 'double', 'b', ast.Ordinal(None), None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

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
          None,
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int32', 'a', ast.Ordinal(None), None),
             ('FIELD', 'double', 'b', ast.Ordinal(None), None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

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
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '{':\n *module {$"):
      parser.Parse(source1, "my_file.mojom")

    # Another similar case, but make sure that line-number tracking/reporting
    # is correct.
    source2 = """\
        module
        // This line intentionally left unblank.

        {
        }
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:4: Error: Unexpected '{':\n *{$"):
      parser.Parse(source2, "my_file.mojom")

  def testEnums(self):
    """Tests that enum statements are correctly parsed."""

    source = """\
        module my_module {
        enum MyEnum1 { VALUE1, VALUE2 };  // No trailing comma.
        enum MyEnum2 {
          VALUE1 = -1,
          VALUE2 = 0,
          VALUE3 = + 987,  // Check that space is allowed.
          VALUE4 = 0xAF12,
          VALUE5 = -0x09bcd,
          VALUE6 = VALUE5,
          VALUE7,  // Leave trailing comma.
        };
        }  // my_module
        """
    expected = \
        [('MODULE',
          ('IDENTIFIER', 'my_module'),
          None,
          [('ENUM',
            'MyEnum1',
            [('ENUM_VALUE', 'VALUE1', None),
             ('ENUM_VALUE', 'VALUE2', None)]),
           ('ENUM',
            'MyEnum2',
            [('ENUM_VALUE', 'VALUE1', '-1'),
             ('ENUM_VALUE', 'VALUE2', '0'),
             ('ENUM_VALUE', 'VALUE3', '+987'),
             ('ENUM_VALUE', 'VALUE4', '0xAF12'),
             ('ENUM_VALUE', 'VALUE5', '-0x09bcd'),
             ('ENUM_VALUE', 'VALUE6', ('IDENTIFIER', 'VALUE5')),
             ('ENUM_VALUE', 'VALUE7', None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidEnumInitializers(self):
    """Tests that invalid enum initializers are correctly detected."""

    # No values.
    source1 = """\
        enum MyEnum {
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '}':\n"
            r" *};$"):
      parser.Parse(source1, "my_file.mojom")

    # Floating point value.
    source2 = "enum MyEnum { VALUE = 0.123 };"
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:1: Error: Unexpected '0\.123':\n"
            r"enum MyEnum { VALUE = 0\.123 };$"):
      parser.Parse(source2, "my_file.mojom")

    # Boolean value.
    source2 = "enum MyEnum { VALUE = true };"
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:1: Error: Unexpected 'true':\n"
            r"enum MyEnum { VALUE = true };$"):
      parser.Parse(source2, "my_file.mojom")

  def testConsts(self):
    """Tests some constants and struct members initialized with them."""

    source = """\
        module my_module {

        struct MyStruct {
          const int8 kNumber = -1;
          int8 number@0 = kNumber;
        };

        }  // my_module
        """
    expected = \
        [('MODULE',
          ('IDENTIFIER', 'my_module'),
          None,
          [('STRUCT',
            'MyStruct', None,
            [('CONST', 'int8', 'kNumber', '-1'),
             ('FIELD', 'int8', 'number',
              ast.Ordinal(0), ('IDENTIFIER', 'kNumber'))])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

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
        lexer.LexError,
        r"^my_file\.mojom:4: Error: Illegal character '\?'$"):
      parser.Parse(source, "my_file.mojom")

  def testSimpleOrdinals(self):
    """Tests that (valid) ordinal values are scanned correctly."""

    source = """\
        module my_module {

        // This isn't actually valid .mojom, but the problem (missing ordinals)
        // should be handled at a different level.
        struct MyStruct {
          int32 a0@0;
          int32 a1@1;
          int32 a2@2;
          int32 a9@9;
          int32 a10 @10;
          int32 a11 @11;
          int32 a29 @29;
          int32 a1234567890 @1234567890;
        };

        }  // module my_module
        """
    expected = \
        [('MODULE',
          ('IDENTIFIER', 'my_module'),
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int32', 'a0', ast.Ordinal(0), None),
             ('FIELD', 'int32', 'a1', ast.Ordinal(1), None),
             ('FIELD', 'int32', 'a2', ast.Ordinal(2), None),
             ('FIELD', 'int32', 'a9', ast.Ordinal(9), None),
             ('FIELD', 'int32', 'a10', ast.Ordinal(10), None),
             ('FIELD', 'int32', 'a11', ast.Ordinal(11), None),
             ('FIELD', 'int32', 'a29', ast.Ordinal(29), None),
             ('FIELD', 'int32', 'a1234567890', ast.Ordinal(1234567890),
              None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidOrdinals(self):
    """Tests that (lexically) invalid ordinals are correctly detected."""

    source1 = """\
        module my_module {

        struct MyStruct {
          int32 a_missing@;
        };

        }  // module my_module
        """
    with self.assertRaisesRegexp(
        lexer.LexError,
        r"^my_file\.mojom:4: Error: Missing ordinal value$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        module my_module {

        struct MyStruct {
          int32 a_octal@01;
        };

        }  // module my_module
        """
    with self.assertRaisesRegexp(
        lexer.LexError,
        r"^my_file\.mojom:4: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        module my_module { struct MyStruct { int32 a_invalid_octal@08; }; }
        """
    with self.assertRaisesRegexp(
        lexer.LexError,
        r"^my_file\.mojom:1: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source3, "my_file.mojom")

    source4 = "module my_module { struct MyStruct { int32 a_hex@0x1aB9; }; }"
    with self.assertRaisesRegexp(
        lexer.LexError,
        r"^my_file\.mojom:1: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source4, "my_file.mojom")

    source5 = "module my_module { struct MyStruct { int32 a_hex@0X0; }; }"
    with self.assertRaisesRegexp(
        lexer.LexError,
        r"^my_file\.mojom:1: Error: "
            r"Octal and hexadecimal ordinal values not allowed$"):
      parser.Parse(source5, "my_file.mojom")

    source6 = """\
        struct MyStruct {
          int32 a_too_big@999999999999;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: "
            r"Ordinal value 999999999999 too large:\n"
            r" *int32 a_too_big@999999999999;$"):
      parser.Parse(source6, "my_file.mojom")

  def testNestedNamespace(self):
    """Tests that "nested" namespaces work."""

    source = """\
        module my.mod {

        struct MyStruct {
          int32 a;
        };

        }  // module my.mod
        """
    expected = \
        [('MODULE',
          ('IDENTIFIER', 'my.mod'),
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int32', 'a', ast.Ordinal(None), None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidHandleTypes(self):
    """Tests (valid) handle types."""

    source = """\
        struct MyStruct {
          handle a;
          handle<data_pipe_consumer> b;
          handle <data_pipe_producer> c;
          handle < message_pipe > d;
          handle
            < shared_buffer
            > e;
        };
        """
    expected = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'handle', 'a', ast.Ordinal(None), None),
             ('FIELD', 'handle<data_pipe_consumer>', 'b', ast.Ordinal(None),
              None),
             ('FIELD', 'handle<data_pipe_producer>', 'c', ast.Ordinal(None),
              None),
             ('FIELD', 'handle<message_pipe>', 'd', ast.Ordinal(None), None),
             ('FIELD', 'handle<shared_buffer>', 'e', ast.Ordinal(None),
              None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidHandleType(self):
    """Tests an invalid (unknown) handle type."""

    source = """\
        struct MyStruct {
          handle<wtf_is_this> foo;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: "
            r"Invalid handle type 'wtf_is_this':\n"
            r" *handle<wtf_is_this> foo;$"):
      parser.Parse(source, "my_file.mojom")

  def testValidDefaultValues(self):
    """Tests default values that are valid (to the parser)."""

    source = """\
        struct MyStruct {
          int16 a0 = 0;
          uint16 a1 = 0x0;
          uint16 a2 = 0x00;
          uint16 a3 = 0x01;
          uint16 a4 = 0xcd;
          int32 a5 = 12345;
          int64 a6 = -12345;
          int64 a7 = +12345;
          uint32 a8 = 0x12cd3;
          uint32 a9 = -0x12cD3;
          uint32 a10 = +0x12CD3;
          bool a11 = true;
          bool a12 = false;
          float a13 = 1.2345;
          float a14 = -1.2345;
          float a15 = +1.2345;
          float a16 = 123.;
          float a17 = .123;
          double a18 = 1.23E10;
          double a19 = 1.E-10;
          double a20 = .5E+10;
          double a21 = -1.23E10;
          double a22 = +.123E10;
        };
        """
    expected = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int16', 'a0', ast.Ordinal(None), '0'),
             ('FIELD', 'uint16', 'a1', ast.Ordinal(None), '0x0'),
             ('FIELD', 'uint16', 'a2', ast.Ordinal(None), '0x00'),
             ('FIELD', 'uint16', 'a3', ast.Ordinal(None), '0x01'),
             ('FIELD', 'uint16', 'a4', ast.Ordinal(None), '0xcd'),
             ('FIELD', 'int32', 'a5' , ast.Ordinal(None), '12345'),
             ('FIELD', 'int64', 'a6', ast.Ordinal(None), '-12345'),
             ('FIELD', 'int64', 'a7', ast.Ordinal(None), '+12345'),
             ('FIELD', 'uint32', 'a8', ast.Ordinal(None), '0x12cd3'),
             ('FIELD', 'uint32', 'a9', ast.Ordinal(None), '-0x12cD3'),
             ('FIELD', 'uint32', 'a10', ast.Ordinal(None), '+0x12CD3'),
             ('FIELD', 'bool', 'a11', ast.Ordinal(None), 'true'),
             ('FIELD', 'bool', 'a12', ast.Ordinal(None), 'false'),
             ('FIELD', 'float', 'a13', ast.Ordinal(None), '1.2345'),
             ('FIELD', 'float', 'a14', ast.Ordinal(None), '-1.2345'),
             ('FIELD', 'float', 'a15', ast.Ordinal(None), '+1.2345'),
             ('FIELD', 'float', 'a16', ast.Ordinal(None), '123.'),
             ('FIELD', 'float', 'a17', ast.Ordinal(None), '.123'),
             ('FIELD', 'double', 'a18', ast.Ordinal(None), '1.23E10'),
             ('FIELD', 'double', 'a19', ast.Ordinal(None), '1.E-10'),
             ('FIELD', 'double', 'a20', ast.Ordinal(None), '.5E+10'),
             ('FIELD', 'double', 'a21', ast.Ordinal(None), '-1.23E10'),
             ('FIELD', 'double', 'a22', ast.Ordinal(None), '+.123E10')])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidFixedSizeArray(self):
    """Tests parsing a fixed size array."""

    source = """\
        struct MyStruct {
          int32[] normal_array;
          int32[1] fixed_size_array_one_entry;
          int32[10] fixed_size_array_ten_entries;
        };
        """
    expected = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int32[]', 'normal_array', ast.Ordinal(None), None),
             ('FIELD', 'int32[1]', 'fixed_size_array_one_entry',
              ast.Ordinal(None), None),
             ('FIELD', 'int32[10]', 'fixed_size_array_ten_entries',
              ast.Ordinal(None), None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidNestedArray(self):
    """Tests parsing a nested array."""

    source = "struct MyStruct { int32[][] nested_array; };"
    expected = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            None,
            [('FIELD', 'int32[][]', 'nested_array', ast.Ordinal(None),
              None)])])]
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidFixedArraySize(self):
    """Tests that invalid fixed array bounds are correctly detected."""

    source1 = """\
        struct MyStruct {
          int32[0] zero_size_array;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Fixed array size 0 invalid\n"
            r" *int32\[0\] zero_size_array;$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          int32[999999999999] too_big_array;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Fixed array size 999999999999 invalid\n"
            r" *int32\[999999999999\] too_big_array;$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        struct MyStruct {
          int32[abcdefg] not_a_number;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'abcdefg':\n"
        r" *int32\[abcdefg\] not_a_number;"):
      parser.Parse(source3, "my_file.mojom")

  def testValidMethod(self):
    """Tests parsing method declarations."""

    source1 = "interface MyInterface { MyMethod(int32 a); };"
    expected1 = \
        [('MODULE',
          None,
          None,
          [('INTERFACE',
            'MyInterface',
            None,
            [('METHOD',
              'MyMethod',
              ast.ParameterList(ast.Parameter('int32', 'a', ast.Ordinal(None))),
              ast.Ordinal(None),
              None)])])]
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    source2 = """\
        interface MyInterface {
          MyMethod1@0(int32 a@0, int64 b@1);
          MyMethod2@1() => ();
        };
        """
    expected2 = \
        [('MODULE',
          None,
          None,
          [('INTERFACE',
            'MyInterface',
            None,
            [('METHOD',
              'MyMethod1',
              ast.ParameterList([ast.Parameter('int32', 'a', ast.Ordinal(0)),
                                 ast.Parameter('int64', 'b', ast.Ordinal(1))]),
              ast.Ordinal(0),
              None),
             ('METHOD',
              'MyMethod2',
              ast.ParameterList(),
              ast.Ordinal(1),
              ast.ParameterList())])])]
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    source3 = """\
        interface MyInterface {
          MyMethod(string a) => (int32 a, bool b);
        };
        """
    expected3 = \
        [('MODULE',
          None,
          None,
          [('INTERFACE',
            'MyInterface',
            None,
            [('METHOD',
              'MyMethod',
              ast.ParameterList(ast.Parameter('string', 'a',
                                              ast.Ordinal(None))),
              ast.Ordinal(None),
              ast.ParameterList([ast.Parameter('int32', 'a', ast.Ordinal(None)),
                                 ast.Parameter('bool', 'b',
                                               ast.Ordinal(None))]))])])]
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

  def testInvalidMethods(self):
    """Tests that invalid method declarations are correctly detected."""

    # No trailing commas.
    source1 = """\
        interface MyInterface {
          MyMethod(string a,);
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '\)':\n"
            r" *MyMethod\(string a,\);$"):
      parser.Parse(source1, "my_file.mojom")

    # No leading commas.
    source2 = """\
        interface MyInterface {
          MyMethod(, string a);
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected ',':\n"
            r" *MyMethod\(, string a\);$"):
      parser.Parse(source2, "my_file.mojom")

  def testValidAttributes(self):
    """Tests parsing attributes (and attribute lists)."""

    # Note: We use structs because they have (optional) attribute lists.

    # Empty attribute list.
    source1 = "[] struct MyStruct {};"
    expected1 = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            ast.AttributeList(),
            None)])]
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    # One-element attribute list, with name value.
    source2 = "[MyAttribute=MyName] struct MyStruct {};"
    expected2 = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            ast.AttributeList(ast.Attribute("MyAttribute", "MyName")),
            None)])]
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    # Two-element attribute list, with one string value and one integer value.
    source3 = "[MyAttribute1 = \"hello\", MyAttribute2 = 5] struct MyStruct {};"
    expected3 = \
        [('MODULE',
          None,
          None,
          [('STRUCT',
            'MyStruct',
            ast.AttributeList([ast.Attribute("MyAttribute1", "hello"),
                               ast.Attribute("MyAttribute2", 5)]),
            None)])]
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

    # TODO(vtl): Boolean attributes don't work yet. (In fact, we just |eval()|
    # literal (non-name) values, which is extremely dubious.)

  def testInvalidAttributes(self):
    """Tests that invalid attributes and attribute lists are correctly
    detected."""

    # Trailing commas not allowed.
    source1 = "[MyAttribute=MyName,] struct MyStruct {};"
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:1: Error: Unexpected '\]':\n"
            r"\[MyAttribute=MyName,\] struct MyStruct {};$"):
      parser.Parse(source1, "my_file.mojom")

    # Missing value.
    source2 = "[MyAttribute=] struct MyStruct {};"
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:1: Error: Unexpected '\]':\n"
            r"\[MyAttribute=\] struct MyStruct {};$"):
      parser.Parse(source2, "my_file.mojom")

    # Missing key.
    source3 = "[=MyName] struct MyStruct {};"
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:1: Error: Unexpected '=':\n"
            r"\[=MyName\] struct MyStruct {};$"):
      parser.Parse(source3, "my_file.mojom")

if __name__ == "__main__":
  unittest.main()
