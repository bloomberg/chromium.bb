# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the lint module."""

from __future__ import print_function

import collections
import os

from six.moves import StringIO

from chromite.cli.cros import lint
from chromite.lib import cros_test_lib
from chromite.lib import osutils


# pylint: disable=protected-access


class DocStringSectionDetailsTest(cros_test_lib.TestCase):
  """Basic DocStringSectionDetails class tests."""

  def testInit(self):
    """Verify constructor behavior."""
    s = lint.DocStringSectionDetails()
    self.assertEqual(None, s.name)
    self.assertEqual(None, s.header)
    self.assertEqual([], s.lines)
    self.assertEqual(None, s.lineno)

    s = lint.DocStringSectionDetails(
        name='Args', header='  Args:', lines=['    foo: Yes.'], lineno=2)
    self.assertEqual('Args', s.name)
    self.assertEqual('  Args:', s.header)
    self.assertEqual(['    foo: Yes.'], s.lines)
    self.assertEqual(2, s.lineno)

  def testStr(self):
    """Sanity check __str__."""
    s = lint.DocStringSectionDetails()
    self.assertNotEqual(None, str(s))

  def testRepr(self):
    """Sanity check __repr__."""
    s = lint.DocStringSectionDetails()
    self.assertNotEqual(None, repr(s))

  def testEqual(self):
    """Sanity check __eq__."""
    s1 = lint.DocStringSectionDetails()
    s2 = lint.DocStringSectionDetails()
    self.assertEqual(s1, s2)

    s2 = lint.DocStringSectionDetails(name='Args')
    self.assertNotEqual(s1, s2)
    s2 = lint.DocStringSectionDetails(header='  Args:')
    self.assertNotEqual(s1, s2)
    s2 = lint.DocStringSectionDetails(lineno=2)
    self.assertNotEqual(s1, s2)

    s1 = lint.DocStringSectionDetails(name='n', header='h', lineno=0)
    s2 = lint.DocStringSectionDetails(name='n', header='h', lineno=0)
    self.assertEqual(s1, s2)


class PylintrcConfigTest(cros_test_lib.TempDirTestCase):
  """Basic _PylintrcConfig tests."""

  def testEmptySettings(self):
    """Check default empty names behavior."""
    lint._PylintrcConfig('/dev/null', '', ())

  def testDefaultValue(self):
    """Check we can read a default."""
    cfg_file = os.path.join(self.tempdir, 'pylintrc')
    osutils.WriteFile(cfg_file, '[sect]\nkey = "  "\n')
    cfg = lint._PylintrcConfig(cfg_file, 'sect', (
        ('key', {'default': 'KEY', 'type': 'string'}),
        ('foo', {'default': 'FOO', 'type': 'string'}),
    ))
    self.assertEqual('  ', cfg.option_value('key'))
    self.assertEqual('FOO', cfg.option_value('foo'))


class TestNode(object):
  """Object good enough to stand in for lint funcs"""

  Args = collections.namedtuple('Args', ('args', 'vararg', 'kwarg'))
  Arg = collections.namedtuple('Arg', ('name',))

  def __init__(self, doc='', fromlineno=0, path='foo.py', args=(), vararg='',
               kwarg='', names=None, lineno=0, name='module',
               display_type='Module', col_offset=None):
    if names is None:
      names = [('name', None)]
    self.doc = doc
    self.lines = doc.split('\n')
    self.fromlineno = fromlineno
    self.lineno = lineno
    self.file = path
    self.args = self.Args(args=[self.Arg(name=x) for x in args],
                          vararg=vararg, kwarg=kwarg)
    self.names = names
    self.name = name
    self._display_type = display_type
    self.col_offset = col_offset

  def argnames(self):
    return [arg.name for arg in self.args.args]

  def display_type(self):
    return self._display_type


class StatStub(object):
  """Dummy object to stand in for stat checks."""

  def __init__(self, size=0, mode=0o644):
    self.st_size = size
    self.st_mode = mode


class CheckerTestCase(cros_test_lib.TestCase):
  """Helpers for Checker modules"""

  def add_message(self, msg_id, node=None, line=None, args=None):
    """Capture lint checks"""
    # We include node.doc here explicitly so the pretty assert message
    # inclues it in the output automatically.
    doc = node.doc if node else ''
    self.results.append((msg_id, doc, line, args))

  def setUp(self):
    assert hasattr(self, 'CHECKER'), 'TestCase must set CHECKER'

    self.results = []
    self.checker = self.CHECKER()
    self.checker.add_message = self.add_message

  def assertLintPassed(self, msg='Checks failed'):
    """Assert that no lint results have been queued."""
    msg += '\nChecks failed: %s' % ([x[0] for x in self.results],)
    self.assertEqual(self.results, [], msg=msg)

  def assertLintFailed(self, msg='Checks incorrectly passed', expected=()):
    """Assert that failed results matching |expected| have been queued."""
    if expected:
      self.assertEqual(list(expected), [x[0] for x in self.results])
    else:
      self.assertNotEqual(len(self.results), 0, msg=msg)


class DocStringCheckerTest(CheckerTestCase):
  """Tests for DocStringChecker module"""

  GOOD_FUNC_DOCSTRINGS = (
      'Some string',
      """Short summary

      Body of text.
      """,
      """line o text

      Body and comments on
      more than one line.

      Args:
        moo: cow

      Returns:
        some value

      Raises:
        something else
      """,
      """Short summary.

      Args:
        fat: cat

      Yields:
        a spoon
      """,
      """Don't flag args variables as sections.

      Args:
        return: Foo!
      """,
      """the indentation is extra special

      Returns:
        First line is two spaces which is ok.
          Then we indent some more!
      """,
  )

  BAD_FUNC_DOCSTRINGS = (
      """
      bad first line
      """,
      """The first line is good
      but the second one isn't
      """,
      """ whitespace is wrong""",
      """whitespace is wrong	""",
      """ whitespace is wrong

      Multiline tickles differently.
      """,
      """Should be no trailing blank lines

      Returns:
        a value

      """,
      """ok line

      cuddled end""",
      """we want Args, not Arguments

      Arguments:
        some: arg
      """,
      """section order is wrong here

      Raises:
        It raised.

      Returns:
        It returned
      """,
      """sections are duplicated

      Returns:
        True

      Returns:
        or was it false
      """,
      """sections lack whitespace between them

      Args:
        foo: bar
      Returns:
        yeah
      """,
      """yields is misspelled

      Yield:
        a car
      """,
      """We want Examples, not Usage.

      Usage:
        a car
      """,
      """Section name has bad spacing

      Args:\x20\x20\x20
        key: here
      """,
      """too many blank lines


      Returns:
        None
      """,
      """wrongly uses javadoc

      @returns None
      """,
      """the indentation is incorrect

        Args:
          some: day
      """,
      """the final indentation is incorrect

      Blah.
       """,
      """the indentation is incorrect

      Returns:
       one space but should be two
      """,
      """the indentation is incorrect

      Returns:
         three spaces but should be two (and we have just one line)
      """,
      """the indentation is incorrect

      Args:
         some: has three spaces but should be two
      """,
      """the indentation is incorrect

      Args:
       some: has one space but should be two
      """,
      """the indentation is incorrect

      Args:
          some: has four spaces but should be two
      """,
      """"Extra leading quotes.""",
  )

  # The current linter isn't good enough yet to detect these.
  TODO_BAD_FUNC_DOCSTRINGS = (
      """The returns section isn't a proper section

      Args:
        bloop: de

      returns something
      """,
  )

  CHECKER = lint.DocStringChecker

  def testGood_visit_function(self):
    """Allow known good docstrings"""
    for dc in self.GOOD_FUNC_DOCSTRINGS:
      self.results = []
      node = TestNode(doc=dc, display_type=None, col_offset=4)
      self.checker.visit_function(node)
      self.assertLintPassed(msg='docstring was not accepted:\n"""%s"""' % dc)

  def testBad_visit_function(self):
    """Reject known bad docstrings"""
    for dc in self.BAD_FUNC_DOCSTRINGS:
      self.results = []
      node = TestNode(doc=dc, display_type=None, col_offset=4)
      self.checker.visit_function(node)
      self.assertLintFailed(msg='docstring was not rejected:\n"""%s"""' % dc)

  def testSmoke_visit_module(self):
    """Smoke test for modules"""
    self.checker.visit_module(TestNode(doc='foo'))
    self.assertLintPassed()
    self.checker.visit_module(TestNode(doc='', path='/foo/__init__.py'))
    self.assertLintPassed()

  def testSmoke_visit_class(self):
    """Smoke test for classes"""
    self.checker.visit_class(TestNode(doc='bar'))

  def testGood_check_first_line(self):
    """Verify _check_first_line accepts good inputs"""
    docstrings = (
        'Some string',
    )
    for dc in docstrings:
      self.results = []
      node = TestNode(doc=dc)
      self.checker._check_first_line(node, node.lines)
      self.assertLintPassed(msg='docstring was not accepted:\n"""%s"""' % dc)

  def testBad_check_first_line(self):
    """Verify _check_first_line rejects bad inputs"""
    docstrings = (
        '\nSome string\n',
    )
    for dc in docstrings:
      self.results = []
      node = TestNode(doc=dc)
      self.checker._check_first_line(node, node.lines)
      self.assertLintFailed(expected=('C9009',))

  def testGood_check_second_line_blank(self):
    """Verify _check_second_line_blank accepts good inputs"""
    docstrings = (
        'Some string\n\nThis is the third line',
        'Some string',
    )
    for dc in docstrings:
      self.results = []
      node = TestNode(doc=dc)
      self.checker._check_second_line_blank(node, node.lines)
      self.assertLintPassed(msg='docstring was not accepted:\n"""%s"""' % dc)

  def testBad_check_second_line_blank(self):
    """Verify _check_second_line_blank rejects bad inputs"""
    docstrings = (
        'Some string\nnonempty secondline',
    )
    for dc in docstrings:
      self.results = []
      node = TestNode(doc=dc)
      self.checker._check_second_line_blank(node, node.lines)
      self.assertLintFailed(expected=('C9014',))

  def testGoodFuncVarKwArg(self):
    """Check valid inputs for *args and **kwargs"""
    for vararg in (None, 'args', '_args'):
      for kwarg in (None, 'kwargs', '_kwargs'):
        self.results = []
        node = TestNode(vararg=vararg, kwarg=kwarg)
        self.checker._check_func_signature(node)
        self.assertLintPassed()

  def testMisnamedFuncVarKwArg(self):
    """Reject anything but *args and **kwargs"""
    for vararg in ('arg', 'params', 'kwargs', '_moo'):
      self.results = []
      node = TestNode(vararg=vararg)
      self.checker._check_func_signature(node)
      self.assertLintFailed(expected=('C9011',))

    for kwarg in ('kwds', '_kwds', 'args', '_moo'):
      self.results = []
      node = TestNode(kwarg=kwarg)
      self.checker._check_func_signature(node)
      self.assertLintFailed(expected=('C9011',))

  def testGoodFuncArgs(self):
    """Verify normal args in Args are allowed"""
    datasets = (
        ("""args are correct, and cls is ignored

         Args:
           moo: cow
         """,
         ('cls', 'moo',), None, None,
        ),
        ("""args are correct, and self is ignored

         Args:
           moo: cow
           *args: here
         """,
         ('self', 'moo',), 'args', 'kwargs',
        ),
        ("""args are allowed to have annotations

         Args:
           moo (Cow): cow
         """,
         ('moo',), None, None,
        ),
        ("""args are allowed to wrap

         Args:
           moo:
             a big fat cow
             that takes many lines
             to describe its fatness
         """,
         ('moo',), None, 'kwargs',
        ),
    )
    for dc, args, vararg, kwarg in datasets:
      self.results = []
      node = TestNode(doc=dc, args=args, vararg=vararg, kwarg=kwarg)
      sections = self.checker._parse_docstring_sections(node, node.lines)
      self.checker._check_all_args_in_doc(node, node.lines, sections)
      self.assertLintPassed()

  def testBadFuncArgs(self):
    """Verify bad/missing args in Args are caught"""
    datasets = (
        ("""missing 'bar'

         Args:
           moo: cow
         """,
         ('moo', 'bar',),
        ),
        ("""missing 'cow' but has 'bloop'

         Args:
           moo: cow
         """,
         ('bloop',),
        ),
        ("""too much space after colon

         Args:
           moo:  cow
         """,
         ('moo',),
        ),
        ("""not enough space after colon

         Args:
           moo:cow
         """,
         ('moo',),
        ),
        ("""too much space after type and colon

         Args:
           moo (Cow):  cow
         """,
         ('moo',),
        ),
    )
    for dc, args in datasets:
      self.results = []
      node = TestNode(doc=dc, args=args)
      sections = self.checker._parse_docstring_sections(node, node.lines)
      self.checker._check_all_args_in_doc(node, node.lines, sections)
      self.assertLintFailed()

  def test_parse_docstring_sections(self):
    """Check docstrings are parsed."""
    datasets = (
        ("""Some docstring

         Args:
           foo: blah

         Raises:
           blaaaaaah

         Note:
           This section shouldn't be checked.

         Returns:
           some value
         """, [
             ('Args', lint.DocStringSectionDetails(
                 name='Args',
                 header='         Args:',
                 lines=['           foo: blah'],
                 lineno=3)),
             ('Raises', lint.DocStringSectionDetails(
                 name='Raises',
                 header='         Raises:',
                 lines=['           blaaaaaah'],
                 lineno=6)),
             ('Returns', lint.DocStringSectionDetails(
                 name='Returns',
                 header='         Returns:',
                 lines=['           some value'],
                 lineno=12)),
         ]),
    )
    for dc, expected in datasets:
      node = TestNode(doc=dc)
      sections = self.checker._parse_docstring_sections(node, node.lines)
      self.assertEqual(expected, sections.items())


class ChromiteLoggingCheckerTest(CheckerTestCase):
  """Tests for ChromiteLoggingChecker module"""

  CHECKER = lint.ChromiteLoggingChecker

  def testLoggingImported(self):
    """Test that import logging is flagged."""
    node = TestNode(names=[('logging', None)], lineno=15)
    self.checker.visit_import(node)
    self.assertEqual(self.results, [('R9301', '', 15, None)])

  def testLoggingNotImported(self):
    """Test that importing something else (not logging) is not flagged."""
    node = TestNode(names=[('myModule', None)], lineno=15)
    self.checker.visit_import(node)
    self.assertLintPassed()


class SourceCheckerTest(CheckerTestCase):
  """Tests for SourceChecker module"""

  CHECKER = lint.SourceChecker

  def _testShebang(self, shebangs, exp, mode):
    """Helper for shebang tests"""
    for shebang in shebangs:
      self.results = []
      node = TestNode()
      stream = StringIO(shebang)
      st = StatStub(size=len(shebang), mode=mode)
      self.checker._check_shebang(node, stream, st)
      msg = 'processing shebang failed: %r' % shebang
      if not exp:
        self.assertLintPassed(msg=msg)
      else:
        self.assertLintFailed(msg=msg, expected=exp)

  def testBadShebang(self):
    """Verify _check_shebang rejects bad shebangs"""
    shebangs = (
        '#!/usr/bin/python\n',
        '#! /usr/bin/python2 \n',
        '#!/usr/bin/env python\n',
        '#! /usr/bin/env python2 \n',
        '#!/usr/bin/python2\n',
    )
    self._testShebang(shebangs, ('R9200',), 0o755)

  def testGoodShebangNoExec(self):
    """Verify _check_shebang rejects shebangs on non-exec files"""
    shebangs = (
        '#!/usr/bin/env python2\n',
        '#!/usr/bin/env python3\n',
    )
    self._testShebang(shebangs, ('R9202',), 0o644)

  def testGoodShebang(self):
    """Verify _check_shebang accepts good shebangs"""
    shebangs = (
        '#!/usr/bin/env python2\n',
        '#!/usr/bin/env python3\n',
        '#!/usr/bin/env python2\t\n',
    )
    self._testShebang(shebangs, (), 0o755)

  def testEmptyFileNoEncoding(self):
    """_check_encoding should ignore 0 byte files"""
    node = TestNode()
    self.results = []
    stream = StringIO('')
    self.checker._check_encoding(node, stream, StatStub())
    self.assertLintPassed()

  def testMissingEncoding(self):
    """_check_encoding should fail when there is no encoding"""
    headers = (
        '#',
        '#\n',
        '#\n#',
        '#\n#\n',
        '#!/usr/bin/python\n# foo\n'
        '#!/usr/bin/python\n',
        '# some comment\n',
        '# some comment\n# another line\n',
        '# first line is not a shebang\n# -*- coding: utf-8 -*-\n',
        '#!/usr/bin/python\n# second line\n# -*- coding: utf-8 -*-\n',
    )
    node = TestNode()
    for header in headers:
      self.results = []
      stream = StringIO(header)
      self.checker._check_encoding(node, stream, StatStub(size=len(header)))
      self.assertLintFailed(expected=('R9204',))

  def testBadEncoding(self):
    """_check_encoding should reject non-"utf-8" encodings"""
    encodings = (
        'UTF8', 'UTF-8', 'utf8', 'ISO-8859-1',
    )
    node = TestNode()
    for encoding in encodings:
      self.results = []
      header = '# -*- coding: %s -*-\n' % (encoding,)
      stream = StringIO(header)
      self.checker._check_encoding(node, stream, StatStub(size=len(header)))
      self.assertLintFailed(expected=('R9205',))

  def testGoodEncodings(self):
    """Verify _check_encoding accepts various correct encoding forms"""
    shebang = '#!/usr/bin/python\n'
    encodings = (
        '# -*- coding: utf-8 -*-',
    )
    node = TestNode()
    self.results = []
    for first in ('', shebang):
      for encoding in encodings:
        data = first + encoding + '\n'
        stream = StringIO(data)
        self.checker._check_encoding(node, stream, StatStub(size=len(data)))
        self.assertLintPassed()

  def testGoodUnittestName(self):
    """Verify _check_module_name accepts good unittest names"""
    module_names = (
        'lint_unittest',
    )
    for name in module_names:
      node = TestNode(name=name)
      self.results = []
      self.checker._check_module_name(node)
      self.assertLintPassed()

  def testBadUnittestName(self):
    """Verify _check_module_name rejects bad unittest names"""
    module_names = (
        'lint_unittests',
    )
    for name in module_names:
      node = TestNode(name=name)
      self.results = []
      self.checker._check_module_name(node)
      self.assertLintFailed(expected=('R9203',))
