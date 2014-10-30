# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import os.path
import sys
import unittest

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
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList(),
        [])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testSourceWithCrLfs(self):
    """Tests a .mojom source with CR-LFs instead of LFs."""

    source = "// This is a comment.\r\n\r\nmodule my_module {\r\n}\r\n"
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList(),
        [])
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
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('a', None, 'int32', None),
                 ast.StructField('b', None, 'double', None)]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testSimpleStructWithoutModule(self):
    """Tests a simple struct without an enclosing module."""

    source = """\
        struct MyStruct {
          int32 a;
          double b;
        };
        """
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('a', None, 'int32', None),
                 ast.StructField('b', None, 'double', None)]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidStructDefinitions(self):
    """Tests all types of definitions that can occur in a struct."""

    source = """\
        struct MyStruct {
          enum MyEnum { VALUE };
          const double kMyConst = 1.23;
          int32 a;
          SomeOtherStruct b;  // Invalidity detected at another stage.
        };
        """
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.Enum('MyEnum',
                          ast.EnumValueList(ast.EnumValue('VALUE', None))),
                 ast.Const('kMyConst', 'double', '1.23'),
                 ast.StructField('a', None, 'int32', None),
                 ast.StructField('b', None, 'SomeOtherStruct', None)]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidStructDefinitions(self):
    """Tests that definitions that aren't allowed in a struct are correctly
    detected."""

    source1 = """\
        struct MyStruct {
          MyMethod(int32 a);
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '\(':\n"
            r" *MyMethod\(int32 a\);$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          struct MyInnerStruct {
            int32 a;
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'struct':\n"
            r" *struct MyInnerStruct {$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        struct MyStruct {
          interface MyInterface {
            MyMethod(int32 a);
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'interface':\n"
            r" *interface MyInterface {$"):
      parser.Parse(source3, "my_file.mojom")

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
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList(),
        [ast.Enum(
            'MyEnum1',
            ast.EnumValueList([ast.EnumValue('VALUE1', None),
                               ast.EnumValue('VALUE2', None)])),
         ast.Enum(
            'MyEnum2',
            ast.EnumValueList([ast.EnumValue('VALUE1', '-1'),
                               ast.EnumValue('VALUE2', '0'),
                               ast.EnumValue('VALUE3', '+987'),
                               ast.EnumValue('VALUE4', '0xAF12'),
                               ast.EnumValue('VALUE5', '-0x09bcd'),
                               ast.EnumValue('VALUE6', ('IDENTIFIER',
                                                        'VALUE5')),
                               ast.EnumValue('VALUE7', None)]))])
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
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList(),
        [ast.Struct(
            'MyStruct', None,
            ast.StructBody(
                [ast.Const('kNumber', 'int8', '-1'),
                 ast.StructField('number', ast.Ordinal(0), 'int8',
                                 ('IDENTIFIER', 'kNumber'))]))])
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
        parser.ParseError,
        r"^my_file\.mojom:4: Error: Unexpected '\?':\n"
            r" *MY_ENUM_1 = 1 \? 2 : 3$"):
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
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('a0', ast.Ordinal(0), 'int32', None),
                 ast.StructField('a1', ast.Ordinal(1), 'int32', None),
                 ast.StructField('a2', ast.Ordinal(2), 'int32', None),
                 ast.StructField('a9', ast.Ordinal(9), 'int32', None),
                 ast.StructField('a10', ast.Ordinal(10), 'int32', None),
                 ast.StructField('a11', ast.Ordinal(11), 'int32', None),
                 ast.StructField('a29', ast.Ordinal(29), 'int32', None),
                 ast.StructField('a1234567890', ast.Ordinal(1234567890),
                                 'int32', None)]))])
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
    expected = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my.mod'), None),
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(ast.StructField('a', None, 'int32', None)))])
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
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('a', None, 'handle', None),
                 ast.StructField('b', None, 'handle<data_pipe_consumer>', None),
                 ast.StructField('c', None, 'handle<data_pipe_producer>', None),
                 ast.StructField('d', None, 'handle<message_pipe>', None),
                 ast.StructField('e', None, 'handle<shared_buffer>', None)]))])
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
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('a0', None, 'int16', '0'),
                 ast.StructField('a1', None, 'uint16', '0x0'),
                 ast.StructField('a2', None, 'uint16', '0x00'),
                 ast.StructField('a3', None, 'uint16', '0x01'),
                 ast.StructField('a4', None, 'uint16', '0xcd'),
                 ast.StructField('a5' , None, 'int32', '12345'),
                 ast.StructField('a6', None, 'int64', '-12345'),
                 ast.StructField('a7', None, 'int64', '+12345'),
                 ast.StructField('a8', None, 'uint32', '0x12cd3'),
                 ast.StructField('a9', None, 'uint32', '-0x12cD3'),
                 ast.StructField('a10', None, 'uint32', '+0x12CD3'),
                 ast.StructField('a11', None, 'bool', 'true'),
                 ast.StructField('a12', None, 'bool', 'false'),
                 ast.StructField('a13', None, 'float', '1.2345'),
                 ast.StructField('a14', None, 'float', '-1.2345'),
                 ast.StructField('a15', None, 'float', '+1.2345'),
                 ast.StructField('a16', None, 'float', '123.'),
                 ast.StructField('a17', None, 'float', '.123'),
                 ast.StructField('a18', None, 'double', '1.23E10'),
                 ast.StructField('a19', None, 'double', '1.E-10'),
                 ast.StructField('a20', None, 'double', '.5E+10'),
                 ast.StructField('a21', None, 'double', '-1.23E10'),
                 ast.StructField('a22', None, 'double', '+.123E10')]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidFixedSizeArray(self):
    """Tests parsing a fixed size array."""

    source = """\
        struct MyStruct {
          array<int32> normal_array;
          array<int32, 1> fixed_size_array_one_entry;
          array<int32, 10> fixed_size_array_ten_entries;
          array<array<array<int32, 1>>, 2> nested_arrays;
        };
        """
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('normal_array', None, 'int32[]', None),
                 ast.StructField('fixed_size_array_one_entry', None, 'int32[1]',
                                 None),
                 ast.StructField('fixed_size_array_ten_entries', None,
                                 'int32[10]', None),
                 ast.StructField('nested_arrays', None,
                                 'int32[1][][2]', None)]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testValidNestedArray(self):
    """Tests parsing a nested array."""

    source = "struct MyStruct { array<array<int32>> nested_array; };"
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                ast.StructField('nested_array', None, 'int32[][]', None)))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidFixedArraySize(self):
    """Tests that invalid fixed array bounds are correctly detected."""

    source1 = """\
        struct MyStruct {
          array<int32, 0> zero_size_array;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Fixed array size 0 invalid\n"
            r" *array<int32, 0> zero_size_array;$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          array<int32, 999999999999> too_big_array;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Fixed array size 999999999999 invalid\n"
            r" *array<int32, 999999999999> too_big_array;$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        struct MyStruct {
          array<int32, abcdefg> not_a_number;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'abcdefg':\n"
        r" *array<int32, abcdefg> not_a_number;"):
      parser.Parse(source3, "my_file.mojom")

  def testValidAssociativeArrays(self):
    """Tests that we can parse valid associative array structures."""

    source1 = "struct MyStruct { map<string, uint8> data; };"
    expected1 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('data', None, 'uint8{string}', None)]))])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    source2 = "interface MyInterface { MyMethod(map<string, uint8> a); };"
    expected2 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Interface(
            'MyInterface',
            None,
            ast.InterfaceBody(
                ast.Method(
                    'MyMethod',
                    None,
                    ast.ParameterList(
                      ast.Parameter('a', None, 'uint8{string}')),
                    None)))])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    source3 = "struct MyStruct { map<string, array<uint8>> data; };"
    expected3 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('data', None, 'uint8[]{string}', None)]))])
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

  def testValidMethod(self):
    """Tests parsing method declarations."""

    source1 = "interface MyInterface { MyMethod(int32 a); };"
    expected1 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Interface(
            'MyInterface',
            None,
            ast.InterfaceBody(
                ast.Method(
                    'MyMethod',
                    None,
                    ast.ParameterList(ast.Parameter('a', None, 'int32')),
                    None)))])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    source2 = """\
        interface MyInterface {
          MyMethod1@0(int32 a@0, int64 b@1);
          MyMethod2@1() => ();
        };
        """
    expected2 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Interface(
            'MyInterface',
            None,
            ast.InterfaceBody(
                [ast.Method(
                    'MyMethod1',
                    ast.Ordinal(0),
                    ast.ParameterList([ast.Parameter('a', ast.Ordinal(0),
                                                     'int32'),
                                       ast.Parameter('b', ast.Ordinal(1),
                                                     'int64')]),
                    None),
                  ast.Method(
                    'MyMethod2',
                    ast.Ordinal(1),
                    ast.ParameterList(),
                    ast.ParameterList())]))])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    source3 = """\
        interface MyInterface {
          MyMethod(string a) => (int32 a, bool b);
        };
        """
    expected3 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Interface(
            'MyInterface',
            None,
            ast.InterfaceBody(
                ast.Method(
                    'MyMethod',
                    None,
                    ast.ParameterList(ast.Parameter('a', None, 'string')),
                    ast.ParameterList([ast.Parameter('a', None, 'int32'),
                                       ast.Parameter('b', None, 'bool')]))))])
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

  def testValidInterfaceDefinitions(self):
    """Tests all types of definitions that can occur in an interface."""

    source = """\
        interface MyInterface {
          enum MyEnum { VALUE };
          const int32 kMyConst = 123;
          MyMethod(int32 x) => (MyEnum y);
        };
        """
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Interface(
            'MyInterface',
            None,
            ast.InterfaceBody(
                [ast.Enum('MyEnum',
                          ast.EnumValueList(ast.EnumValue('VALUE', None))),
                 ast.Const('kMyConst', 'int32', '123'),
                 ast.Method(
                    'MyMethod',
                    None,
                    ast.ParameterList(ast.Parameter('x', None, 'int32')),
                    ast.ParameterList(ast.Parameter('y', None, 'MyEnum')))]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidInterfaceDefinitions(self):
    """Tests that definitions that aren't allowed in an interface are correctly
    detected."""

    source1 = """\
        interface MyInterface {
          struct MyStruct {
            int32 a;
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'struct':\n"
            r" *struct MyStruct {$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        interface MyInterface {
          interface MyInnerInterface {
            MyMethod(int32 x);
          };
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'interface':\n"
            r" *interface MyInnerInterface {$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        interface MyInterface {
          int32 my_field;
        };
        """
    # The parser thinks that "int32" is a plausible name for a method, so it's
    # "my_field" that gives it away.
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'my_field':\n"
            r" *int32 my_field;$"):
      parser.Parse(source3, "my_file.mojom")

  def testValidAttributes(self):
    """Tests parsing attributes (and attribute lists)."""

    # Note: We use structs because they have (optional) attribute lists.

    # Empty attribute list.
    source1 = "[] struct MyStruct {};"
    expected1 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct('MyStruct', ast.AttributeList(), ast.StructBody())])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    # One-element attribute list, with name value.
    source2 = "[MyAttribute=MyName] struct MyStruct {};"
    expected2 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            ast.AttributeList(ast.Attribute("MyAttribute", "MyName")),
            ast.StructBody())])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    # Two-element attribute list, with one string value and one integer value.
    source3 = "[MyAttribute1 = \"hello\", MyAttribute2 = 5] struct MyStruct {};"
    expected3 = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            ast.AttributeList([ast.Attribute("MyAttribute1", "hello"),
                               ast.Attribute("MyAttribute2", 5)]),
            ast.StructBody())])
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

  def testValidImports(self):
    """Tests parsing import statements."""

    # One import (no module statement).
    source1 = "import \"somedir/my.mojom\";"
    expected1 = ast.Mojom(
        None,
        ast.ImportList(ast.Import("somedir/my.mojom")),
        [])
    self.assertEquals(parser.Parse(source1, "my_file.mojom"), expected1)

    # Two imports (no module statement).
    source2 = """\
        import "somedir/my1.mojom";
        import "somedir/my2.mojom";
        """
    expected2 = ast.Mojom(
        None,
        ast.ImportList([ast.Import("somedir/my1.mojom"),
                        ast.Import("somedir/my2.mojom")]),
        [])
    self.assertEquals(parser.Parse(source2, "my_file.mojom"), expected2)

    # Imports with module statement.
    source3 = """\
        import "somedir/my1.mojom";
        import "somedir/my2.mojom";
        module my_module {}
        """
    expected3 = ast.Mojom(
        ast.Module(('IDENTIFIER', 'my_module'), None),
        ast.ImportList([ast.Import("somedir/my1.mojom"),
                        ast.Import("somedir/my2.mojom")]),
        [])
    self.assertEquals(parser.Parse(source3, "my_file.mojom"), expected3)

  def testInvalidImports(self):
    """Tests that invalid import statements are correctly detected."""

    source1 = """\
        // Make the error occur on line 2.
        import invalid
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'invalid':\n"
            r" *import invalid$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        import  // Missing string.
        module {}
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'module':\n"
            r" *module {}$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        import "foo.mojom"  // Missing semicolon.
        module {}
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected 'module':\n"
            r" *module {}$"):
      parser.Parse(source3, "my_file.mojom")

  def testValidNullableTypes(self):
    """Tests parsing nullable types."""

    source = """\
        struct MyStruct {
          int32? a;  // This is actually invalid, but handled at a different
                     // level.
          string? b;
          array<int32> ? c;
          array<string ? > ? d;
          array<array<int32>?>? e;
          array<int32, 1>? f;
          array<string?, 1>? g;
          some_struct? h;
          handle? i;
          handle<data_pipe_consumer>? j;
          handle<data_pipe_producer>? k;
          handle<message_pipe>? l;
          handle<shared_buffer>? m;
          some_interface&? n;
        };
        """
    expected = ast.Mojom(
        None,
        ast.ImportList(),
        [ast.Struct(
            'MyStruct',
            None,
            ast.StructBody(
                [ast.StructField('a', None, 'int32?', None),
                 ast.StructField('b', None, 'string?', None),
                 ast.StructField('c', None, 'int32[]?', None),
                 ast.StructField('d', None, 'string?[]?', None),
                 ast.StructField('e', None, 'int32[]?[]?', None),
                 ast.StructField('f', None, 'int32[1]?', None),
                 ast.StructField('g', None, 'string?[1]?', None),
                 ast.StructField('h', None, 'some_struct?', None),
                 ast.StructField('i', None, 'handle?', None),
                 ast.StructField('j', None, 'handle<data_pipe_consumer>?',
                                 None),
                 ast.StructField('k', None, 'handle<data_pipe_producer>?',
                                 None),
                 ast.StructField('l', None, 'handle<message_pipe>?', None),
                 ast.StructField('m', None, 'handle<shared_buffer>?', None),
                 ast.StructField('n', None, 'some_interface&?', None)]))])
    self.assertEquals(parser.Parse(source, "my_file.mojom"), expected)

  def testInvalidNullableTypes(self):
    """Tests that invalid nullable types are correctly detected."""
    source1 = """\
        struct MyStruct {
          string?? a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '\?':\n"
            r" *string\?\? a;$"):
      parser.Parse(source1, "my_file.mojom")

    source2 = """\
        struct MyStruct {
          handle?<data_pipe_consumer> a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '<':\n"
            r" *handle\?<data_pipe_consumer> a;$"):
      parser.Parse(source2, "my_file.mojom")

    source3 = """\
        struct MyStruct {
          some_interface?& a;
        };
        """
    with self.assertRaisesRegexp(
        parser.ParseError,
        r"^my_file\.mojom:2: Error: Unexpected '&':\n"
            r" *some_interface\?& a;$"):
      parser.Parse(source3, "my_file.mojom")

if __name__ == "__main__":
  unittest.main()
