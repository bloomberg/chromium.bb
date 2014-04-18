#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a syntax tree from a Mojo IDL file."""


import os.path
import sys

# Try to load the ply module, if not, then assume it is in the third_party
# directory.
try:
  # Disable lint check which fails to find the ply module.
  # pylint: disable=F0401
  from ply import lex
  from ply import yacc
except ImportError:
  # This assumes we're under some src/ directory.
  path = os.path.abspath(__file__)
  while os.path.split(path)[1] != "src":
    path = os.path.split(path)[0]
  sys.path.append(os.path.join(path, "third_party"))
  del path
  # pylint: disable=F0401
  from ply import lex
  from ply import yacc

import ast
from lexer import Lexer


_MAX_ORDINAL_VALUE = 0xffffffff


def _ListFromConcat(*items):
  """Generate list by concatenating inputs (note: only concatenates lists, not
  tuples or other iterables)."""
  itemsout = []
  for item in items:
    if item is None:
      continue
    if type(item) is not type([]):
      itemsout.append(item)
    else:
      itemsout.extend(item)
  return itemsout


class ParseError(Exception):

  def __init__(self, filename, message, lineno=None, snippet=None):
    self.filename = filename
    self.message = message
    self.lineno = lineno
    self.snippet = snippet

  def __str__(self):
    if self.lineno is None:
      return "%s: Error: %s" % (self.filename, self.message)

    s = "%s:%d: Error: %s" % (self.filename, self.lineno, self.message)
    return s if self.snippet is None else s + "\n" + self.snippet

  def __repr__(self):
    return str(self)


class Parser(object):

  def __init__(self, lexer, source, filename):
    self.tokens = lexer.tokens
    self.source = source
    self.filename = filename

  def p_root(self, p):
    """root : import root
            | module
            | definitions"""
    if len(p) > 2:
      p[0] = _ListFromConcat(p[1], p[2])
    else:
      # Generator expects a module. If one wasn't specified insert one with an
      # empty name.
      if p[1][0] != 'MODULE':
        p[0] = [('MODULE', '', p[1])]
      else:
        p[0] = [p[1]]

  def p_import(self, p):
    """import : IMPORT STRING_LITERAL"""
    # 'eval' the literal to strip the quotes.
    p[0] = ('IMPORT', eval(p[2]))

  def p_module(self, p):
    """module : MODULE identifier LBRACE definitions RBRACE"""
    p[0] = ('MODULE', p[2], p[4])

  def p_definitions(self, p):
    """definitions : definition definitions
                   | """
    if len(p) > 1:
      p[0] = _ListFromConcat(p[1], p[2])

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
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_attribute(self, p):
    """attribute : NAME EQUALS expression
                 | NAME EQUALS NAME"""
    p[0] = ('ATTRIBUTE', p[1], p[3])

  def p_struct(self, p):
    """struct : attribute_section STRUCT NAME LBRACE struct_body RBRACE SEMI"""
    p[0] = ('STRUCT', p[3], p[1], p[5])

  def p_struct_body(self, p):
    """struct_body : field struct_body
                   | enum struct_body
                   | """
    if len(p) > 1:
      p[0] = _ListFromConcat(p[1], p[2])

  def p_field(self, p):
    """field : typename NAME default ordinal SEMI"""
    p[0] = ('FIELD', p[1], p[2], p[4], p[3])

  def p_default(self, p):
    """default : EQUALS expression
               | EQUALS expression_object
               | """
    if len(p) > 2:
      p[0] = p[2]

  def p_interface(self, p):
    """interface : attribute_section INTERFACE NAME LBRACE interface_body \
                       RBRACE SEMI"""
    p[0] = ('INTERFACE', p[3], p[1], p[5])

  def p_interface_body(self, p):
    """interface_body : method interface_body
                      | enum interface_body
                      | """
    if len(p) > 1:
      p[0] = _ListFromConcat(p[1], p[2])

  def p_response(self, p):
    """response : RESPONSE LPAREN parameters RPAREN
                | """
    if len(p) > 3:
      p[0] = p[3]

  def p_method(self, p):
    """method : NAME ordinal LPAREN parameters RPAREN response SEMI"""
    p[0] = ('METHOD', p[1], p[4], p[2], p[6])

  def p_parameters(self, p):
    """parameters : parameter
                  | parameter COMMA parameters
                  | """
    if len(p) == 1:
      p[0] = []
    elif len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_parameter(self, p):
    """parameter : typename NAME ordinal"""
    p[0] = ('PARAM', p[1], p[2], p[3])

  def p_typename(self, p):
    """typename : basictypename
                | array"""
    p[0] = p[1]

  def p_basictypename(self, p):
    """basictypename : identifier
                     | HANDLE
                     | specializedhandle"""
    p[0] = p[1]

  def p_specializedhandle(self, p):
    """specializedhandle : HANDLE LANGLE specializedhandlename RANGLE"""
    p[0] = "handle<" + p[3] + ">"

  def p_specializedhandlename(self, p):
    """specializedhandlename : DATA_PIPE_CONSUMER
                             | DATA_PIPE_PRODUCER
                             | MESSAGE_PIPE
                             | SHARED_BUFFER"""
    p[0] = p[1]

  def p_array(self, p):
    """array : basictypename LBRACKET RBRACKET"""
    p[0] = p[1] + "[]"

  def p_ordinal(self, p):
    """ordinal : ORDINAL
               | """
    if len(p) > 1:
      value = int(p[1][1:])
      if value > _MAX_ORDINAL_VALUE:
        raise ParseError(self.filename, "Ordinal value %d too large:" % value,
                         lineno=p.lineno(1),
                         snippet=self._GetSnippet(p.lineno(1)))
      p[0] = ast.Ordinal(value, filename=self.filename, lineno=p.lineno(1))
    else:
      p[0] = ast.Ordinal(None)

  def p_enum(self, p):
    """enum : ENUM NAME LBRACE enum_fields RBRACE SEMI"""
    p[0] = ('ENUM', p[2], p[4])

  def p_enum_fields(self, p):
    """enum_fields : enum_field
                   | enum_field COMMA enum_fields
                   | """
    if len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_enum_field(self, p):
    """enum_field : NAME
                  | NAME EQUALS expression"""
    if len(p) == 2:
      p[0] = ('ENUM_FIELD', p[1], None)
    else:
      p[0] = ('ENUM_FIELD', p[1], p[3])

  ### Expressions ###

  def p_expression_object(self, p):
    """expression_object : expression_array
                         | LBRACE expression_object_elements RBRACE """
    if len(p) < 3:
      p[0] = p[1]
    else:
      p[0] = ('OBJECT', p[2])

  def p_expression_object_elements(self, p):
    """expression_object_elements : expression_object
                                  | expression_object COMMA expression_object_elements
                                  | """
    if len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  def p_expression_array(self, p):
    """expression_array : expression
                        | LBRACKET expression_array_elements RBRACKET """
    if len(p) < 3:
      p[0] = p[1]
    else:
      p[0] = ('ARRAY', p[2])

  def p_expression_array_elements(self, p):
    """expression_array_elements : expression_object
                                 | expression_object COMMA expression_array_elements
                                 | """
    if len(p) == 2:
      p[0] = _ListFromConcat(p[1])
    elif len(p) > 3:
      p[0] = _ListFromConcat(p[1], p[3])

  # TODO(vtl): This is now largely redundant.
  def p_expression(self, p):
    """expression : binary_expression"""
    p[0] = ('EXPRESSION', p[1])

  # PLY lets us specify precedence of operators, but since we don't actually
  # evaluate them, we don't need that here.
  # TODO(vtl): We're going to need to evaluate them.
  def p_binary_expression(self, p):
    """binary_expression : unary_expression
                         | binary_expression binary_operator \
                               binary_expression"""
    p[0] = _ListFromConcat(*p[1:])

  def p_binary_operator(self, p):
    """binary_operator : TIMES
                       | DIVIDE
                       | MOD
                       | PLUS
                       | MINUS
                       | RSHIFT
                       | LSHIFT
                       | AND
                       | OR
                       | XOR"""
    p[0] = p[1]

  def p_unary_expression(self, p):
    """unary_expression : primary_expression
                        | unary_operator expression"""
    p[0] = _ListFromConcat(*p[1:])

  def p_unary_operator(self, p):
    """unary_operator : PLUS
                      | MINUS
                      | NOT"""
    p[0] = p[1]

  def p_primary_expression(self, p):
    """primary_expression : constant
                          | identifier
                          | LPAREN expression RPAREN"""
    p[0] = _ListFromConcat(*p[1:])

  def p_identifier(self, p):
    """identifier : NAME
                  | NAME DOT identifier"""
    p[0] = ''.join(p[1:])

  def p_constant(self, p):
    """constant : INT_CONST_DEC
                | INT_CONST_OCT
                | INT_CONST_HEX
                | FLOAT_CONST
                | CHAR_CONST
                | STRING_LITERAL"""
    p[0] = _ListFromConcat(*p[1:])

  def p_error(self, e):
    if e is None:
      # Unexpected EOF.
      # TODO(vtl): Can we figure out what's missing?
      raise ParseError(self.filename, "Unexpected end of file")

    raise ParseError(self.filename, "Unexpected %r:" % e.value, lineno=e.lineno,
                     snippet=self._GetSnippet(e.lineno))

  def _GetSnippet(self, lineno):
    return self.source.split('\n')[lineno - 1]


def Parse(source, filename):
  lexer = Lexer(filename)
  parser = Parser(lexer, source, filename)

  lex.lex(object=lexer)
  yacc.yacc(module=parser, debug=0, write_tables=0)

  tree = yacc.parse(source)
  return tree


def main(argv):
  if len(argv) < 2:
    print "usage: %s filename" % argv[0]
    return 0

  for filename in argv[1:]:
    with open(filename) as f:
      print "%s:" % filename
      try:
        print Parse(f.read(), filename)
      except ParseError, e:
        print e
        return 1

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
