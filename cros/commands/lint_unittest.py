#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the lint module."""

from __future__ import print_function

import collections
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))

from chromite.lib import cros_test_lib
import lint


class TestNode(object):
  """Object good enough to stand in for lint funcs"""

  Args = collections.namedtuple('Args', ('args', 'vararg', 'kwarg'))
  Arg = collections.namedtuple('Arg', ('name',))

  def __init__(self, doc='', fromlineno=0, path='foo.py', args=(), vararg='',
               kwarg=''):
    self.doc = doc
    self.lines = doc.split('\n')
    self.fromlineno = fromlineno
    self.file = path
    self.args = self.Args(args=[self.Arg(name=x) for x in args],
                          vararg=vararg, kwarg=kwarg)

  def argnames(self):
    return self.args


class DocStringCheckerTest(cros_test_lib.TestCase):
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
  )

  BAD_FUNC_DOCSTRINGS = (
      """
      bad first line
      """,
      """ whitespace is wrong""",
      """whitespace is wrong	""",
      """ whitespace is wrong

      Multiline tickles differently.
      """,
      """Should be no trailing blank lines

      Returns:
        a value

      """
      """ok line

      cuddled end""",
      """we want Args/Returns not Arguments/Return

      Arguments:
      Return:
      """,
      """section order is wrong here

      Raises:
      Returns:
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
      """
  )

  # The current linter isn't good enough yet to detect these.
  TODO_BAD_FUNC_DOCSTRINGS = (
      """The returns section isn't a proper section

      Args:
        bloop: de

      returns something
      """,
      """the indentation is incorrect

        Args:
          some: day
      """,
  )

  def add_message(self, msg_id, node=None, line=None, args=None):
    """Capture lint checks"""
    # We include node.doc here explicitly so the pretty assert message
    # inclues it in the output automatically.
    self.results.append((msg_id, node.doc, line, args))

  def setUp(self):
    self.results = []
    self.checker = lint.DocStringChecker()
    self.checker.add_message = self.add_message

  def testGood_visit_function(self):
    """Allow known good docstrings"""
    for dc in self.GOOD_FUNC_DOCSTRINGS:
      self.results = []
      node = TestNode(doc=dc)
      self.checker.visit_function(node)
      self.assertEqual(self.results, [],
                       msg='docstring was not accepted:\n"""%s"""' % dc)

  def testBad_visit_function(self):
    """Reject known bad docstrings"""
    for dc in self.BAD_FUNC_DOCSTRINGS:
      self.results = []
      node = TestNode(doc=dc)
      self.checker.visit_function(node)
      self.assertNotEqual(self.results, [],
                          msg='docstring was not rejected:\n"""%s"""' % dc)

  def testSmoke_visit_module(self):
    """Smoke test for modules"""
    self.checker.visit_module(TestNode(doc='foo'))
    self.assertEqual(self.results, [])
    self.checker.visit_module(TestNode(doc='', path='/foo/__init__.py'))
    self.assertEqual(self.results, [])

  def testSmoke_visit_class(self):
    """Smoke test for classes"""
    self.checker.visit_class(TestNode(doc='bar'))

  def testGood_check_first_line(self):
    """Verify _check_first_line accepts good inputs"""
    # pylint: disable=W0212
    docstrings = (
        'Some string',
    )
    for dc in docstrings:
      self.results = []
      node = TestNode(doc=dc)
      self.checker._check_first_line(node, node.lines)
      self.assertEqual(self.results, [],
                       msg='docstring was not accepted:\n"""%s"""' % dc)

  def testBad_check_first_line(self):
    """Verify _check_first_line rejects bad inputs"""
    # pylint: disable=W0212
    docstrings = (
        '\nSome string\n',
    )
    for dc in docstrings:
      self.results = []
      node = TestNode(doc=dc)
      self.checker._check_first_line(node, node.lines)
      self.assertEqual(len(self.results), 1)

  def testGoodFuncVarKwArg(self):
    """Check valid inputs for *args and **kwargs"""
    # pylint: disable=W0212
    for vararg in (None, 'args', '_args'):
      for kwarg in (None, 'kwargs', '_kwargs'):
        self.results = []
        node = TestNode(vararg=vararg, kwarg=kwarg)
        self.checker._check_func_signature(node)
        self.assertEqual(len(self.results), 0)

  def testMisnamedFuncVarKwArg(self):
    """Reject anything but *args and **kwargs"""
    # pylint: disable=W0212
    for vararg in ('arg', 'params', 'kwargs', '_moo'):
      self.results = []
      node = TestNode(vararg=vararg)
      self.checker._check_func_signature(node)
      self.assertEqual(len(self.results), 1)

    for kwarg in ('kwds', '_kwds', 'args', '_moo'):
      self.results = []
      node = TestNode(kwarg=kwarg)
      self.checker._check_func_signature(node)
      self.assertEqual(len(self.results), 1)

  def testGoodFuncArgs(self):
    """Verify normal args in Args are allowed"""
    # pylint: disable=W0212
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
      self.checker._check_all_args_in_doc(node, node.lines)
      self.assertEqual(len(self.results), 0)

  def testBadFuncArgs(self):
    """Verify bad/missing args in Args are caught"""
    # pylint: disable=W0212
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
    )
    for dc, args in datasets:
      self.results = []
      node = TestNode(doc=dc, args=args)
      self.checker._check_all_args_in_doc(node, node.lines)
      self.assertEqual(len(self.results), 1)


if __name__ == '__main__':
  cros_test_lib.main()
