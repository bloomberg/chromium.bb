#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test Utils library."""

import datetime
import operator
import os
import unittest
import __builtin__

import fixup_path
fixup_path.FixupPath()

from chromite.lib import osutils
from chromite.lib.paygen import unittest_lib
from chromite.lib.paygen import utils


class TestUtils(unittest_lib.MoxTestCase):
  """Test utils methods."""

  def testCreateTmpInvalidPath(self):
    """Test that we create a tmp eventually even with invalid paths."""
    tmps = ['/usr/local/nope', '/tmp']
    tmp = utils.CreateTmpDir(tmps=tmps)
    self.assertTrue(tmp.startswith('/tmp'))
    os.rmdir(tmp)

  def testCreateTmpRaiseException(self):
    """Test that we raise an exception when we do not have enough space."""
    self.assertRaises(utils.UnableToCreateTmpDir, utils.CreateTmpDir,
                      minimum_size=2**50)

  def testCreateTempFileWithContents(self):
    """Verify that we create a temp file with the right message in it."""

    message = "Test Message With Rocks In"

    # Create the temp file.
    with utils.CreateTempFileWithContents(message) as temp_file:
      temp_name = temp_file.name

      # Verify the name is valid.
      self.assertTrue(os.path.exists(temp_name))

      # Verify it has the right contents
      with open(temp_name, 'r') as f:
        contents = f.readlines()

      self.assertEqual([message], contents)

    # Verify the temp file goes away when we close it.
    self.assertFalse(os.path.exists(temp_name))

  #pylint: disable-msg=E1101
  @osutils.TempDirDecorator
  def testListdirFullpath(self):
    file_a = os.path.join(self.tempdir, 'a')
    file_b = os.path.join(self.tempdir, 'b')

    with file(file_a, 'w+'):
      pass

    with file(file_b, 'w+'):
      pass

    self.assertEqual(sorted(utils.ListdirFullpath(self.tempdir)),
                     [file_a, file_b])

  def testThreadedMapNormal(self):
    args = [2, 5, 7]
    results = utils.ThreadedMap((lambda x: x + 1), args)
    self.assertEqual(results, [3, 6, 8])

  def testThreadedMapStar(self):
    args = [(2, 3), (2, 5), (7, 10)]
    results = utils.ThreadedMap((lambda x, y: x * y), args, star=True)
    self.assertEqual(results, [6, 10, 70])

  def testThreadedMapException(self):
    args = [(6, 2), (1, 0), (9, 3)]
    self.assertRaises(utils.ThreadedMapError, utils.ThreadedMap,
                      (lambda x, y: x / y), args, star=True)

  def testGroup(self):
    items = [(1, 'a'), (2, 'b'), (1, 'c')]
    self.assertEquals(utils.Group(items, operator.itemgetter(0)),
                      [(1, [(1, 'a')]), (2, [(2, 'b')]), (1, [(1, 'c')])])

    items = [(1, 'c'), (2, 'b'), (1, 'a')]
    self.assertEquals(utils.Group(items, operator.itemgetter(0)),
                      [(1, [(1, 'c')]), (2, [(2, 'b')]), (1, [(1, 'a')])])

    items = [(1, 'a'), (1, 'c'), (2, 'b')]
    self.assertEquals(utils.Group(items, operator.itemgetter(0)),
                      [(1, [(1, 'a'), (1, 'c')]), (2, [(2, 'b')])])

    items = [(2, 'b'), (1, 'c'), (1, 'a')]
    self.assertEquals(utils.Group(items, operator.itemgetter(0)),
                      [(2, [(2, 'b')]), (1, [(1, 'c'), (1, 'a')])])

    # Special case: an empty input.
    self.assertEquals(utils.Group([], operator.itemgetter(0)), [])

  def testLinear(self):
    # Check basic linear growth.
    self.assertEquals([utils.Linear(x, 0, 5, 10, 20) for x in range(0, 6)],
                      range(10, 21, 2))

    # Check linear decay.
    self.assertEquals([utils.Linear(x, 0, 5, 20, 10) for x in range(0, 6)],
                      range(20, 9, -2))

    # Check threashold enforcement.
    self.assertEquals(utils.Linear(-2, 0, 5, 10, 20), 10)
    self.assertEquals(utils.Linear(7, 0, 5, 10, 20), 20)

  def testTimeDeltaToString(self):
    # Shorthand notation.
    C = datetime.timedelta
    c = C(days=5, hours=3, minutes=15, seconds=33, microseconds=12037)

    # Test with default formatting.
    self.assertEquals(utils.TimeDeltaToString(C(5)), '5d')
    self.assertEquals(utils.TimeDeltaToString(C(hours=3)), '3h')
    self.assertEquals(utils.TimeDeltaToString(C(minutes=15)), '15m')
    self.assertEquals(utils.TimeDeltaToString(C(seconds=33)), '33s')
    self.assertEquals(utils.TimeDeltaToString(C(microseconds=12037)), '0s')
    self.assertEquals(utils.TimeDeltaToString(c), '5d3h15m')

    # Test with forced seconds and altered precision.
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True), '5d3h15m33s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=1),
        '5d3h15m33s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=2),
        '5d3h15m33.01s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=3),
        '5d3h15m33.012s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=4),
        '5d3h15m33.012s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=5),
        '5d3h15m33.01203s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=6),
        '5d3h15m33.012037s')
    self.assertEquals(
        utils.TimeDeltaToString(c, force_seconds=True, subsecond_precision=7),
        '5d3h15m33.012037s')


class YNInteraction():
  """Class to hold a list of responses and expected reault of YN prompt."""
  def __init__(self, responses, expected_result):
    self.responses = responses
    self.expected_result = expected_result


class InputTest(unittest_lib.MoxTestCase):
  """Test cases for utils interactive user input."""

  def testGetInput(self):
    self.mox.StubOutWithMock(__builtin__, 'raw_input')

    prompt = 'Some prompt'
    response = 'Some response'
    __builtin__.raw_input(prompt).AndReturn(response)
    self.mox.ReplayAll()

    self.assertEquals(response, utils.GetInput(prompt))
    self.mox.VerifyAll()

  def _TestYesNoPrompt(self, interactions, prompt_suffix, default, full):
    self.mox.StubOutWithMock(__builtin__, 'raw_input')

    prompt = 'Continue' # Not important
    actual_prompt = '\n%s %s? ' % (prompt, prompt_suffix)

    for interaction in interactions:
      for response in interaction.responses:
        __builtin__.raw_input(actual_prompt).AndReturn(response)
    self.mox.ReplayAll()

    for interaction in interactions:
      retval = utils.YesNoPrompt(default, prompt=prompt, full=full)
      msg = ('A %s prompt with %r responses expected result %r, but got %r' %
             (prompt_suffix, interaction.responses,
              interaction.expected_result, retval))
      self.assertEquals(interaction.expected_result, retval, msg=msg)
    self.mox.VerifyAll()

  def testYesNoDefaultNo(self):
    tests = [YNInteraction([''], utils.NO),
             YNInteraction(['n'], utils.NO),
             YNInteraction(['no'], utils.NO),
             YNInteraction(['foo', ''], utils.NO),
             YNInteraction(['foo', 'no'], utils.NO),
             YNInteraction(['y'], utils.YES),
             YNInteraction(['ye'], utils.YES),
             YNInteraction(['yes'], utils.YES),
             YNInteraction(['foo', 'yes'], utils.YES),
             YNInteraction(['foo', 'bar', 'x', 'y'], utils.YES),
             ]
    self._TestYesNoPrompt(tests, '(y/N)', utils.NO, False)

  def testYesNoDefaultYes(self):
    tests = [YNInteraction([''], utils.YES),
             YNInteraction(['n'], utils.NO),
             YNInteraction(['no'], utils.NO),
             YNInteraction(['foo', ''], utils.YES),
             YNInteraction(['foo', 'no'], utils.NO),
             YNInteraction(['y'], utils.YES),
             YNInteraction(['ye'], utils.YES),
             YNInteraction(['yes'], utils.YES),
             YNInteraction(['foo', 'yes'], utils.YES),
             YNInteraction(['foo', 'bar', 'x', 'y'], utils.YES),
             ]
    self._TestYesNoPrompt(tests, '(Y/n)', utils.YES, False)

  def testYesNoDefaultNoFull(self):
    tests = [YNInteraction([''], utils.NO),
             YNInteraction(['n', ''], utils.NO),
             YNInteraction(['no'], utils.NO),
             YNInteraction(['foo', ''], utils.NO),
             YNInteraction(['foo', 'no'], utils.NO),
             YNInteraction(['y', 'yes'], utils.YES),
             YNInteraction(['ye', ''], utils.NO),
             YNInteraction(['yes'], utils.YES),
             YNInteraction(['foo', 'yes'], utils.YES),
             YNInteraction(['foo', 'bar', 'y', ''], utils.NO),
             YNInteraction(['n', 'y', 'no'], utils.NO),
            ]
    self._TestYesNoPrompt(tests, '(yes/No)', utils.NO, True)

  def testYesNoDefaultYesFull(self):
    tests = [YNInteraction([''], utils.YES),
             YNInteraction(['n', ''], utils.YES),
             YNInteraction(['no'], utils.NO),
             YNInteraction(['foo', ''], utils.YES),
             YNInteraction(['foo', 'no'], utils.NO),
             YNInteraction(['y', 'yes'], utils.YES),
             YNInteraction(['n', ''], utils.YES),
             YNInteraction(['yes'], utils.YES),
             YNInteraction(['foo', 'yes'], utils.YES),
             YNInteraction(['foo', 'bar', 'n', ''], utils.YES),
             YNInteraction(['n', 'y', 'no'], utils.NO),
            ]
    self._TestYesNoPrompt(tests, '(Yes/no)', utils.YES, True)


if __name__ == '__main__':
  unittest.main()
