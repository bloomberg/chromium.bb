# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

from __future__ import print_function

import base64
import contextlib
from datetime import datetime
import email.utils
import errno
import functools
import getpass
import inspect
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time

import six

from chromite.lib import constants
from chromite.lib import cros_collections
from chromite.lib import cros_logging as logging
from chromite.lib import signals


STRICT_SUDO = False

# For use by ShellQuote.  Match all characters that the shell might treat
# specially.  This means a number of things:
#  - Reserved characters.
#  - Characters used in expansions (brace, variable, path, globs, etc...).
#  - Characters that an interactive shell might use (like !).
#  - Whitespace so that one arg turns into multiple.
# See the bash man page as well as the POSIX shell documentation for more info:
#   http://www.gnu.org/software/bash/manual/bashref.html
#   http://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
_SHELL_QUOTABLE_CHARS = frozenset('[|&;()<> \t!{}[]=*?~$"\'\\#^')
# The chars that, when used inside of double quotes, need escaping.
# Order here matters as we need to escape backslashes first.
_SHELL_ESCAPE_CHARS = r'\"`$'

# The number of files is larger than this, we will use -T option
# and files to be added may not show up to the command line.
_THRESHOLD_TO_USE_T_FOR_TAR = 50


def ShellQuote(s):
  """Quote |s| in a way that is safe for use in a shell.

  We aim to be safe, but also to produce "nice" output.  That means we don't
  use quotes when we don't need to, and we prefer to use less quotes (like
  putting it all in single quotes) than more (using double quotes and escaping
  a bunch of stuff, or mixing the quotes).

  While python does provide a number of alternatives like:
   - pipes.quote
   - shlex.quote
  They suffer from various problems like:
   - Not widely available in different python versions.
   - Do not produce pretty output in many cases.
   - Are in modules that rarely otherwise get used.

  Note: We don't handle reserved shell words like "for" or "case".  This is
  because those only matter when they're the first element in a command, and
  there is no use case for that.  When we want to run commands, we tend to
  run real programs and not shell ones.

  Args:
    s: The string to quote.

  Returns:
    A safely (possibly quoted) string.
  """
  if sys.version_info.major < 3:
    # This is a bit of a hack.  Python 2 will display strings with u prefixes
    # when logging which makes things harder to work with.  Writing bytes to
    # stdout will be interpreted as UTF-8 content implicitly.
    if isinstance(s, six.string_types):
      try:
        s = s.encode('utf-8')
      except UnicodeDecodeError:
        # We tried our best.  Let Python's automatic mixed encoding kick in.
        pass
    else:
      return repr(s)
  else:
    # If callers pass down bad types, don't blow up.
    if isinstance(s, six.binary_type):
      s = s.decode('utf-8', 'backslashreplace')
    elif not isinstance(s, six.string_types):
      return repr(s)

  # See if no quoting is needed so we can return the string as-is.
  for c in s:
    if c in _SHELL_QUOTABLE_CHARS:
      break
  else:
    if not s:
      return "''"
    else:
      return s

  # See if we can use single quotes first.  Output is nicer.
  if "'" not in s:
    return "'%s'" % s

  # Have to use double quotes.  Escape the few chars that still expand when
  # used inside of double quotes.
  for c in _SHELL_ESCAPE_CHARS:
    if c in s:
      s = s.replace(c, r'\%s' % c)
  return '"%s"' % s


def TruncateStringToLine(s, maxlen=80):
  """Truncate |s| to a maximum length of |maxlen| including elipsis (...)

  Args:
    s: A string.
    maxlen: Maximum length of desired returned string. Must be at least 3.

  Returns:
    s if len(s) <= maxlen already and s has no newline in it.
    Otherwise, a single line truncation that ends with '...' and is of
    length |maxlen|.
  """
  assert maxlen >= 3
  line = s.splitlines()[0]
  if len(line) <= maxlen:
    return line
  else:
    return line[:maxlen-3] + '...'


def ShellUnquote(s):
  """Do the opposite of ShellQuote.

  This function assumes that the input is a valid escaped string. The behaviour
  is undefined on malformed strings.

  Args:
    s: An escaped string.

  Returns:
    The unescaped version of the string.
  """
  if not s:
    return ''

  if s[0] == "'":
    return s[1:-1]

  if s[0] != '"':
    return s

  s = s[1:-1]
  output = ''
  i = 0
  while i < len(s) - 1:
    # Skip the backslash when it makes sense.
    if s[i] == '\\' and s[i + 1] in _SHELL_ESCAPE_CHARS:
      i += 1
    output += s[i]
    i += 1
  return output + s[i] if i < len(s) else output


def CmdToStr(cmd):
  """Translate a command list into a space-separated string.

  The resulting string should be suitable for logging messages and for
  pasting into a terminal to run.  Command arguments are surrounded by
  quotes to keep them grouped, even if an argument has spaces in it.

  Examples:
    ['a', 'b'] ==> "'a' 'b'"
    ['a b', 'c'] ==> "'a b' 'c'"
    ['a', 'b\'c'] ==> '\'a\' "b\'c"'
    [u'a', "/'$b"] ==> '\'a\' "/\'$b"'
    [] ==> ''
    See unittest for additional (tested) examples.

  Args:
    cmd: List of command arguments.

  Returns:
    String representing full command.
  """
  # If callers pass down bad types, triage it a bit.
  if isinstance(cmd, (list, tuple)):
    return ' '.join(ShellQuote(arg) for arg in cmd)
  else:
    raise ValueError('cmd must be list or tuple, not %s: %r' %
                     (type(cmd), repr(cmd)))


class CommandResult(object):
  """An object to store various attributes of a child process.

  This is akin to subprocess.CompletedProcess.
  """

  # TODO(crbug.com/1006587): Drop redundant arguments & backwards compat APIs.
  def __init__(self, cmd=None, error=None, output=None, returncode=None,
               args=None, stdout=None, stderr=None):
    if args is None:
      args = cmd
    elif cmd is not None:
      raise TypeError('Only specify |args|, not |cmd|')
    if stdout is None:
      stdout = output
    elif output is not None:
      raise TypeError('Only specify |stdout|, not |output|')
    if stderr is None:
      stderr = error
    elif error is not None:
      raise TypeError('Only specify |stderr|, not |error|')

    self.args = args
    self.stdout = stdout
    self.stderr = stderr
    self.returncode = returncode

  @property
  def cmd(self):
    """Backwards compat API."""
    return self.args

  @property
  def output(self):
    """Backwards compat API."""
    return self.stdout

  @property
  def error(self):
    """Backwards compat API."""
    return self.stderr

  @property
  def cmdstr(self):
    """Return self.cmd as a space-separated string, useful for log messages."""
    if self.args is None:
      return ''
    else:
      return CmdToStr(self.args)

  def check_returncode(self):
    """Raise RunCommandError if the exit code is non-zero."""
    if self.returncode:
      raise RunCommandError('check_returncode failed', result=self)


class RunCommandError(Exception):
  """Error caught in run() method.

  Attributes:
    args: Tuple of the attributes below.
    msg: Short explanation of the error.
    result: The CommandResult that triggered this error, if available.
    exception: The underlying Exception if available.
  """

  def __init__(self, msg, result=None, exception=None):
    if exception is not None and not isinstance(exception, Exception):
      raise TypeError('exception must be an exception instance; got %r'
                      % (exception,))

    # This makes mocking tests easier.
    if result is None:
      result = CommandResult()
    elif not isinstance(result, CommandResult):
      raise TypeError('result must be a CommandResult instance; got %r'
                      % (result,))

    self.msg, self.result, self.exception = msg, result, exception
    super(RunCommandError, self).__init__(msg, result, exception)

  def Stringify(self, error=True, output=True):
    """Custom method for controlling what is included in stringifying this.

    Each individual argument is the literal name of an attribute
    on the result object; if False, that value is ignored for adding
    to this string content.  If true, it'll be incorporated.

    Args:
      error: See comment about individual arguments above.
      output: See comment about individual arguments above.

    Returns:
      A summary string for this result.
    """
    items = [
        u'return code: %s; command: %s' % (
            self.result.returncode, self.result.cmdstr),
    ]
    if error and self.result.stderr:
      stderr = self.result.stderr
      if isinstance(stderr, six.binary_type):
        stderr = stderr.decode('utf-8', 'replace')
      items.append(stderr)
    if output and self.result.stdout:
      stdout = self.result.stdout
      if isinstance(stdout, six.binary_type):
        stdout = stdout.decode('utf-8', 'replace')
      items.append(stdout)
    if self.msg:
      msg = self.msg
      if isinstance(msg, six.binary_type):
        msg = msg.decode('utf-8', 'replace')
      items.append(msg)
    return u'\n'.join(items)

  def __str__(self):
    if sys.version_info.major < 3:
      # __str__ needs to return ascii, thus force a conversion to be safe.
      return self.Stringify().encode('ascii', 'xmlcharrefreplace')
    else:
      return self.Stringify()

  def __eq__(self, other):
    return (isinstance(other, type(self)) and
            self.args == other.args)

  def __ne__(self, other):
    return not self.__eq__(other)


class TerminateRunCommandError(RunCommandError):
  """We were signaled to shutdown while running a command.

  Client code shouldn't generally know, nor care about this class.  It's
  used internally to suppress retry attempts when we're signaled to die.
  """


def sudo_run(cmd, user='root', preserve_env=False, **kwargs):
  """Run a command via sudo.

  Client code must use this rather than coming up with their own run
  invocation that jams sudo in- this function is used to enforce certain
  rules in our code about sudo usage, and as a potential auditing point.

  Args:
    cmd: The command to run.  See run for rules of this argument: sudo_run
         purely prefixes it with sudo.
    user: The user to run the command as.
    preserve_env (bool): Whether to preserve the environment.
    kwargs: See run() options, it's a direct pass thru to it.
          Note that this supports a 'strict' keyword that defaults to True.
          If set to False, it'll suppress strict sudo behavior.

  Returns:
    See run documentation.

  Raises:
    This function may immediately raise RunCommandError if we're operating
    in a strict sudo context and the API is being misused.
    Barring that, see run's documentation: it can raise the same things run
    does.
  """
  sudo_cmd = ['sudo']

  strict = kwargs.pop('strict', True)

  if user == 'root' and os.geteuid() == 0:
    return run(cmd, **kwargs)

  if strict and STRICT_SUDO:
    if 'CROS_SUDO_KEEP_ALIVE' not in os.environ:
      raise RunCommandError(
          'We were invoked in a strict sudo non - interactive context, but no '
          'sudo keep alive daemon is running.  This is a bug in the code.',
          CommandResult(args=cmd, returncode=126))
    sudo_cmd += ['-n']

  if user != 'root':
    sudo_cmd += ['-u', user]

  if preserve_env:
    sudo_cmd += ['--preserve-env']

  # Pass these values down into the sudo environment, since sudo will
  # just strip them normally.
  extra_env = kwargs.pop('extra_env', None)
  extra_env = {} if extra_env is None else extra_env.copy()

  for var in constants.ENV_PASSTHRU:
    if var not in extra_env and var in os.environ:
      extra_env[var] = os.environ[var]

  sudo_cmd.extend('%s=%s' % (k, v) for k, v in extra_env.items())

  # Finally, block people from passing options to sudo.
  sudo_cmd.append('--')

  if isinstance(cmd, six.string_types):
    # We need to handle shell ourselves so the order is correct:
    #  $ sudo [sudo args] -- bash -c '[shell command]'
    # If we let run take care of it, we'd end up with:
    #  $ bash -c 'sudo [sudo args] -- [shell command]'
    shell = kwargs.pop('shell', False)
    if not shell:
      raise Exception('Cannot run a string command without a shell')
    sudo_cmd.extend(['/bin/bash', '-c', cmd])
  else:
    sudo_cmd.extend(cmd)

  return run(sudo_cmd, **kwargs)


def SudoRunCommand(cmd, **kwargs):
  """Backwards compat API."""
  return sudo_run(cmd, **kwargs)


def _KillChildProcess(proc, int_timeout, kill_timeout, cmd, original_handler,
                      signum, frame):
  """Used as a signal handler by run.

  This is internal to run.  No other code should use this.
  """
  if signum:
    # If we've been invoked because of a signal, ignore delivery of that signal
    # from this point forward.  The invoking context of _KillChildProcess
    # restores signal delivery to what it was prior; we suppress future delivery
    # till then since this code handles SIGINT/SIGTERM fully including
    # delivering the signal to the original handler on the way out.
    signal.signal(signum, signal.SIG_IGN)

  # Do not trust Popen's returncode alone; we can be invoked from contexts where
  # the Popen instance was created, but no process was generated.
  if proc.returncode is None and proc.pid is not None:
    try:
      while proc.poll() is None and int_timeout >= 0:
        time.sleep(0.1)
        int_timeout -= 0.1

      proc.terminate()
      while proc.poll() is None and kill_timeout >= 0:
        time.sleep(0.1)
        kill_timeout -= 0.1

      if proc.poll() is None:
        # Still doesn't want to die.  Too bad, so sad, time to die.
        proc.kill()
    except EnvironmentError as e:
      logging.warning('Ignoring unhandled exception in _KillChildProcess: %s',
                      e)

    # Ensure our child process has been reaped.
    proc.wait()

  if not signals.RelaySignal(original_handler, signum, frame):
    # Mock up our own, matching exit code for signaling.
    cmd_result = CommandResult(args=cmd, returncode=signum << 8)
    raise TerminateRunCommandError('Received signal %i' % signum, cmd_result)


class _Popen(subprocess.Popen):
  """subprocess.Popen derivative customized for our usage.

  Specifically, we fix terminate/send_signal/kill to work if the child process
  was a setuid binary; on vanilla kernels, the parent can wax the child
  regardless, on goobuntu this apparently isn't allowed, thus we fall back
  to the sudo machinery we have.

  While we're overriding send_signal, we also suppress ESRCH being raised
  if the process has exited, and suppress signaling all together if the process
  has knowingly been waitpid'd already.
  """

  # Pylint seems to be buggy with the send_signal signature detection.
  # pylint: disable=arguments-differ
  def send_signal(self, sig):
    if self.returncode is not None:
      # The original implementation in Popen would allow signaling whatever
      # process now occupies this pid, even if the Popen object had waitpid'd.
      # Since we can escalate to sudo kill, we do not want to allow that.
      # Fixing this addresses that angle, and makes the API less sucky in the
      # process.
      return

    try:
      os.kill(self.pid, sig)
    except EnvironmentError as e:
      if e.errno == errno.EPERM:
        # Kill returns either 0 (signal delivered), or 1 (signal wasn't
        # delivered).  This isn't particularly informative, but we still
        # need that info to decide what to do, thus the check=False.
        ret = sudo_run(['kill', '-%i' % sig, str(self.pid)],
                       print_cmd=False, redirect_stdout=True,
                       redirect_stderr=True, check=False)
        if ret.returncode == 1:
          # The kill binary doesn't distinguish between permission denied,
          # and the pid is missing.  Denied can only occur under weird
          # grsec/selinux policies.  We ignore that potential and just
          # assume the pid was already dead and try to reap it.
          self.poll()
      elif e.errno == errno.ESRCH:
        # Since we know the process is dead, reap it now.
        # Normally Popen would throw this error- we suppress it since frankly
        # that's a misfeature and we're already overriding this method.
        self.poll()
      else:
        raise


# pylint: disable=redefined-builtin
def run(cmd, print_cmd=True, redirect_stdout=False,
        redirect_stderr=False, cwd=None, input=None, enter_chroot=False,
        shell=False, env=None, extra_env=None, ignore_sigint=False,
        combine_stdout_stderr=False, log_stdout_to_file=None,
        append_to_file=False, chroot_args=None, debug_level=logging.INFO,
        check=True, int_timeout=1, kill_timeout=1,
        log_output=False, capture_output=False,
        quiet=False, mute_output=None, encoding=None, errors=None, **kwargs):
  """Runs a command.

  Args:
    cmd: cmd to run.  Should be input to subprocess.Popen. If a string, shell
      must be true. Otherwise the command must be an array of arguments, and
      shell must be false.
    print_cmd: prints the command before running it.
    redirect_stdout: returns the stdout.
    redirect_stderr: holds stderr output until input is communicated.
    cwd: the working directory to run this cmd.
    input: The data to pipe into this command through stdin.  If a file object
      or file descriptor, stdin will be connected directly to that.
    enter_chroot: this command should be run from within the chroot.  If set,
      cwd must point to the scripts directory. If we are already inside the
      chroot, this command will be run as if |enter_chroot| is False.
    shell: Controls whether we add a shell as a command interpreter.  See cmd
      since it has to agree as to the type.
    env: If non-None, this is the environment for the new process.  If
      enter_chroot is true then this is the environment of the enter_chroot,
      most of which gets removed from the cmd run.
    extra_env: If set, this is added to the environment for the new process.
      In enter_chroot=True case, these are specified on the post-entry
      side, and so are often more useful.  This dictionary is not used to
      clear any entries though.
    ignore_sigint: If True, we'll ignore signal.SIGINT before calling the
      child.  This is the desired behavior if we know our child will handle
      Ctrl-C.  If we don't do this, I think we and the child will both get
      Ctrl-C at the same time, which means we'll forcefully kill the child.
    combine_stdout_stderr: Combines stdout and stderr streams into stdout.
    log_stdout_to_file: If set, redirects stdout to file specified by this path.
      If |combine_stdout_stderr| is set to True, then stderr will also be logged
      to the specified file.
    append_to_file: If True, the stdout streams are appended to the end of log
      stdout_to_file.
    chroot_args: An array of arguments for the chroot environment wrapper.
    debug_level: The debug level of run's output.
    check: Whether to raise an exception when command returns a non-zero exit
      code, or return the CommandResult object containing the exit code.
      Note: will still raise an exception if the cmd file does not exist.
    int_timeout: If we're interrupted, how long (in seconds) should we give the
      invoked process to clean up before we send a SIGTERM.
    kill_timeout: If we're interrupted, how long (in seconds) should we give the
      invoked process to shutdown from a SIGTERM before we SIGKILL it.
    log_output: Log the command and its output automatically.
    capture_output: Set |redirect_stdout| and |redirect_stderr| to True.
    quiet: Set |print_cmd| to False, |redirect_stdout| to True, and
      |combine_stdout_stderr| to True.
    mute_output: Mute subprocess printing to parent stdout/stderr. Defaults to
      None, which bases muting on |debug_level|.
    encoding: Encoding for stdin/stdout/stderr, otherwise bytes are used.  Most
      users want 'utf-8' here for string data.
    errors: How to handle errors when |encoding| is used.  Defaults to 'strict',
      but 'ignore' and 'replace' are common settings.

  Returns:
    A CommandResult object.

  Raises:
    RunCommandError: Raised on error.
  """
  if 'error_code_ok' in kwargs:
    # TODO(vapier): Enable this warning once chromite & users migrate.
    # logging.warning('run: error_code_ok= is renamed/inverted to check=')
    check = not kwargs.pop('error_code_ok')
  assert not kwargs, 'Unknown arguments to run: %s' % (list(kwargs),)

  if capture_output:
    redirect_stdout, redirect_stderr = True, True

  stdout_to_pipe = False
  if quiet:
    debug_level = logging.DEBUG
    stdout_to_pipe, combine_stdout_stderr = True, True

  if encoding is not None and errors is None:
    errors = 'strict'

  # Set default for variables.
  popen_stdout = None
  popen_stderr = None
  stdin = None
  cmd_result = CommandResult()

  if mute_output is None:
    mute_output = logging.getLogger().getEffectiveLevel() > debug_level

  # Force the timeout to float; in the process, if it's not convertible,
  # a self-explanatory exception will be thrown.
  kill_timeout = float(kill_timeout)

  def _get_tempfile():
    try:
      return UnbufferedTemporaryFile()
    except EnvironmentError as e:
      if e.errno != errno.ENOENT:
        raise
      # This can occur if we were pointed at a specific location for our
      # TMP, but that location has since been deleted.  Suppress that issue
      # in this particular case since our usage gurantees deletion,
      # and since this is primarily triggered during hard cgroups shutdown.
      return UnbufferedTemporaryFile(dir='/tmp')

  # Modify defaults based on parameters.
  # Note that tempfiles must be unbuffered else attempts to read
  # what a separate process did to that file can result in a bad
  # view of the file.
  if log_stdout_to_file:
    if append_to_file:
      popen_stdout = open(log_stdout_to_file, 'a+')
    else:
      popen_stdout = open(log_stdout_to_file, 'w+')
  elif stdout_to_pipe:
    popen_stdout = subprocess.PIPE
  elif redirect_stdout or mute_output or log_output:
    popen_stdout = _get_tempfile()

  if combine_stdout_stderr:
    popen_stderr = subprocess.STDOUT
  elif redirect_stderr or mute_output or log_output:
    popen_stderr = _get_tempfile()

  # If subprocesses have direct access to stdout or stderr, they can bypass
  # our buffers, so we need to flush to ensure that output is not interleaved.
  if popen_stdout is None or popen_stderr is None:
    sys.stdout.flush()
    sys.stderr.flush()

  # If input is a string, we'll create a pipe and send it through that.
  # Otherwise we assume it's a file object that can be read from directly.
  if isinstance(input, (six.string_types, six.binary_type)):
    stdin = subprocess.PIPE
    # Allow people to always pass in bytes or strings regardless of encoding.
    # Our Popen usage takes care of converting everything to bytes first.
    #
    # Linter can't see that we're using |input| as a var, not a builtin.
    # pylint: disable=input-builtin
    if encoding and isinstance(input, six.text_type):
      input = input.encode(encoding, errors)
    elif not encoding and isinstance(input, six.text_type):
      input = input.encode('utf-8')
  elif input is not None:
    stdin = input
    input = None

  # Sanity check the command.  This helps when RunCommand is deep in the call
  # chain, but the command itself was constructed along the way.
  if isinstance(cmd, (six.string_types, six.binary_type)):
    if not shell:
      raise ValueError('Cannot run a string command without a shell')
    cmd = ['/bin/bash', '-c', cmd]
    shell = False
  elif shell:
    raise ValueError('Cannot run an array command with a shell')
  elif not cmd:
    raise ValueError('Missing command to run')
  elif not isinstance(cmd, (list, tuple)):
    raise TypeError('cmd must be list or tuple, not %s: %r' %
                    (type(cmd), repr(cmd)))
  elif not all(isinstance(x, (six.binary_type, six.string_types)) for x in cmd):
    raise TypeError('All command elements must be bytes/strings: %r' % (cmd,))

  # If we are using enter_chroot we need to use enterchroot pass env through
  # to the final command.
  env = env.copy() if env is not None else os.environ.copy()
  # Looking at localized error messages may be unexpectedly dangerous, so we
  # set LC_MESSAGES=C to make sure the output of commands is safe to inspect.
  env['LC_MESSAGES'] = 'C'
  env.update(extra_env if extra_env else {})

  if enter_chroot and not IsInsideChroot():
    wrapper = ['cros_sdk']
    if cwd:
      # If the current working directory is set, try to find cros_sdk relative
      # to cwd. Generally cwd will be the buildroot therefore we want to use
      # {cwd}/chromite/bin/cros_sdk. For more info PTAL at crbug.com/432620
      path = os.path.join(cwd, constants.CHROMITE_BIN_SUBDIR, 'cros_sdk')
      if os.path.exists(path):
        wrapper = [path]

    if chroot_args:
      wrapper += chroot_args

    if extra_env:
      wrapper.extend('%s=%s' % (k, v) for k, v in extra_env.items())

    cmd = wrapper + ['--'] + cmd

  for var in constants.ENV_PASSTHRU:
    if var not in env and var in os.environ:
      env[var] = os.environ[var]

  # Print out the command before running.
  if print_cmd or log_output:
    if cwd:
      logging.log(debug_level, 'run: %s in %s', CmdToStr(cmd), cwd)
    else:
      logging.log(debug_level, 'run: %s', CmdToStr(cmd))

  cmd_result.args = cmd

  proc = None
  # Verify that the signals modules is actually usable, and won't segfault
  # upon invocation of getsignal.  See signals.SignalModuleUsable for the
  # details and upstream python bug.
  use_signals = signals.SignalModuleUsable()
  try:
    proc = _Popen(cmd, cwd=cwd, stdin=stdin, stdout=popen_stdout,
                  stderr=popen_stderr, shell=False, env=env,
                  close_fds=True)

    if use_signals:
      if ignore_sigint:
        old_sigint = signal.signal(signal.SIGINT, signal.SIG_IGN)
      else:
        old_sigint = signal.getsignal(signal.SIGINT)
        signal.signal(signal.SIGINT,
                      functools.partial(_KillChildProcess, proc, int_timeout,
                                        kill_timeout, cmd, old_sigint))

      old_sigterm = signal.getsignal(signal.SIGTERM)
      signal.signal(signal.SIGTERM,
                    functools.partial(_KillChildProcess, proc, int_timeout,
                                      kill_timeout, cmd, old_sigterm))

    try:
      (cmd_result.stdout, cmd_result.stderr) = proc.communicate(input)
    finally:
      if use_signals:
        signal.signal(signal.SIGINT, old_sigint)
        signal.signal(signal.SIGTERM, old_sigterm)

      if popen_stdout and not log_stdout_to_file and not stdout_to_pipe:
        popen_stdout.seek(0)
        cmd_result.stdout = popen_stdout.read()
        popen_stdout.close()
      elif log_stdout_to_file:
        popen_stdout.close()

      if popen_stderr and popen_stderr != subprocess.STDOUT:
        popen_stderr.seek(0)
        cmd_result.stderr = popen_stderr.read()
        popen_stderr.close()

    cmd_result.returncode = proc.returncode

    # The try/finally block is a bit hairy.  We normally want the logged
    # output to be what gets passed back up.  But if there's a decode error,
    # we don't want it to break logging entirely.  If the output had a lot of
    # newlines, always logging it as bytes wouldn't be human readable.
    try:
      if encoding:
        if cmd_result.stdout is not None:
          cmd_result.stdout = cmd_result.stdout.decode(encoding, errors)
        if cmd_result.stderr is not None:
          cmd_result.stderr = cmd_result.stderr.decode(encoding, errors)
    finally:
      if log_output:
        if cmd_result.stdout:
          logging.log(debug_level, '(stdout):\n%s', cmd_result.stdout)
        if cmd_result.stderr:
          logging.log(debug_level, '(stderr):\n%s', cmd_result.stderr)

    if check and proc.returncode:
      msg = 'cmd=%s' % cmd
      if cwd:
        msg += ', cwd=%s' % cwd
      if extra_env:
        msg += ', extra env=%s' % extra_env
      raise RunCommandError(msg, cmd_result)
  except OSError as e:
    estr = str(e)
    if e.errno == errno.EACCES:
      estr += '; does the program need `chmod a+x`?'
    raise RunCommandError(estr, CommandResult(args=cmd), exception=e)
  finally:
    if proc is not None:
      # Ensure the process is dead.
      _KillChildProcess(proc, int_timeout, kill_timeout, cmd, None, None, None)

  return cmd_result
# pylint: enable=redefined-builtin


def RunCommand(cmd, **kwargs):
  """Backwards compat API."""
  return run(cmd, **kwargs)


# Convenience run methods.
#
# We don't use functools.partial because it binds the methods at import time,
# which doesn't work well with unit tests, since it bypasses the mock that may
# be set up for run.

def dbg_run(*args, **kwargs):
  kwargs.setdefault('debug_level', logging.DEBUG)
  return run(*args, **kwargs)


class DieSystemExit(SystemExit):
  """Custom Exception used so we can intercept this if necessary."""


def Die(message, *args, **kwargs):
  """Emits an error message with a stack trace and halts execution.

  Args:
    message: The message to be emitted before exiting.
  """
  logging.error(message, *args, **kwargs)
  raise DieSystemExit(1)


def GetSysrootToolPath(sysroot, tool_name):
  """Returns the path to the sysroot specific version of a tool.

  Does not check that the tool actually exists.

  Args:
    sysroot: build root of the system in question.
    tool_name: string name of tool desired (e.g. 'equery').

  Returns:
    string path to tool inside the sysroot.
  """
  if sysroot == '/':
    return os.path.join(sysroot, 'usr', 'bin', tool_name)

  return os.path.join(sysroot, 'build', 'bin', tool_name)


def IsInsideChroot():
  """Returns True if we are inside chroot."""
  return os.path.exists('/etc/cros_chroot_version')


def IsOutsideChroot():
  """Returns True if we are outside chroot."""
  return not IsInsideChroot()


def AssertInsideChroot():
  """Die if we are outside the chroot"""
  if not IsInsideChroot():
    Die('%s: please run inside the chroot', os.path.basename(sys.argv[0]))


def AssertOutsideChroot():
  """Die if we are inside the chroot"""
  if IsInsideChroot():
    Die('%s: please run outside the chroot', os.path.basename(sys.argv[0]))


def GetHostName(fully_qualified=False):
  """Return hostname of current machine, with domain if |fully_qualified|."""
  hostname = socket.gethostname()
  try:
    hostname = socket.gethostbyaddr(hostname)[0]
  except (socket.gaierror, socket.herror) as e:
    logging.warning('please check your /etc/hosts file; resolving your hostname'
                    ' (%s) failed: %s', hostname, e)

  if fully_qualified:
    return hostname
  else:
    return hostname.partition('.')[0]


def GetHostDomain():
  """Return domain of current machine.

  If there is no domain, return 'localdomain'.
  """

  hostname = GetHostName(fully_qualified=True)
  domain = hostname.partition('.')[2]
  return domain if domain else 'localdomain'


def HostIsCIBuilder(fq_hostname=None, golo_only=False, gce_only=False):
  """Return True iff a host is a continuous-integration builder.

  Args:
    fq_hostname: The fully qualified hostname. By default, we fetch it for you.
    golo_only: Only return True if the host is in the Chrome Golo. Defaults to
      False.
    gce_only: Only return True if the host is in the Chrome GCE block. Defaults
      to False.
  """
  if not fq_hostname:
    fq_hostname = GetHostName(fully_qualified=True)
  in_golo = fq_hostname.endswith('.' + constants.GOLO_DOMAIN)
  in_gce = (fq_hostname.endswith('.' + constants.CHROME_DOMAIN) or
            fq_hostname.endswith('.' + constants.CHROMEOS_BOT_INTERNAL))
  if golo_only:
    return in_golo
  elif gce_only:
    return in_gce
  else:
    return in_golo or in_gce


COMP_NONE = 0
COMP_GZIP = 1
COMP_BZIP2 = 2
COMP_XZ = 3


def FindCompressor(compression, chroot=None):
  """Locate a compressor utility program (possibly in a chroot).

  Since we compress/decompress a lot, make it easy to locate a
  suitable utility program in a variety of locations.  We favor
  the one in the chroot over /, and the parallel implementation
  over the single threaded one.

  Args:
    compression: The type of compression desired.
    chroot: Optional path to a chroot to search.

  Returns:
    Path to a compressor.

  Raises:
    ValueError: If compression is unknown.
  """
  if compression == COMP_GZIP:
    std = 'gzip'
    para = 'pigz'
  elif compression == COMP_BZIP2:
    std = 'bzip2'
    para = 'pbzip2'
  elif compression == COMP_XZ:
    std = 'xz'
    para = 'pixz'
  elif compression == COMP_NONE:
    return 'cat'
  else:
    raise ValueError('unknown compression')

  roots = []
  if chroot:
    roots.append(chroot)
  roots.append('/')

  for prog in [para, std]:
    for root in roots:
      for subdir in ['', 'usr']:
        path = os.path.join(root, subdir, 'bin', prog)
        if os.path.exists(path):
          return path

  return std


def CompressionStrToType(s):
  """Convert a compression string type to a constant.

  Args:
    s: string to check

  Returns:
    A constant, or None if the compression type is unknown.
  """
  _COMP_STR = {
      'gz': COMP_GZIP,
      'bz2': COMP_BZIP2,
      'xz': COMP_XZ,
  }
  if s:
    return _COMP_STR.get(s)
  else:
    return COMP_NONE


def CompressionExtToType(file_name):
  """Retrieve a compression type constant from a compression file's name.

  Args:
    file_name: Name of a compression file.

  Returns:
    A constant, return COMP_NONE if the extension is unknown.
  """
  ext = os.path.splitext(file_name)[-1]
  _COMP_EXT = {
      '.tgz': COMP_GZIP,
      '.gz': COMP_GZIP,
      '.tbz2': COMP_BZIP2,
      '.bz2': COMP_BZIP2,
      '.txz': COMP_XZ,
      '.xz': COMP_XZ,
  }
  return _COMP_EXT.get(ext, COMP_NONE)


def CompressFile(infile, outfile):
  """Compress a file using compressor specified by |outfile| suffix.

  Args:
    infile: File to compress.
    outfile: Name of output file. Compression used is based on the
             type of suffix of the name specified (e.g.: .bz2).
  """
  comp_type = CompressionExtToType(outfile)
  assert comp_type and comp_type != COMP_NONE
  comp = FindCompressor(comp_type)
  if os.path.basename(comp) == 'pixz':
    # pixz does not accept '-c'; instead an explicit '-i' indicates input file
    # should not be deleted, and '-o' specifies output file.
    cmd = [comp, '-i', infile, '-o', outfile]
    run(cmd)
  else:
    cmd = [comp, '-c', infile]
    run(cmd, log_stdout_to_file=outfile)


def UncompressFile(infile, outfile):
  """Uncompress a file using compressor specified by |infile| suffix.

  Args:
    infile: File to uncompress. Compression used is based on the
            type of suffix of the name specified (e.g.: .bz2).
    outfile: Name of output file.
  """
  comp_type = CompressionExtToType(infile)
  assert comp_type and comp_type != COMP_NONE
  comp = FindCompressor(comp_type)
  if os.path.basename(comp) == 'pixz':
    # pixz does not accept '-c'; instead an explicit '-i' indicates input file
    # should not be deleted, and '-o' specifies output file.
    cmd = [comp, '-d', '-i', infile, '-o', outfile]
    run(cmd)
  else:
    cmd = [comp, '-dc', infile]
    run(cmd, log_stdout_to_file=outfile)


class CreateTarballError(RunCommandError):
  """Error while running tar.

  We may run tar multiple times because of "soft" errors.  The result is from
  the last run instance.
  """


def CreateTarball(target, cwd, sudo=False, compression=COMP_XZ, chroot=None,
                  inputs=None, timeout=300, extra_args=None, **kwargs):
  """Create a tarball.  Executes 'tar' on the commandline.

  Args:
    target: The path of the tar file to generate.
    cwd: The directory to run the tar command.
    sudo: Whether to run with "sudo".
    compression: The type of compression desired.  See the FindCompressor
      function for details.
    chroot: See FindCompressor().
    inputs: A list of files or directories to add to the tarball.  If unset,
      defaults to ".".
    timeout: The number of seconds to wait on soft failure.
    extra_args: A list of extra args to pass to "tar".
    kwargs: Any run options/overrides to use.

  Returns:
    The cmd_result object returned by the run invocation.

  Raises:
    CreateTarballError: if the tar command failed, possibly after retry.
  """
  if inputs is None:
    inputs = ['.']

  if extra_args is None:
    extra_args = []
  kwargs.setdefault('debug_level', logging.INFO)

  comp = FindCompressor(compression, chroot=chroot)
  cmd = (['tar'] +
         extra_args +
         ['--sparse', '-I', comp, '-cf', target])
  if len(inputs) > _THRESHOLD_TO_USE_T_FOR_TAR:
    cmd += ['--null', '-T', '/dev/stdin']
    rc_input = b'\0'.join(x.encode('utf-8') for x in inputs)
  else:
    cmd += list(inputs)
    rc_input = None

  rc_func = sudo_run if sudo else run

  # If tar fails with status 1, retry twice. Once after timeout seconds and
  # again 2*timeout seconds after that.
  for try_count in range(3):
    result = rc_func(cmd, cwd=cwd, **dict(kwargs, check=False, input=rc_input))
    if result.returncode == 0:
      return result
    if result.returncode != 1 or try_count > 1:
      # Since the build is abandoned at this point, we will take 5
      # entire minutes to track down the competing process.
      raise CreateTarballError('CreateTarball', result)

    assert result.returncode == 1
    time.sleep(timeout * (try_count + 1))
    logging.warning('CreateTarball: tar: source modification time changed ' +
                    '(see crbug.com/547055), retrying')
    logging.PrintBuildbotStepWarnings()


def GetInput(prompt):
  """Helper function to grab input from a user.   Makes testing easier."""
  # We have people use GetInput() so they don't have to use these bad builtins
  # themselves or deal with version skews.
  # pylint: disable=bad-builtin,input-builtin,raw_input-builtin
  if sys.version_info.major < 3:
    return raw_input(prompt)
  else:
    return input(prompt)


def GetChoice(title, options, group_size=0):
  """Ask user to choose an option from the list.

  When |group_size| is 0, then all items in |options| will be extracted and
  shown at the same time.  Otherwise, the items will be extracted |group_size|
  at a time, and then shown to the user.  This makes it easier to support
  generators that are slow, extremely large, or people usually want to pick
  from the first few choices.

  Args:
    title: The text to display before listing options.
    options: Iterable which provides options to display.
    group_size: How many options to show before asking the user to choose.

  Returns:
    An integer of the index in |options| the user picked.
  """
  def PromptForChoice(max_choice, more):
    prompt = 'Please choose an option [0-%d]' % max_choice
    if more:
      prompt += ' (Enter for more options)'
    prompt += ': '

    while True:
      choice = GetInput(prompt)
      if more and not choice.strip():
        return None
      try:
        choice = int(choice)
      except ValueError:
        print('Input is not an integer')
        continue
      if choice < 0 or choice > max_choice:
        print('Choice %d out of range (0-%d)' % (choice, max_choice))
        continue
      return choice

  print(title)
  max_choice = 0
  for i, opt in enumerate(options):
    if i and group_size and not i % group_size:
      choice = PromptForChoice(i - 1, True)
      if choice is not None:
        return choice
    print('  [%d]: %s' % (i, opt))
    max_choice = i

  return PromptForChoice(max_choice, False)


def BooleanPrompt(prompt='Do you want to continue?', default=True,
                  true_value='yes', false_value='no', prolog=None):
  """Helper function for processing boolean choice prompts.

  Args:
    prompt: The question to present to the user.
    default: Boolean to return if the user just presses enter.
    true_value: The text to display that represents a True returned.
    false_value: The text to display that represents a False returned.
    prolog: The text to display before prompt.

  Returns:
    True or False.
  """
  true_value, false_value = true_value.lower(), false_value.lower()
  true_text, false_text = true_value, false_value
  if true_value == false_value:
    raise ValueError('true_value and false_value must differ: got %r'
                     % true_value)

  if default:
    true_text = true_text[0].upper() + true_text[1:]
  else:
    false_text = false_text[0].upper() + false_text[1:]

  prompt = ('\n%s (%s/%s)? ' % (prompt, true_text, false_text))

  if prolog:
    prompt = ('\n%s\n%s' % (prolog, prompt))

  while True:
    try:
      response = GetInput(prompt).lower()
    except EOFError:
      # If the user hits CTRL+D, or stdin is disabled, use the default.
      print()
      response = None
    except KeyboardInterrupt:
      # If the user hits CTRL+C, just exit the process.
      print()
      Die('CTRL+C detected; exiting')

    if not response:
      return default
    if true_value.startswith(response):
      if not false_value.startswith(response):
        return True
      # common prefix between the two...
    elif false_value.startswith(response):
      return False


def BooleanShellValue(sval, default, msg=None):
  """See if the string value is a value users typically consider as boolean

  Often times people set shell variables to different values to mean "true"
  or "false".  For example, they can do:
    export FOO=yes
    export BLAH=1
    export MOO=true
  Handle all that user ugliness here.

  If the user picks an invalid value, you can use |msg| to display a non-fatal
  warning rather than raising an exception.

  Args:
    sval: The string value we got from the user.
    default: If we can't figure out if the value is true or false, use this.
    msg: If |sval| is an unknown value, use |msg| to warn the user that we
         could not decode the input.  Otherwise, raise ValueError().

  Returns:
    The interpreted boolean value of |sval|.

  Raises:
    ValueError() if |sval| is an unknown value and |msg| is not set.
  """
  if sval is None:
    return default

  if isinstance(sval, six.string_types):
    s = sval.lower()
    if s in ('yes', 'y', '1', 'true'):
      return True
    elif s in ('no', 'n', '0', 'false'):
      return False

  if msg is not None:
    logging.warning('%s: %r', msg, sval)
    return default
  else:
    raise ValueError('Could not decode as a boolean value: %r' % sval)


# Suppress whacked complaints about abstract class being unused.
class MasterPidContextManager(object):
  """Allow context managers to restrict their exit to within the same PID."""

  # In certain cases we actually want this ran outside
  # of the main pid- specifically in backup processes
  # doing cleanup.
  ALTERNATE_MASTER_PID = None

  def __init__(self):
    self._invoking_pid = None

  def __enter__(self):
    self._invoking_pid = os.getpid()
    return self._enter()

  def __exit__(self, exc_type, exc, exc_tb):
    curpid = os.getpid()
    if curpid == self.ALTERNATE_MASTER_PID:
      self._invoking_pid = curpid
    if curpid == self._invoking_pid:
      return self._exit(exc_type, exc, exc_tb)

  def _enter(self):
    raise NotImplementedError(self, '_enter')

  def _exit(self, exc_type, exc, exc_tb):
    raise NotImplementedError(self, '_exit')


@contextlib.contextmanager
def NoOpContextManager():
  yield


class ContextManagerStack(object):
  """Context manager that is designed to safely allow nesting and stacking.

  Python2.7 directly supports a with syntax generally removing the need for
  this, although this form avoids indentation hell if there is a lot of context
  managers.  It also permits more programmatic control and allowing conditional
  usage.

  For Python2.6, see http://docs.python.org/library/contextlib.html; the short
  version is that there is a race in the available stdlib/language rules under
  2.6 when dealing w/ multiple context managers, thus this safe version was
  added.

  For each context manager added to this instance, it will unwind them,
  invoking them as if it had been constructed as a set of manually nested
  with statements.
  """

  def __init__(self):
    self._stack = []

  def Add(self, functor, *args, **kwargs):
    """Add a context manager onto the stack.

    Usage of this is essentially the following:
    >>> stack.add(Timeout, 60)

    It must be done in this fashion, else there is a mild race that exists
    between context manager instantiation and initial __enter__.

    Invoking it in the form specified eliminates that race.

    Args:
      functor: A callable to instantiate a context manager.
      args and kwargs: positional and optional args to functor.

    Returns:
      The newly created (and __enter__'d) context manager.
      Note: This is not the same value as the "with" statement -- that returns
      the value from the __enter__ function while this is the manager itself.
    """
    obj = None
    try:
      obj = functor(*args, **kwargs)
      return obj
    finally:
      if obj is not None:
        obj.__enter__()
        self._stack.append(obj)

  def __enter__(self):
    # Nothing to do in this case.  The individual __enter__'s are done
    # when the context managers are added, which will likely be after
    # the __enter__ method of this stack is called.
    return self

  def __exit__(self, exc_type, exc, exc_tb):
    # Exit each context manager in stack in reverse order, tracking the results
    # to know whether or not to suppress the exception raised (or to switch that
    # exception to a new one triggered by an individual handler's __exit__).
    for handler in reversed(self._stack):
      # pylint: disable=bare-except
      try:
        if handler.__exit__(exc_type, exc, exc_tb):
          exc_type = exc = exc_tb = None
      except:
        exc_type, exc, exc_tb = sys.exc_info()

    self._stack = []

    # Return True if any exception was handled.
    if all(x is None for x in (exc_type, exc, exc_tb)):
      return True

    # Raise any exception that is left over from exiting all context managers.
    # Normally a single context manager would return False to allow caller to
    # re-raise the exception itself, but here the exception might have been
    # raised during the exiting of one of the individual context managers.
    six.reraise(exc_type, exc, exc_tb)


def iflatten_instance(iterable,
                      terminate_on_kls=(six.string_types, six.binary_type)):
  """Derivative of snakeoil.lists.iflatten_instance; flatten an object.

  Given an object, flatten it into a single depth iterable-
  stopping descent on objects that either aren't iterable, or match
  isinstance(obj, terminate_on_kls).

  Examples:
    >>> print list(iflatten_instance([1, 2, "as", ["4", 5]))
    [1, 2, "as", "4", 5]
  """
  def descend_into(item):
    if isinstance(item, terminate_on_kls):
      return False
    try:
      iter(item)
    except TypeError:
      return False
    # Note strings can be infinitely descended through- thus this
    # recursion limiter.
    return not isinstance(item, six.string_types) or len(item) > 1

  if not descend_into(iterable):
    yield iterable
    return
  for item in iterable:
    if not descend_into(item):
      yield item
    else:
      for subitem in iflatten_instance(item, terminate_on_kls):
        yield subitem


@contextlib.contextmanager
def Open(obj, mode='r'):
  """Convenience ctx that accepts a file path or an already open file object."""
  if isinstance(obj, six.string_types):
    with open(obj, mode=mode) as f:
      yield f
  else:
    yield obj


def SafeRun(functors, combine_exceptions=False):
  """Executes a list of functors, continuing on exceptions.

  Args:
    functors: An iterable of functors to call.
    combine_exceptions: If set, and multiple exceptions are encountered,
      SafeRun will raise a RuntimeError containing a list of all the exceptions.
      If only one exception is encountered, then the default behavior of
      re-raising the original exception with unmodified stack trace will be
      kept.

  Raises:
    The first exception encountered, with corresponding backtrace, unless
    |combine_exceptions| is specified and there is more than one exception
    encountered, in which case a RuntimeError containing a list of all the
    exceptions that were encountered is raised.
  """
  errors = []

  for f in functors:
    try:
      f()
    except Exception as e:
      # Append the exception object and the traceback.
      errors.append((e, sys.exc_info()[2]))

  if errors:
    if len(errors) == 1 or not combine_exceptions:
      # To preserve the traceback.
      inst, tb = errors[0]
      six.reraise(type(inst), inst, tb)
    else:
      raise RuntimeError([e[0] for e in errors])


def UserDateTimeFormat(timeval=None):
  """Format a date meant to be viewed by a user

  The focus here is to have a format that is easily readable by humans,
  but still easy (and unambiguous) for a machine to parse.  Hence, we
  use the RFC 2822 date format (with timezone name appended).

  Args:
    timeval: Either a datetime object or a floating point time value as accepted
             by gmtime()/localtime().  If None, the current time is used.

  Returns:
    A string format such as 'Wed, 20 Feb 2013 15:25:15 -0500 (EST)'
  """
  if isinstance(timeval, datetime):
    timeval = time.mktime(timeval.timetuple())
  return '%s (%s)' % (email.utils.formatdate(timeval=timeval, localtime=True),
                      time.strftime('%Z', time.localtime(timeval)))


def ParseUserDateTimeFormat(time_string):
  """Parse a time string into a floating point time value.

  This function is essentially the inverse of UserDateTimeFormat.

  Args:
    time_string: A string datetime represetation in RFC 2822 format, such as
                 'Wed, 20 Feb 2013 15:25:15 -0500 (EST)'.

  Returns:
    Floating point Unix timestamp (seconds since epoch).
  """
  return email.utils.mktime_tz(email.utils.parsedate_tz(time_string))


def GetDefaultBoard():
  """Gets the default board.

  Returns:
    The default board (as a string), or None if either the default board
    file was missing or malformed.
  """
  default_board_file_name = os.path.join(constants.SOURCE_ROOT, 'src',
                                         'scripts', '.default_board')
  try:
    with open(default_board_file_name) as default_board_file:
      default_board = default_board_file.read().strip()
      # Check for user typos like whitespace
      if not re.match('[a-zA-Z0-9-_]*$', default_board):
        logging.warning('Noticed invalid default board: |%s|. Ignoring this '
                        'default.', default_board)
        default_board = None
  except IOError:
    return None

  return default_board


def SetDefaultBoard(board):
  """Set the default board.

  Args:
    board (str): The name of the board to save as the default.

  Returns:
    bool - True if successfully wrote default, False otherwise.
  """
  config_path = os.path.join(constants.CROSUTILS_DIR, '.default_board')
  try:
    with open(config_path, 'w') as f:
      f.write(board)
  except IOError as e:
    logging.error('Unable to write default board: %s', e)
    return False

  return True


def GetBoard(device_board, override_board=None, force=False, strict=False):
  """Gets the board name to use.

  Ask user to confirm when |override_board| and |device_board| are
  both None.

  Args:
    device_board: The board detected on the device.
    override_board: Overrides the board.
    force: Force using the default board if |device_board| is None.
    strict: If True, abort if no valid board can be found.

  Returns:
    Returns the first non-None board in the following order:
    |override_board|, |device_board|, and GetDefaultBoard().

  Raises:
    DieSystemExit: If board is not set or user enters no.
  """
  if override_board:
    return override_board

  board = device_board or GetDefaultBoard()
  if not device_board:
    if not board and strict:
      Die('No board specified and no default board found.')
    msg = 'Cannot detect board name; using default board %s.' % board
    if not force and not BooleanPrompt(default=False, prolog=msg):
      Die('Exiting...')

    logging.warning(msg)

  return board


def GetSysroot(board=None):
  """Returns the sysroot for |board| or '/' if |board| is None."""
  return '/' if board is None else os.path.join('/build', board)


# Structure to hold the values produced by TimedSection.
#
#  Attributes:
#    start: The absolute start time as a datetime.
#    finish: The absolute finish time as a datetime, or None if in progress.
#    delta: The runtime as a timedelta, or None if in progress.
TimedResults = cros_collections.Collection(
    'TimedResults', start=None, finish=None, delta=None)


@contextlib.contextmanager
def TimedSection():
  """Context manager to time how long a code block takes.

  Examples:
    with cros_build_lib.TimedSection() as timer:
      DoWork()
    logging.info('DoWork took %s', timer.delta)

  Context manager value will be a TimedResults instance.
  """
  # Create our context manager value.
  times = TimedResults(start=datetime.now())
  try:
    yield times
  finally:
    times.finish = datetime.now()
    times.delta = times.finish - times.start


def GetRandomString():
  """Returns a random string.

  It will be 32 characters long, although callers shouldn't rely on this.
  Only lowercase & numbers are used to avoid case-insensitive collisions.
  """
  # Start with current time.  This "scopes" the following random data.
  stamp = b'%x' % int(time.time())
  # Add in some entropy.  This reads more bytes than strictly necessary, but
  # it guarantees that we always have enough bytes below.
  data = os.urandom(16)
  # Then convert it to a lowercase base32 string of 32 characters.
  return base64.b32encode(stamp + data).decode('utf-8')[0:32].lower()


def MachineDetails():
  """Returns a string to help identify the source of a job.

  This is not meant for machines to parse; instead, we want content that is easy
  for humans to read when trying to figure out where "something" is coming from.
  For example, when a service has grabbed a lock in Google Storage, and we want
  to see what process actually triggered that (in case it is a test gone rogue),
  the content in here should help triage.

  Note: none of the details included may be secret so they can be freely pasted
  into bug reports/chats/logs/etc...

  Note: this content should not be large

  Returns:
    A string with content that helps identify this system/process/etc...
  """
  return '\n'.join((
      'PROG=%s' % inspect.stack()[-1][1],
      'USER=%s' % getpass.getuser(),
      'HOSTNAME=%s' % GetHostName(fully_qualified=True),
      'PID=%s' % os.getpid(),
      'TIMESTAMP=%s' % UserDateTimeFormat(),
      'RANDOM_JUNK=%s' % GetRandomString(),
  )) + '\n'


def UnbufferedTemporaryFile(**kwargs):
  """Handle buffering changes in tempfile.TemporaryFile."""
  assert 'bufsize' not in kwargs
  assert 'buffering' not in kwargs
  if sys.version_info.major < 3:
    kwargs['bufsize'] = 0
  else:
    kwargs['buffering'] = 0
  return tempfile.TemporaryFile(**kwargs)


def UnbufferedNamedTemporaryFile(**kwargs):
  """Handle buffering changes in tempfile.NamedTemporaryFile."""
  assert 'bufsize' not in kwargs
  assert 'buffering' not in kwargs
  if sys.version_info.major < 3:
    kwargs['bufsize'] = 0
  else:
    kwargs['buffering'] = 0
  return tempfile.NamedTemporaryFile(**kwargs)
