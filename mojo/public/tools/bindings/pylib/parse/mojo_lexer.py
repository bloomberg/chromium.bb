# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys
import os.path

# Try to load the ply module, if not, then assume it is in the third_party
# directory.
try:
  # Disable lint check which fails to find the ply module.
  # pylint: disable=F0401
  from ply.lex import TOKEN
except ImportError:
  # This assumes this file is in src/mojo/public/tools/bindings/pylib/parse/.
  module_path, module_name = os.path.split(__file__)
  third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir,
                             os.pardir, os.pardir, os.pardir, 'third_party')
  sys.path.append(third_party)
  # pylint: disable=F0401
  from ply.lex import TOKEN


class LexError(Exception):
  def __init__(self, filename, lineno, msg):
    self.filename = filename
    self.lineno = lineno
    self.msg = msg

  def __str__(self):
    return "%s:%d: Error: %s" % (self.filename, self.lineno, self.msg)

  def __repr__(self):
    return str(self)


class Lexer(object):

  def __init__(self, filename):
    self.filename = filename

  ######################--   PRIVATE   --######################

  ##
  ## Internal auxiliary methods
  ##
  def _error(self, msg, token):
    raise LexError(self.filename, token.lineno, msg)

  ##
  ## Reserved keywords
  ##
  keywords = (
    'HANDLE',
    'DATA_PIPE_CONSUMER',
    'DATA_PIPE_PRODUCER',
    'MESSAGE_PIPE',
    'SHARED_BUFFER',

    'IMPORT',
    'MODULE',
    'STRUCT',
    'INTERFACE',
    'ENUM',
  )

  keyword_map = {}
  for keyword in keywords:
    keyword_map[keyword.lower()] = keyword

  ##
  ## All the tokens recognized by the lexer
  ##
  tokens = keywords + (
    # Identifiers
    'NAME',

    # Constants
    'ORDINAL',
    'INT_CONST_DEC', 'INT_CONST_OCT', 'INT_CONST_HEX',
    'FLOAT_CONST',
    'CHAR_CONST',

    # String literals
    'STRING_LITERAL',

    # Operators
    'PLUS', 'MINUS', 'TIMES', 'DIVIDE', 'MOD',
    'OR', 'AND', 'NOT', 'XOR', 'LSHIFT', 'RSHIFT',

    # Assignment
    'EQUALS',

    # Request / response
    'RESPONSE',

    # Delimiters
    'LPAREN', 'RPAREN',         # ( )
    'LBRACKET', 'RBRACKET',     # [ ]
    'LBRACE', 'RBRACE',         # { }
    'LANGLE', 'RANGLE',         # < >
    'SEMI',                     # ;
    'COMMA', 'DOT'              # , .
  )

  ##
  ## Regexes for use in tokens
  ##

  # valid C identifiers (K&R2: A.2.3), plus '$' (supported by some compilers)
  identifier = r'[a-zA-Z_$][0-9a-zA-Z_$]*'

  hex_prefix = '0[xX]'
  hex_digits = '[0-9a-fA-F]+'

  # integer constants (K&R2: A.2.5.1)
  integer_suffix_opt = \
      r'(([uU]ll)|([uU]LL)|(ll[uU]?)|(LL[uU]?)|([uU][lL])|([lL][uU]?)|[uU])?'
  decimal_constant = \
      '(0'+integer_suffix_opt+')|([1-9][0-9]*'+integer_suffix_opt+')'
  octal_constant = '0[0-7]*'+integer_suffix_opt
  hex_constant = hex_prefix+hex_digits+integer_suffix_opt

  bad_octal_constant = '0[0-7]*[89]'

  # character constants (K&R2: A.2.5.2)
  # Note: a-zA-Z and '.-~^_!=&;,' are allowed as escape chars to support #line
  # directives with Windows paths as filenames (..\..\dir\file)
  # For the same reason, decimal_escape allows all digit sequences. We want to
  # parse all correct code, even if it means to sometimes parse incorrect
  # code.
  #
  simple_escape = r"""([a-zA-Z._~!=&\^\-\\?'"])"""
  decimal_escape = r"""(\d+)"""
  hex_escape = r"""(x[0-9a-fA-F]+)"""
  bad_escape = r"""([\\][^a-zA-Z._~^!=&\^\-\\?'"x0-7])"""

  escape_sequence = \
      r"""(\\("""+simple_escape+'|'+decimal_escape+'|'+hex_escape+'))'
  cconst_char = r"""([^'\\\n]|"""+escape_sequence+')'
  char_const = "'"+cconst_char+"'"
  unmatched_quote = "('"+cconst_char+"*\\n)|('"+cconst_char+"*$)"
  bad_char_const = \
      r"""('"""+cconst_char+"""[^'\n]+')|('')|('"""+ \
      bad_escape+r"""[^'\n]*')"""

  # string literals (K&R2: A.2.6)
  string_char = r"""([^"\\\n]|"""+escape_sequence+')'
  string_literal = '"'+string_char+'*"'
  bad_string_literal = '"'+string_char+'*'+bad_escape+string_char+'*"'

  # floating constants (K&R2: A.2.5.3)
  exponent_part = r"""([eE][-+]?[0-9]+)"""
  fractional_constant = r"""([0-9]*\.[0-9]+)|([0-9]+\.)"""
  floating_constant = \
      '(((('+fractional_constant+')'+ \
      exponent_part+'?)|([0-9]+'+exponent_part+')))'

  # Ordinals
  ordinal = r'@[0-9]+'
  missing_ordinal_value = r'@'
  # Don't allow ordinal values in octal (even invalid octal, like 09) or
  # hexadecimal.
  octal_or_hex_ordinal_disallowed = r'@((0[0-9]+)|('+hex_prefix+hex_digits+'))'

  ##
  ## Rules for the normal state
  ##
  t_ignore = ' \t\r'

  # Newlines
  def t_NEWLINE(self, t):
    r'\n+'
    t.lexer.lineno += len(t.value)

  # Operators
  t_PLUS              = r'\+'
  t_MINUS             = r'-'
  t_TIMES             = r'\*'
  t_DIVIDE            = r'/'
  t_MOD               = r'%'
  t_OR                = r'\|'
  t_AND               = r'&'
  t_NOT               = r'~'
  t_XOR               = r'\^'
  t_LSHIFT            = r'<<'
  t_RSHIFT            = r'>>'

  # =
  t_EQUALS            = r'='

  # =>
  t_RESPONSE          = r'=>'

  # Delimiters
  t_LPAREN            = r'\('
  t_RPAREN            = r'\)'
  t_LBRACKET          = r'\['
  t_RBRACKET          = r'\]'
  t_LBRACE            = r'\{'
  t_RBRACE            = r'\}'
  t_LANGLE            = r'<'
  t_RANGLE            = r'>'
  t_COMMA             = r','
  t_DOT               = r'\.'
  t_SEMI              = r';'

  t_STRING_LITERAL    = string_literal

  # The following floating and integer constants are defined as
  # functions to impose a strict order (otherwise, decimal
  # is placed before the others because its regex is longer,
  # and this is bad)
  #
  @TOKEN(floating_constant)
  def t_FLOAT_CONST(self, t):
    return t

  @TOKEN(hex_constant)
  def t_INT_CONST_HEX(self, t):
    return t

  @TOKEN(bad_octal_constant)
  def t_BAD_CONST_OCT(self, t):
    msg = "Invalid octal constant"
    self._error(msg, t)

  @TOKEN(octal_constant)
  def t_INT_CONST_OCT(self, t):
    return t

  @TOKEN(decimal_constant)
  def t_INT_CONST_DEC(self, t):
    return t

  # Must come before bad_char_const, to prevent it from
  # catching valid char constants as invalid
  #
  @TOKEN(char_const)
  def t_CHAR_CONST(self, t):
    return t

  @TOKEN(unmatched_quote)
  def t_UNMATCHED_QUOTE(self, t):
    msg = "Unmatched '"
    self._error(msg, t)

  @TOKEN(bad_char_const)
  def t_BAD_CHAR_CONST(self, t):
    msg = "Invalid char constant %s" % t.value
    self._error(msg, t)

  # unmatched string literals are caught by the preprocessor

  @TOKEN(bad_string_literal)
  def t_BAD_STRING_LITERAL(self, t):
    msg = "String contains invalid escape code"
    self._error(msg, t)

  # Handle ordinal-related tokens in the right order:
  @TOKEN(octal_or_hex_ordinal_disallowed)
  def t_OCTAL_OR_HEX_ORDINAL_DISALLOWED(self, t):
    msg = "Octal and hexadecimal ordinal values not allowed"
    self._error(msg, t)

  @TOKEN(ordinal)
  def t_ORDINAL(self, t):
    return t

  @TOKEN(missing_ordinal_value)
  def t_BAD_ORDINAL(self, t):
    msg = "Missing ordinal value"
    self._error(msg, t)

  @TOKEN(identifier)
  def t_NAME(self, t):
    t.type = self.keyword_map.get(t.value, "NAME")
    return t

  # Ignore C and C++ style comments
  def t_COMMENT(self, t):
    r'(/\*(.|\n)*?\*/)|(//.*(\n[ \t]*//.*)*)'
    pass

  def t_error(self, t):
    msg = 'Illegal character %s' % repr(t.value[0])
    self._error(msg, t)
