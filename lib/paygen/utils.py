# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various internal build scripts."""

from __future__ import print_function

from contextlib import contextmanager
import datetime
import itertools
import os
import Queue
import subprocess
import tempfile
import threading
import time
import traceback

from chromite.lib import cros_logging as logging


# Give preference to /usr/local/google/tmp for space reasons.
TMPS = ('/usr/local/google/tmp', '/tmp')
TMP = next(d for d in TMPS if os.path.exists(d))

ONE_GIG = 2 ** 30      # One gig in bytes
TWO_GIGS = ONE_GIG * 2  # Two gigs in bytes


class CommandFailedException(Exception):
  """Exception gets thrown for a command that fails to execute."""


class UnableToCreateTmpDir(Exception):
  """Raised if we are unable to find a suitable tmp area."""


class Timer(object):
  """Timer class to measure the time that is spent during a task."""
  # pylint: disable=W0201

  def __enter__(self):
    self.__start = time.time()

  def __exit__(self, exc_type, exc_value, exc_traceback):
    self.__finish = time.time()

  def ElapsedTime(self):
    """Returns the time since the invocation of the object."""
    return self.__finish - self.__start


class CommandResult(object):
  """An object to store various attributes of a child process."""

  def __init__(self, cmd=None, error=None, output=None, returncode=None):
    self.cmd = cmd
    self.error = error
    self.output = output
    self.returncode = returncode

  @property
  def cmdstr(self):
    """Return self.cmd as space-separated string."""
    if self.cmd:
      return ' '.join(self.cmd)


def CreateTmpDir(prefix='cros-rel', tmps=TMPS, minimum_size=0):
  """Return a unique tmp dir with enough free space (if specified).

  Check if any tmp in tmps exists that also meets the minimum_size
  free space requirement. If so, return a unique tmp dir in that path.

  Args:
    prefix: Prefix to use with tempfile.mkdtemp.
    tmps: An iterable of directories to consider for tmp space.
    minimum_size: The minimum size the tmp dir needs to have. Default: 0.

  Raises:
    UnableToCreateTmpDir: If we are unable to find a suitable tmp dir.
  """
  for entry in tmps:
    if os.path.exists(entry):
      if not minimum_size or GetFreeSpace(entry) > minimum_size:
        return tempfile.mkdtemp(prefix=prefix, dir=entry)
      else:
        logging.warning('Not enough space in %s to create %s temp dir.',
                        entry, prefix)

  raise UnableToCreateTmpDir('Unable to find a suitable %s tmp dir.'
                             '  Considered: %s', prefix, ', '.join(tmps))


# TODO(mtennant): The RunCommand function in chromite/lib/cros_build_lib is
# more mature and has more functionality than this function.  Try using it here.
# In particular, note that this function handles redirect_stdout differently
# from redirect_stderr.  For stdout the output is returned, but for stderr
# the output is simply discarded (in both cases the output does not go to
# the standard stdout/stderr handlers if "redirected").
def RunCommand(cmd, error_ok=False, redirect_stdout=False,
               redirect_stderr=False, cwd=None, input=None, dryrun=False,
               shell=False, env=None, return_result=False):
  """Runs the given command passed in 'cmd'.

  Args:
    cmd: list of command and arguments to run.
    error_ok: ignore failures.
    redirect_stdout: boolean. If true redirects stdout.
    redirect_stderr: boolean. If true mutes stderr.  Returned
      output will not include stderr output, just stdout.
    cwd: Path to change to when running command. Default: None.
    input: input handle to the child process.
    dryrun: boolean.if True print the command an not execute.
    shell: boolean. if True run in a shell. default: False
    env: Environ dict to pass to the command. Default: None.
    return_result: If True, return CommandResult object instead of stdout.
      This behavior should become standard someday.

  Returns:
    If return_result is True then a CommandResult object is returned.
      Otherwise, return the stdout output of the child process.

  Raises:
    CommandFailedException: on child process failure and error_ok is False.
  """
  # pylint: disable=W0622
  cmd_result = CommandResult()

  # Prepare log_cmd for logging purposes only.
  log_cmd = cmd
  if type(cmd) is list:
    log_cmd = ' '.join(cmd)
  if cwd is not None:
    log_cmd = 'cd %s && %s' % (cwd, log_cmd)

  logging.debug('Begin: %s', log_cmd)

  if dryrun:
    return

  timer = Timer()
  with timer:
    if redirect_stdout:
      stdout = subprocess.PIPE
    else:
      stdout = None
    if redirect_stderr:
      stderr = subprocess.PIPE
    else:
      stderr = None
    if input:
      stdin = subprocess.PIPE
    else:
      stdin = None

    cmd_result.cmd = cmd
    proc = subprocess.Popen(cmd, cwd=cwd, stdin=stdin, stdout=stdout,
                            stderr=stderr, shell=shell, env=env)
    (cmd_result.output, cmd_result.error) = proc.communicate(input)
    cmd_result.returncode = proc.returncode

  logging.debug('Done : %s', log_cmd)
  logging.debug('Time Taken: %s seconds, Return Code: %r',
                timer.ElapsedTime(), cmd_result.returncode)

  if not error_ok and cmd_result.returncode != 0:
    raise CommandFailedException(cmd_result.error)

  if return_result:
    return cmd_result

  return cmd_result.output


def GetFreeSpace(path):
  """Return the available free space in bytes.

  Args:
    path: The dir path to check. If this is a file it will be converted to a
        path.

  Returns:
    The byte representation of available space.
  """
  if os.path.isfile(path):
    path = os.path.dirname(path)

  stats = os.statvfs(path)
  return stats.f_bavail * stats.f_frsize


def CreateTempFileWithContents(contents):
  """Creates a temp file containing contents which self deletes when closed.

  Args:
    contents: The string to write into the temp file.

  Returns:
    tempfile.NamedTemporaryFile. A file object that will self delete
    when closed.
  """
  message_file = tempfile.NamedTemporaryFile()
  message_file.write(contents)
  message_file.flush()
  return message_file


def ListdirFullpath(directory):
  """Return all files in a directory with full pathnames.

  Args:
    directory: directory to find files for.

  Returns:
    Full paths to every file in that directory.
  """
  return [os.path.join(directory, f) for f in os.listdir(directory)]


class RestrictedAttrDict(dict):
  """Define a dictionary which is also a struct.

  The keys will belong to a restricted list of values.
  """

  _slots = ()

  def __init__(self, *args, **kwargs):
    """Ensure that only the expected keys are added during initialization."""
    dict.__init__(self, *args, **kwargs)

    # Ensure all slots are at least populated with None.
    for key in self._slots:
      self.setdefault(key)

    for key in self.keys():
      assert key in self._slots, 'Unexpected key %s in %s' % (key, self._slots)

  def __setattr__(self, name, val):
    """Setting an attribute, actually sets a dictionary value."""
    if name not in self._slots:
      raise AttributeError("'%s' may not have attribute '%s'" %
                           (self.__class__.__name__, name))
    self[name] = val

  def __getattr__(self, name):
    """Fetching an attribute, actually fetches a dictionary value."""
    if name not in self:
      raise AttributeError("'%s' has no attribute '%s'" %
                           (self.__class__.__name__, name))
    return self[name]

  def __setitem__(self, name, val):
    """Restrict which keys can be stored in this dictionary."""
    if name not in self._slots:
      raise KeyError(name)
    dict.__setitem__(self, name, val)

  def __str__(self):
    """Default stringification behavior."""
    name = self._name if hasattr(self, '_name') else self.__class__.__name__
    return '%s (%s)' % (name, self._GetAttrString())

  def _GetAttrString(self, delim=', ', equal='='):
    """Return string showing all non-None values of self._slots.

    The ordering of attributes in self._slots is honored in string.

    Args:
      delim: String for separating key/value elements in result.
      equal: String to put between key and associated value in result.

    Returns:
      A string like "a='foo', b=12".
    """
    slots = [s for s in self._slots if self[s] is not None]
    elems = ['%s%s%r' % (s, equal, self[s]) for s in slots]
    return delim.join(elems)

  def _clear_if_default(self, key, default):
    """Helper for constructors.

    If they key value is set to the default value, set it to None.

    Args:
      key: Key value to check and possibly clear.
      default: Default value to compare the key value against.
    """
    if self[key] == default:
      self[key] = None


def PathPrepend(new_dir, curr_path=None):
  """Prepends a directory to a given path (or system path, if none provided)."""
  if curr_path is None:
    curr_path = os.environ.get('PATH')
  return '%s:%s' % (new_dir, curr_path) if curr_path else new_dir


@contextmanager
def CheckedOpen(name, mode=None, buffering=None):
  """A context for opening/closing a file iff an actual name is provided."""
  # Open the file, as necessary.
  f = None
  if name:
    dargs = {'name': name}
    if mode is not None:
      dargs['mode'] = mode
    if buffering is not None:
      dargs['buffering'] = buffering
    f = open(**dargs)

  try:
    # Yield to the wait-statement body.
    yield f
  finally:
    # If an actual file was opened, close it.
    if f:
      f.close()


class ThreadError(object):
  """Thread execution error as returned by traceback.format_exc()."""

  def __init__(self, exc):
    self.exc = exc

  def __str__(self):
    return self.exc


def _RunInThread(func, args, star, idx, result_queue):
  """Runs a function, queuing its return value / exception.

  If the function returns a value, this will push (idx, output) to the result
  queue; otherwise, an exception will be caught and pushed to the queue.

  Args:
    func: A function to execute.
    args: Arguments (possibly a tuple) to feed to the function.
    star: Whether the arguments need to be expanded (starred) when calling.
    idx: An index to attach to the result.
    result_queue: A queue to push the result onto.
  """
  try:
    ret = func(*args) if star else func(args)

  # We want to catch all exceptions so we can queue them back to the caller.
  # pylint: disable=W0703
  except Exception:
    ret = ThreadError(traceback.format_exc())

  result_queue.put((idx, ret))


class ThreadedMapError(Exception):
  """An error during execution of a threaded map."""

  def __init__(self, thread_errors, *args, **kwargs):
    """Intitializer for ThreadedMapError.

    Args:
      thread_errors: A list of pairs (idx, error), where idx is the thread
        index in the map and error is an instance of ThreadError for it.
    """
    super(ThreadedMapError, self).__init__(*args, **kwargs)
    self.thread_errors = thread_errors

  def __str__(self):
    return ('Some threads failed during threaded map:\n%s' %
            '\n'.join([self._FormatThreadError(idx, error)
                       for idx, error in self.thread_errors]))

  @classmethod
  def _FormatThreadError(cls, idx, error):
    """Format the traceback occurring within a single thread."""
    return 'Thread %d:\n%s' % (idx, cls._AddIndent(str(error), 2))

  @staticmethod
  def _AddIndent(text, indent):
    """Prepend an indent (spaces) to all lines of a given text."""
    return '\n'.join([' ' * indent + line for line in text.splitlines()])


def ThreadedMap(func, args_list, star=False):
  """Executes a function over a list of arguments in multiple threads.

  This will apply the function to each of the given values in parallel,
  spawning a new thread for each call. If star is True, each argument is
  assumed to be a tuple and will be expanded (starred) when calling the
  function.

  Args:
    func: A function to apply to each tuple of arguments.
    args_list: Arguments to feed to the function.
    star: Whether the argument to the function should be starred.

  Returns:
    A list of outputs corresponding to each input, in order.

  Raises:
    ThreadedMapError: If execution in one or more threads raised an exception.
  """
  result_queue = Queue.Queue()
  num_results = len(args_list)
  logging.debug('Threaded mapping over %d inputs', num_results)

  for i, args in enumerate(args_list):
    t = threading.Thread(target=_RunInThread,
                         args=(func, args, star, i, result_queue))
    t.daemon = True
    t.start()

  results = [None] * num_results
  logging.debug('Collecting results from threaded runs')
  thread_errors = []
  while num_results > 0:
    idx, result = result_queue.get()
    if isinstance(result, ThreadError):
      thread_errors.append((idx, result))
    else:
      results[idx] = result

    num_results -= 1

  if thread_errors:
    raise ThreadedMapError(thread_errors)

  return results


def Group(items, key_func):
  """Groups items based on their key.

  Note that this only associate subsequences of consecutive items of the same
  key. If a caller requires that all items bearing the same key be grouped
  together, it is their responsibility to sort the input by key prior to
  calling this function.

  Args:
    items: Iterable of values.
    key_func: A function that returns the key of each item.

  Returns:
    A list of pairs (key, vals), where vals is a list of consecutive items
    whose key_func() equals key.
  """
  return [(k, list(v)) for k, v in itertools.groupby(items, key_func)]


def Linear(score, score_min, score_max, val_start, val_end):
  """Computes a value as a linear function of a score within given bounds.

  This computes the linear growth/decay of a value based on a given score.
  Roughly speaking:

    ret = val_start + C * (score - score_min)

  where

    C = (val_end - val_start) / (score_max - score_min)

  Note that score_min/max are used as lower/upper thresholds, determining the
  range of scores that actually have impact on the returned value. Also note
  that val_start/end may be arbitrarily related, for example it may be that
  val_start > val_end, in which case the result will be a linearly decaying
  function.  The result is undefined (and may raise an exception) if
  score_min >= score_max.  Provided all arguments are integers, this guarantees
  that all arithmetic operations, intermediate values, and returned result are
  integers as well.

  Args:
    score: A number that determines the linear factor.
    score_min: The lowest score to consider.
    score_max: The highest score to consider.
    val_start: The return value when score <= score_min.
    val_end: The return value when score >= score_max.

  Returns:
    An integer value ranging between val_start and val_end.
  """
  relative_score = max(min(score, score_max), score_min) - score_min
  score_range = score_max - score_min
  val_range = val_end - val_start
  return val_start + ((val_range * relative_score) / score_range)


def TimeDeltaToString(delta, force_seconds=False, subsecond_precision=0):
  """Returns a readable string representation of a datetime.timedelta.

  The returned value takes the form 5d2h37m5.347s. Normally, this will only
  show seconds (and fraction thereof) if the delta is under 1 minute long,
  unless force_seconds is True. Subsecond fractions are shown up to the
  provided precision and without trailing zeros. Bear in mind that Python's
  timedelta is only accurate to the microseconds, so specifying precision
  beyond 6 will have no effect on the output.

  Args:
    delta: A datetime.timedelta object.
    force_seconds: Whether to show seconds unconditionally.
    subsecond_precision: Number of digits after the decimal point (0-6).

  Returns:
    A string representation of the time delta.
  """
  delta_str = ''

  if delta.days > 0:
    delta_str += '%dd' % delta.days
    delta -= datetime.timedelta(days=delta.days)

  delta_hours = delta.seconds / 3600
  if delta_hours > 0:
    delta_str += '%dh' % delta_hours
    delta -= datetime.timedelta(hours=delta_hours)

  delta_minutes = delta.seconds / 60
  if delta_minutes > 0:
    delta_str += '%dm' % delta_minutes
    delta -= datetime.timedelta(minutes=delta_minutes)

  if not delta_str or force_seconds:
    delta_str += '%d' % delta.seconds
    subseconds = delta.microseconds / (10 ** (6 - subsecond_precision))
    subsecond_str = ('%0*d' % (subsecond_precision, subseconds)).rstrip('0')
    if subsecond_str:
      delta_str += '.%s' % subsecond_str
    delta_str += 's'

  return delta_str
