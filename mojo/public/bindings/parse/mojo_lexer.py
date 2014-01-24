# PLY based Lexer class, based on pycparser by Eli Bendersky.
#
# Copyright (c) 2012, Eli Bendersky
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Eli Bendersky nor the names of its contributors may
#   be used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
  module_path, module_name = os.path.split(__file__)
  third_party = os.path.join(
      module_path, os.pardir, os.pardir, os.pardir, os.pardir, 'third_party')
  sys.path.append(third_party)
  # pylint: disable=F0401
  from ply.lex import TOKEN


class Lexer(object):
  ######################--   PRIVATE   --######################

  ##
  ## Internal auxiliary methods
  ##
  def _error(self, msg, token):
    print('%s at line %d' % (msg, token.lineno))
    self.lexer.skip(1)

  ##
  ## Reserved keywords
  ##
  keywords = (
    'HANDLE',
    'DATA_PIPE_CONSUMER',
    'DATA_PIPE_PRODUCER',
    'MESSAGE_PIPE',

    'MODULE',
    'STRUCT',
    'INTERFACE',
    'ENUM',
    'VOID',
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

    # constants
    'ORDINAL',
    'INT_CONST_DEC', 'INT_CONST_OCT', 'INT_CONST_HEX',
    'FLOAT_CONST', 'HEX_FLOAT_CONST',
    'CHAR_CONST',
    'WCHAR_CONST',

    # String literals
    'STRING_LITERAL',
    'WSTRING_LITERAL',

    # Operators
    'PLUS', 'MINUS', 'TIMES', 'DIVIDE', 'MOD',
    'OR', 'AND', 'NOT', 'XOR', 'LSHIFT', 'RSHIFT',
    'LOR', 'LAND', 'LNOT',
    'LT', 'LE', 'GT', 'GE', 'EQ', 'NE',

    # Assignment
    'EQUALS',

    # Conditional operator (?)
    'CONDOP',

    # Delimeters
    'LPAREN', 'RPAREN',         # ( )
    'LBRACKET', 'RBRACKET',     # [ ]
    'LBRACE', 'RBRACE',         # { }
    'SEMI', 'COLON',            # ; :
    'COMMA',                    # .
  )

  ##
  ## Regexes for use in tokens
  ##
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
  wchar_const = 'L'+char_const
  unmatched_quote = "('"+cconst_char+"*\\n)|('"+cconst_char+"*$)"
  bad_char_const = \
      r"""('"""+cconst_char+"""[^'\n]+')|('')|('"""+ \
      bad_escape+r"""[^'\n]*')"""

  # string literals (K&R2: A.2.6)
  string_char = r"""([^"\\\n]|"""+escape_sequence+')'
  string_literal = '"'+string_char+'*"'
  wstring_literal = 'L'+string_literal
  bad_string_literal = '"'+string_char+'*'+bad_escape+string_char+'*"'

  # floating constants (K&R2: A.2.5.3)
  exponent_part = r"""([eE][-+]?[0-9]+)"""
  fractional_constant = r"""([0-9]*\.[0-9]+)|([0-9]+\.)"""
  floating_constant = \
      '(((('+fractional_constant+')'+ \
      exponent_part+'?)|([0-9]+'+exponent_part+'))[FfLl]?)'
  binary_exponent_part = r'''([pP][+-]?[0-9]+)'''
  hex_fractional_constant = \
      '((('+hex_digits+r""")?\."""+hex_digits+')|('+hex_digits+r"""\.))"""
  hex_floating_constant = \
      '('+hex_prefix+'('+hex_digits+'|'+hex_fractional_constant+')'+ \
      binary_exponent_part+'[FfLl]?)'

  ##
  ## Rules for the normal state
  ##
  t_ignore = ' \t'

  # Newlines
  def t_NEWLINE(self, t):
    r'\n+'
    t.lexer.lineno += t.value.count("\n")

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
  t_LOR               = r'\|\|'
  t_LAND              = r'&&'
  t_LNOT              = r'!'
  t_LT                = r'<'
  t_GT                = r'>'
  t_LE                = r'<='
  t_GE                = r'>='
  t_EQ                = r'=='
  t_NE                = r'!='

  # =
  t_EQUALS            = r'='

  # ?
  t_CONDOP            = r'\?'

  # Delimeters
  t_LPAREN            = r'\('
  t_RPAREN            = r'\)'
  t_LBRACKET          = r'\['
  t_RBRACKET          = r'\]'
  t_LBRACE            = r'\{'
  t_RBRACE            = r'\}'
  t_COMMA             = r','
  t_SEMI              = r';'
  t_COLON             = r':'

  t_STRING_LITERAL    = string_literal
  t_ORDINAL           = r'@[0-9]*'

  # The following floating and integer constants are defined as
  # functions to impose a strict order (otherwise, decimal
  # is placed before the others because its regex is longer,
  # and this is bad)
  #
  @TOKEN(floating_constant)
  def t_FLOAT_CONST(self, t):
    return t

  @TOKEN(hex_floating_constant)
  def t_HEX_FLOAT_CONST(self, t):
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

  @TOKEN(wchar_const)
  def t_WCHAR_CONST(self, t):
    return t

  @TOKEN(unmatched_quote)
  def t_UNMATCHED_QUOTE(self, t):
    msg = "Unmatched '"
    self._error(msg, t)

  @TOKEN(bad_char_const)
  def t_BAD_CHAR_CONST(self, t):
    msg = "Invalid char constant %s" % t.value
    self._error(msg, t)

  @TOKEN(wstring_literal)
  def t_WSTRING_LITERAL(self, t):
    return t

  # unmatched string literals are caught by the preprocessor

  @TOKEN(bad_string_literal)
  def t_BAD_STRING_LITERAL(self, t):
    msg = "String contains invalid escape code"
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
