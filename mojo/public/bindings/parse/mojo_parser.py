#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a syntax tree from a Mojo IDL file."""


import sys
import os.path


# Try to load the ply module, if not, then assume it is in the third_party
# directory.
try:
  # Disable lint check which fails to find the ply module.
  # pylint: disable=F0401
  from ply import lex
  from ply import yacc
except ImportError:
  module_path, module_name = os.path.split(__file__)
  third_party = os.path.join(
      module_path, os.pardir, os.pardir, os.pardir, os.pardir, 'third_party')
  sys.path.append(third_party)
  # pylint: disable=F0401
  from ply import lex
  from ply import yacc


def ListFromConcat(*items):
  """Generate list by concatenating inputs"""
  itemsout = []
  for item in items:
    if item is None:
      continue
    if type(item) is not type([]):
      itemsout.append(item)
    else:
      itemsout.extend(item)

  return itemsout


class Lexer(object):

  # This field is required by lex to specify the complete list of valid tokens.
  tokens = (
    'NAME',
    'NUMBER',

    'ORDINAL',

    'HANDLE',
    'DATAPIPECONSUMER',
    'DATAPIPEPRODUCER',
    'MESSAGEPIPE',

    'MODULE',
    'STRUCT',
    'INTERFACE',
    'ENUM',
    'VOID',

    'LCURLY',
    'RCURLY',
    'LPAREN',
    'RPAREN',
    'LANGLE',
    'RANGLE',
    'LBRACKET',
    'RBRACKET',
    'COMMA',
    'SEMICOLON',
    'EQUALS',
  )

  t_LCURLY     = r'{'
  t_RCURLY     = r'}'
  t_LPAREN     = r'\('
  t_RPAREN     = r'\)'
  t_LANGLE     = r'<'
  t_RANGLE     = r'>'
  t_LBRACKET   = r'\['
  t_RBRACKET   = r'\]'
  t_COMMA      = r','
  t_SEMICOLON  = r';'
  t_EQUALS     = r'='
  t_NAME       = r'[a-zA-Z_][a-zA-Z0-9_]*'
  t_NUMBER     = r'\d+'
  t_ORDINAL    = r'@[0-9]*'

  def t_HANDLE(self, t):
    r'handle'
    return t

  def t_DATAPIPECONSUMER(self, t):
    r'data_pipe_consumer'
    return t

  def t_DATAPIPEPRODUCER(self, t):
    r'data_pipe_producer'
    return t

  def t_MESSAGEPIPE(self, t):
    r'message_pipe'
    return t

  def t_MODULE(self, t):
    r'module'
    return t

  def t_STRUCT(self, t):
    r'struct'
    return t

  def t_INTERFACE(self, t):
    r'interface'
    return t

  def t_ENUM(self, t):
    r'enum'
    return t

  def t_VOID(self, t):
    r'void'
    return t

  # Ignore C and C++ style comments
  def t_COMMENT(self, t):
    r'(/\*(.|\n)*?\*/)|(//.*(\n[ \t]*//.*)*)'
    pass

  # Ignored characters
  t_ignore = " \t"

  def t_newline(self, t):
    r'\n+'
    t.lexer.lineno += t.value.count("\n")

  def t_error(self, t):
    print("Illegal character '%s'" % t.value[0])
    t.lexer.skip(1)


class Parser(object):

  def __init__(self, lexer):
    self.tokens = lexer.tokens

  def p_module(self, p):
    """module : MODULE NAME LCURLY definitions RCURLY"""
    p[0] = ('MODULE', p[2], p[4])

  def p_definitions(self, p):
    """definitions : definition definitions
                   |"""
    if len(p) > 1:
      p[0] = ListFromConcat(p[1], p[2])

  def p_definition(self, p):
    """definition : struct
                  | interface
                  | enum"""
    p[0] = p[1]

  def p_attribute_section(self, p):
    """attribute_section : LBRACKET attributes RBRACKET
                         | """
    if len(p) > 3:
      p[0] = p[2]

  def p_attributes(self, p):
    """attributes : attribute
                  | attribute COMMA attributes
                  | """
    if len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_attribute(self, p):
    """attribute : NAME EQUALS NUMBER
                 | NAME EQUALS NAME"""
    p[0] = ('ATTRIBUTE', p[1], p[3])

  def p_struct(self, p):
    """struct : attribute_section STRUCT NAME LCURLY fields RCURLY SEMICOLON"""
    p[0] = ('STRUCT', p[3], p[1], p[5])

  def p_fields(self, p):
    """fields : field fields
              |"""
    if len(p) > 1:
      p[0] = ListFromConcat(p[1], p[2])

  def p_field(self, p):
    """field : typename NAME ordinal SEMICOLON"""
    p[0] = ('FIELD', p[1], p[2], p[3])

  def p_interface(self, p):
    """interface : attribute_section INTERFACE NAME LCURLY methods RCURLY SEMICOLON"""
    p[0] = ('INTERFACE', p[3], p[1], p[5])

  def p_methods(self, p):
    """methods : method methods
               | """
    if len(p) > 1:
      p[0] = ListFromConcat(p[1], p[2])

  def p_method(self, p):
    """method : VOID NAME LPAREN parameters RPAREN ordinal SEMICOLON"""
    p[0] = ('METHOD', p[2], p[4], p[6])

  def p_parameters(self, p):
    """parameters : parameter
                  | parameter COMMA parameters
                  | """
    if len(p) == 1:
      p[0] = []
    elif len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_parameter(self, p):
    """parameter : typename NAME ordinal"""
    p[0] = ('PARAM', p[1], p[2], p[3])

  def p_typename(self, p):
    """typename : basictypename
                | array"""
    p[0] = p[1]

  def p_basictypename(self, p):
    """basictypename : NAME
                     | HANDLE
                     | specializedhandle"""
    p[0] = p[1]

  def p_specializedhandle(self, p):
    """specializedhandle : HANDLE LANGLE specializedhandlename RANGLE"""
    p[0] = "handle<" + p[3] + ">"

  def p_specializedhandlename(self, p):
    """specializedhandlename : DATAPIPECONSUMER
                             | DATAPIPEPRODUCER
                             | MESSAGEPIPE"""
    p[0] = p[1]

  def p_array(self, p):
    """array : basictypename LBRACKET RBRACKET"""
    p[0] = p[1] + "[]"

  def p_ordinal(self, p):
    """ordinal : ORDINAL
               | """
    if len(p) > 1:
      p[0] = p[1]

  def p_enum(self, p):
    """enum : ENUM NAME LCURLY enum_fields RCURLY SEMICOLON"""
    p[0] = ('ENUM', p[2], p[4])

  def p_enum_fields(self, p):
    """enum_fields : enum_field
                   | enum_field COMMA enum_fields
                   |"""
    if len(p) == 2:
      p[0] = ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = ListFromConcat(p[1], p[3])

  def p_enum_field(self, p):
    """enum_field : NAME
                  | NAME EQUALS NUMBER"""
    if len(p) == 2:
      p[0] = ('ENUM_FIELD', p[1], None)
    else:
      p[0] = ('ENUM_FIELD', p[1], p[3])

  def p_error(self, e):
    print('error: %s'%e)


def Parse(filename):
  lexer = Lexer()
  parser = Parser(lexer)

  lex.lex(object=lexer)
  yacc.yacc(module=parser, debug=0, write_tables=0)

  tree = yacc.parse(open(filename).read())
  return tree


def Main():
  if len(sys.argv) < 2:
    print("usage: %s filename" % (sys.argv[0]))
    sys.exit(1)
  tree = Parse(filename=sys.argv[1])
  print(tree)


if __name__ == '__main__':
  Main()
