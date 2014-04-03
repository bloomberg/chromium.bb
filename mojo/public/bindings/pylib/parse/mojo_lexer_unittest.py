# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojo_lexer
import unittest

# Try to load the ply module, if not, then assume it is in the third_party
# directory.
try:
  # Disable lint check which fails to find the ply module.
  # pylint: disable=F0401
  from ply import lex
except ImportError:
  module_path, module_name = os.path.split(__file__)
  third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir,
                             os.pardir, os.pardir, 'third_party')
  sys.path.append(third_party)
  # pylint: disable=F0401
  from ply import lex


# This (monkey-patching LexToken to make comparison value-based) is evil, but
# we'll do it anyway. (I'm pretty sure ply's lexer never cares about comparing
# for object identity.)
def _LexTokenEq(self, other):
  return self.type == other.type and self.value == other.value and \
         self.lineno == other.lineno and self.lexpos == other.lexpos
setattr(lex.LexToken, '__eq__', _LexTokenEq)


def _MakeLexToken(type, value, lineno=1, lexpos=0):
  """Makes a LexToken with the given parameters. (Note that lineno is 1-based,
  but lexpos is 0-based.)"""
  rv = lex.LexToken()
  rv.type, rv.value, rv.lineno, rv.lexpos = type, value, lineno, lexpos
  return rv


def _MakeLexTokenForKeyword(keyword, **kwargs):
  """Makes a LexToken for the given keyword."""
  return _MakeLexToken(keyword.upper(), keyword.lower(), **kwargs)


class MojoLexerTest(unittest.TestCase):
  """Tests mojo_lexer (in particular, Lexer)."""

  def __init__(self, *args, **kwargs):
    unittest.TestCase.__init__(self, *args, **kwargs)
    # Clone all lexer instances from this one, since making a lexer is slow.
    self._zygote_lexer = lex.lex(mojo_lexer.Lexer("my_file.mojom"))

  def testValidSingleKeywords(self):
    """Tests valid, single keywords."""
    self.assertEquals(self._SingleTokenForInput("handle"),
                      _MakeLexTokenForKeyword("handle"))
    self.assertEquals(self._SingleTokenForInput("data_pipe_consumer"),
                      _MakeLexTokenForKeyword("data_pipe_consumer"))
    self.assertEquals(self._SingleTokenForInput("data_pipe_producer"),
                      _MakeLexTokenForKeyword("data_pipe_producer"))
    self.assertEquals(self._SingleTokenForInput("message_pipe"),
                      _MakeLexTokenForKeyword("message_pipe"))
    self.assertEquals(self._SingleTokenForInput("import"),
                      _MakeLexTokenForKeyword("import"))
    self.assertEquals(self._SingleTokenForInput("module"),
                      _MakeLexTokenForKeyword("module"))
    self.assertEquals(self._SingleTokenForInput("struct"),
                      _MakeLexTokenForKeyword("struct"))
    self.assertEquals(self._SingleTokenForInput("interface"),
                      _MakeLexTokenForKeyword("interface"))
    self.assertEquals(self._SingleTokenForInput("enum"),
                      _MakeLexTokenForKeyword("enum"))

  def testValidSingleTokens(self):
    """Tests valid, single (non-keyword) tokens."""
    self.assertEquals(self._SingleTokenForInput("asdf"),
                      _MakeLexToken("NAME", "asdf"))
    self.assertEquals(self._SingleTokenForInput("@123"),
                      _MakeLexToken("ORDINAL", "@123"))
    self.assertEquals(self._SingleTokenForInput("456"),
                      _MakeLexToken("INT_CONST_DEC", "456"))
    self.assertEquals(self._SingleTokenForInput("0765"),
                      _MakeLexToken("INT_CONST_OCT", "0765"))
    self.assertEquals(self._SingleTokenForInput("0x01aB2eF3"),
                      _MakeLexToken("INT_CONST_HEX", "0x01aB2eF3"))
    self.assertEquals(self._SingleTokenForInput("123.456"),
                      _MakeLexToken("FLOAT_CONST", "123.456"))
    self.assertEquals(self._SingleTokenForInput("'x'"),
                      _MakeLexToken("CHAR_CONST", "'x'"))
    self.assertEquals(self._SingleTokenForInput("\"hello\""),
                      _MakeLexToken("STRING_LITERAL", "\"hello\""))
    self.assertEquals(self._SingleTokenForInput("+"),
                      _MakeLexToken("PLUS", "+"))
    self.assertEquals(self._SingleTokenForInput("-"),
                      _MakeLexToken("MINUS", "-"))
    self.assertEquals(self._SingleTokenForInput("*"),
                      _MakeLexToken("TIMES", "*"))
    self.assertEquals(self._SingleTokenForInput("/"),
                      _MakeLexToken("DIVIDE", "/"))
    self.assertEquals(self._SingleTokenForInput("%"),
                      _MakeLexToken("MOD", "%"))
    self.assertEquals(self._SingleTokenForInput("|"),
                      _MakeLexToken("OR", "|"))
    self.assertEquals(self._SingleTokenForInput("~"),
                      _MakeLexToken("NOT", "~"))
    self.assertEquals(self._SingleTokenForInput("^"),
                      _MakeLexToken("XOR", "^"))
    self.assertEquals(self._SingleTokenForInput("<<"),
                      _MakeLexToken("LSHIFT", "<<"))
    self.assertEquals(self._SingleTokenForInput(">>"),
                      _MakeLexToken("RSHIFT", ">>"))
    self.assertEquals(self._SingleTokenForInput("="),
                      _MakeLexToken("EQUALS", "="))
    self.assertEquals(self._SingleTokenForInput("=>"),
                      _MakeLexToken("RESPONSE", "=>"))
    self.assertEquals(self._SingleTokenForInput("("),
                      _MakeLexToken("LPAREN", "("))
    self.assertEquals(self._SingleTokenForInput(")"),
                      _MakeLexToken("RPAREN", ")"))
    self.assertEquals(self._SingleTokenForInput("["),
                      _MakeLexToken("LBRACKET", "["))
    self.assertEquals(self._SingleTokenForInput("]"),
                      _MakeLexToken("RBRACKET", "]"))
    self.assertEquals(self._SingleTokenForInput("{"),
                      _MakeLexToken("LBRACE", "{"))
    self.assertEquals(self._SingleTokenForInput("}"),
                      _MakeLexToken("RBRACE", "}"))
    self.assertEquals(self._SingleTokenForInput("<"),
                      _MakeLexToken("LANGLE", "<"))
    self.assertEquals(self._SingleTokenForInput(">"),
                      _MakeLexToken("RANGLE", ">"))
    self.assertEquals(self._SingleTokenForInput(";"),
                      _MakeLexToken("SEMI", ";"))
    self.assertEquals(self._SingleTokenForInput(","),
                      _MakeLexToken("COMMA", ","))
    self.assertEquals(self._SingleTokenForInput("."),
                      _MakeLexToken("DOT", "."))

  def _TokensForInput(self, input):
    """Gets a list of tokens for the given input string."""
    lexer = self._zygote_lexer.clone()
    lexer.input(input)
    rv = []
    while True:
      tok = lexer.token()
      if not tok:
        return rv
      rv.append(tok)

  def _SingleTokenForInput(self, input):
    """Gets the single token for the given input string. (Raises an exception if
    the input string does not result in exactly one token.)"""
    toks = self._TokensForInput(input)
    assert len(toks) == 1
    return toks[0]


if __name__ == "__main__":
  unittest.main()
