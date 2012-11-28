#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains functionality used to implement a partial mock."""

import collections
import logging
import mock
import re

from chromite.lib import cros_build_lib


class Comparator(object):
  """Base class for all comparators."""

  def Match(self, arg):
    """Match the comparator against an argument."""
    raise NotImplementedError, 'method must be implemented by a subclass.'

  def Equals(self, rhs):
    """Returns whether rhs compares the same thing."""
    return type(self) == type(rhs) and self.__dict__ == rhs.__dict__

  def __eq__(self, rhs):
    return self.Equals(rhs)

  def __ne__(self, rhs):
    return not self.Equals(rhs)


class In(Comparator):
  """Checks whether an item (or key) is in a list (or dict) parameter."""

  def __init__(self, key):
    """Initialize.

    Arguments:
      key:  Any thing that could be in a list or a key in a dict
    """
    Comparator.__init__(self)
    self._key = key

  def Match(self, arg):
    try:
      return self._key in arg
    except TypeError:
      return False

  def __repr__(self):
    return '<sequence or map containing %r>' % str(self._key)


class Regex(Comparator):
  """Checks if a string matches a regular expression."""

  def __init__(self, pattern, flags=0):
    """Initialize.

    Arguments:
      pattern: is the regular expression to search for
      flags: passed to re.compile function as the second argument
    """
    Comparator.__init__(self)
    self.pattern = pattern
    self.flags = flags
    self.regex = re.compile(pattern, flags=flags)

  def Match(self, arg):
    try:
      return self.regex.search(arg) is not None
    except TypeError:
      return False

  def __repr__(self):
    s = '<regular expression %r' % self.regex.pattern
    if self.regex.flags:
      s += ', flags=%d' % self.regex.flags
    s += '>'
    return s


class ListRegex(Regex):
  """Checks if an iterable of strings matches a regular expression."""

  @staticmethod
  def _ProcessArg(arg):
    if not isinstance(arg, basestring):
      return ' '.join(arg)
    return arg

  def Match(self, arg):
    try:
      return self.regex.search(self._ProcessArg(arg)) is not None
    except TypeError:
      return False


class Ignore(Comparator):
  """Used when we don't care about an argument of a method call."""

  def Match(self, _arg):
    return True

  def __repr__(self):
    return '<IgnoreArg>'


def _RecursiveCompare(lhs, rhs):
  """Compare parameter specs recursively.

  Arguments:
    lhs, rhs: Function parameter specs to compare.
    equality: In the case of comparing Comparator objects, True means we call
      the Equals() function.  We call Match() if set to False (default).
  """
  if isinstance(lhs, Comparator):
    return lhs.Match(rhs)
  elif type(lhs) != type(rhs):
    return False
  elif isinstance(lhs, (tuple, list)):
    return (len(lhs) == len(rhs) and
            all(_RecursiveCompare(i, j) for i, j in zip(lhs, rhs)))
  elif isinstance(lhs, dict):
    return _RecursiveCompare(sorted(lhs.iteritems()), sorted(rhs.iteritems()))
  else:
    return lhs == rhs


class MockedCallResults(object):
  """Implements internal result specification for partial mocks.

  Used with the PartialMock class.

  Internal results are different from external results (return values,
  side effects, exceptions, etc.) for functions.  Internal results are
  *used* by the partial mock to generate external results.  Often internal
  results represent the external results of the dependencies of the function
  being partially mocked.  Of course, the partial mock can just pass through
  the internal results to become external results.
  """

  Params = collections.namedtuple('Params', ['args', 'kwargs'])
  MockedCall = collections.namedtuple(
      'MockedCall', ['params', 'strict', 'result', 'side_effect'])

  def __init__(self, name):
    """Initialize.

    Arguments:
      name: The name given to the mock.  Will be used in debug output.
    """
    self.name = name
    self.mocked_calls = []

  @staticmethod
  def AssertArgs(args, kwargs):
    """Verify arguments are of expected type."""
    assert isinstance(args, (tuple))
    if kwargs:
      assert isinstance(kwargs, dict)

  def AddResultForParams(self, args, result, kwargs=None, side_effect=None,
                         strict=True):
    """Record the internal results of a given partial mock call.

    Arguments:
      args: A list containing the positional args an invocation must have for
        it to match the internal result.  The list can contain instances of
        meta-args (such as IgnoreArg, Regex, In, etc.).  Positional argument
        matching is always *strict*, meaning extra positional arguments in
        the invocation are not allowed.
      result: The internal result that will be matched for the command
        invocation specified.
      kwargs: A dictionary containing the keyword args an invocation must have
        for it to match the internal result.  The dictionary can contain
        instances of meta-args (such as IgnoreArg, Regex, In, etc.).  Keyword
        argument matching is by default *strict*, but can be modified by the
        |strict| argument.
      side_effect:  A functor that gets called every time a partially mocked
        function is invoked.  The arguments the partial mock is invoked with are
        passed to the functor.  This is similar to how side effects work for
        mocks.
      strict: Specifies whether keyword are matched strictly.  With strict
        matching turned on, any keyword args a partial mock is invoked with that
        are not specified in |kwargs| will cause the match to fail.
    """
    self.AssertArgs(args, kwargs)
    if kwargs is None:
      kwargs = {}

    params = self.Params(args=args, kwargs=kwargs)
    dup, filtered = cros_build_lib.PredicateSplit(
        lambda mc: mc.params == params, self.mocked_calls)

    new = self.MockedCall(params=params, strict=strict, result=result,
                          side_effect=side_effect)
    filtered.append(new)
    self.mocked_calls = filtered

    if dup:
      logging.debug('%s: replacing mock for arguments %r:\n%r -> %r',
                    self.name, params, dup, new)

  def LookupResult(self, args, kwargs=None, hook_args=None, hook_kwargs=None):
    """For a given mocked function call lookup the recorded internal results.

    args: A list containing positional args the function was called with.
    kwargs: A dict containing keyword args the function was called with.
    hook_args: A list of positional args to call the hook with.
    hook_kwargs: A dict of key/value args to call the hook with.

    Returns:
      The recorded result for the invocation.

    Raises:
      AssertionError when the call is not mocked, or when there is more
      than one mock that matches.
    """
    def filter_fn(mc):
      if mc.strict:
        return _RecursiveCompare(mc.params, params)

      for k, v in mc.params.kwargs.iteritems():
        if k not in kwargs or not _RecursiveCompare(v, kwargs[k]):
          return False
      return _RecursiveCompare(mc.params.args, args)

    self.AssertArgs(args, kwargs)
    if kwargs is None:
      kwargs = {}

    params = self.Params(args, kwargs)
    matched, _ = cros_build_lib.PredicateSplit(filter_fn, self.mocked_calls)
    if len(matched) > 1:
      raise AssertionError(
          "%s: args %r matches more than one mock:\n%s"
          % (self.name, params, '\n'.join([repr(c) for c in matched])))
    elif not matched:
      raise AssertionError("%s: %r not mocked!" % (self.name, params))

    side_effect = matched[0].side_effect
    if side_effect:
      assert(hook_args is not None)
      assert(hook_kwargs is not None)
      hook_result = side_effect(*hook_args, **hook_kwargs)
      if hook_result is not None:
        return hook_result
    return matched[0].result


class PartialMock(object):
  """Provides functionality for partially mocking out a function or method.

  Partial mocking is useful in cases where the side effects of a function or
  method are complex, and so re-using the logic of the function with
  *dependencies* mocked out is preferred over mocking out the entire function
  and re-implementing the side effect (return value, state modification) logic
  in the test.  It is also useful for creating re-usable mocks.
  """

  TARGET = None
  ATTRS = None

  def __init__(self):
    self.backup = {}
    self.patchers = {}

  def __enter__(self):
    self.Start()

  def __exit__(self, exc_type, exc_value, traceback):
    self.Stop()

  def Start(self):
    """Activates the mock context."""
    chunks = self.TARGET.rsplit('.', 1)
    module = cros_build_lib.load_module(chunks[0])

    cls = getattr(module, chunks[1])
    for attr in self.ATTRS:
      self.backup[attr] = getattr(cls, attr)
      src_attr = '_target%s' % attr if attr.startswith('__') else attr
      if callable(self.backup[attr]):
        patcher = mock.patch.object(cls, attr, autospec=True,
                                    side_effect=getattr(self, src_attr))
      else:
        patcher = mock.patch.object(cls, attr, getattr(self, src_attr))
      patcher.start()
      self.patchers[attr] = patcher

  def Stop(self):
    """Restores namespace to the unmocked state."""
    for patcher in self.patchers.itervalues():
      patcher.stop()

  def UnMockAttr(self, attr):
    """Unsetting the mock of an attribute/function."""
    self.patchers.pop(attr).stop()


class PartialCmdMock(PartialMock):
  """Base class for mocking functions that wrap command line functionality.

  Implements mocking for functions that shell out.  The internal results are
  'returncode', 'output', 'error'.
  """
  CmdResult = collections.namedtuple(
    'MockResult', ['returncode', 'output', 'error'])

  def __init__(self):
    PartialMock.__init__(self)
    self._results = MockedCallResults(self.ATTRS[0])

  def AddCmdResult(self, cmd, returncode=0, output='', error='',
                   kwargs=None, strict=False, side_effect=None):
    """Specify the result to simulate for a given command.

    Arguments:
      cmd: The command string or list to record a result for.
      returncode: The returncode of the command (on the command line).
      output: The stdout output of the command.
      error: The stderr output of the command.
      kwargs: Keyword arguments that the function needs to be invoked with.
      strict: Defaults to False.  See MockedCallResults.AddResultForParams.
      side_effect: See MockedCallResults.AddResultForParams
    """
    result = self.CmdResult(returncode, output, error)
    self._results.AddResultForParams(
        (cmd,), result, kwargs=kwargs, side_effect=side_effect, strict=strict)
