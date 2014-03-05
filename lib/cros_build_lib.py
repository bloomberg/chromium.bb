# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

import collections
import contextlib
from datetime import datetime
from email.utils import formatdate
import errno
import functools
import logging
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import types

# TODO(build): Fix this.
# This should be absolute import, but that requires fixing all
# relative imports first.
_path = os.path.realpath(__file__)
_path = os.path.normpath(os.path.join(os.path.dirname(_path), '..', '..'))
sys.path.insert(0, _path)
from chromite.buildbot import constants
from chromite.lib import signals
# Now restore it so that relative scripts don't get cranky.
sys.path.pop(0)
del _path


STRICT_SUDO = False

logger = logging.getLogger('chromite')

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
_SHELL_ESCAPE_CHARS = frozenset('"`$')


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
  s = s.encode('utf-8')

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
  # Use str before repr to translate unicode strings to regular strings.
  return ' '.join(ShellQuote(arg) for arg in cmd)


class CommandResult(object):
  """An object to store various attributes of a child process."""

  def __init__(self, cmd=None, error=None, output=None, returncode=None):
    self.cmd = cmd
    self.error = error
    self.output = output
    self.returncode = returncode

  @property
  def cmdstr(self):
    """Return self.cmd as a space-separated string, useful for log messages."""
    return CmdToStr(self.cmd)


class RunCommandError(Exception):
  """Error caught in RunCommand() method."""
  def __init__(self, msg, result, exception=None):
    self.msg, self.result, self.exception = msg, result, exception
    if exception is not None and not isinstance(exception, Exception):
      raise ValueError("exception must be an exception instance; got %r"
                       % (exception,))
    Exception.__init__(self, msg)
    self.args = (msg, result, exception)

  def Stringify(self, error=True, output=True):
    """Custom method for controlling what is included in stringifying this.

    Each individual argument is the literal name of an attribute
    on the result object; if False, that value is ignored for adding
    to this string content.  If true, it'll be incorporated.

    Args:
      error: See comment about individual arguments above.
      output: See comment about individual arguments above.
    """
    items = ['return code: %s' % (self.result.returncode,)]
    if error and self.result.error:
      items.append(self.result.error)
    if output and self.result.output:
      items.append(self.result.output)
    items.append(self.msg)
    return '\n'.join(items)

  def __str__(self):
    # __str__ needs to return ascii, thus force a conversion to be safe.
    return self.Stringify().decode('utf-8').encode('ascii', 'xmlcharrefreplace')

  def __eq__(self, other):
    return (type(self) == type(other) and
            self.args == other.args)

  def __ne__(self, other):
    return not self.__eq__(other)


class TerminateRunCommandError(RunCommandError):
  """We were signaled to shutdown while running a command.

  Client code shouldn't generally know, nor care about this class.  It's
  used internally to suppress retry attempts when we're signaled to die.
  """


def SudoRunCommand(cmd, user='root', **kwargs):
  """Run a command via sudo.

  Client code must use this rather than coming up with their own RunCommand
  invocation that jams sudo in- this function is used to enforce certain
  rules in our code about sudo usage, and as a potential auditing point.

  Args:
    cmd: The command to run.  See RunCommand for rules of this argument-
         SudoRunCommand purely prefixes it with sudo.
    user: The user to run the command as.
    kwargs: See RunCommand options, it's a direct pass thru to it.
          Note that this supports a 'strict' keyword that defaults to True.
          If set to False, it'll suppress strict sudo behavior.

  Returns:
    See RunCommand documentation.

  Raises:
    This function may immediately raise RunCommandError if we're operating
    in a strict sudo context and the API is being misused.
    Barring that, see RunCommand's documentation- it can raise the same things
    RunCommand does.
  """
  sudo_cmd = ['sudo']

  strict = kwargs.pop('strict', True)

  if user == 'root' and os.geteuid() == 0:
    return RunCommand(cmd, **kwargs)

  if strict and STRICT_SUDO:
    if 'CROS_SUDO_KEEP_ALIVE' not in os.environ:
      raise RunCommandError(
          'We were invoked in a strict sudo non - interactive context, but no '
          'sudo keep alive daemon is running.  This is a bug in the code.',
          CommandResult(cmd=cmd, returncode=126))
    sudo_cmd += ['-n']

  if user != 'root':
    sudo_cmd += ['-u', user]

  # Pass these values down into the sudo environment, since sudo will
  # just strip them normally.
  extra_env = kwargs.pop('extra_env', None)
  extra_env = {} if extra_env is None else extra_env.copy()

  for var in constants.ENV_PASSTHRU:
    if var not in extra_env and var in os.environ:
      extra_env[var] = os.environ[var]

  sudo_cmd.extend('%s=%s' % (k, v) for k, v in extra_env.iteritems())

  # Finally, block people from passing options to sudo.
  sudo_cmd.append('--')

  if isinstance(cmd, basestring):
    # We need to handle shell ourselves so the order is correct:
    #  $ sudo [sudo args] -- bash -c '[shell command]'
    # If we let RunCommand take care of it, we'd end up with:
    #  $ bash -c 'sudo [sudo args] -- [shell command]'
    shell = kwargs.pop('shell', False)
    if not shell:
      raise Exception('Cannot run a string command without a shell')
    sudo_cmd.extend(['/bin/bash', '-c', cmd])
  else:
    sudo_cmd.extend(cmd)

  return RunCommand(sudo_cmd, **kwargs)


def _KillChildProcess(proc, kill_timeout, cmd, original_handler, signum, frame):
  """Functor that when curried w/ the appropriate arguments, is used as a signal
  handler by RunCommand.

  This is internal to Runcommand.  No other code should use this.
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
      proc.terminate()
      while proc.poll() is None and kill_timeout >= 0:
        time.sleep(0.1)
        kill_timeout -= 0.1

      if proc.poll() is None:
        # Still doesn't want to die.  Too bad, so sad, time to die.
        proc.kill()
    except EnvironmentError as e:
      Warning('Ignoring unhandled exception in _KillChildProcess: %s', e)

    # Ensure our child process has been reaped.
    proc.wait()

  if not signals.RelaySignal(original_handler, signum, frame):
    # Mock up our own, matching exit code for signaling.
    cmd_result = CommandResult(cmd=cmd, returncode=signum << 8)
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

  def send_signal(self, signum):
    if self.returncode is not None:
      # The original implementation in Popen would allow signaling whatever
      # process now occupies this pid, even if the Popen object had waitpid'd.
      # Since we can escalate to sudo kill, we do not want to allow that.
      # Fixing this addresses that angle, and makes the API less sucky in the
      # process.
      return

    try:
      os.kill(self.pid, signum)
    except EnvironmentError as e:
      if e.errno == errno.EPERM:
        # Kill returns either 0 (signal delivered), or 1 (signal wasn't
        # delivered).  This isn't particularly informative, but we still
        # need that info to decide what to do, thus the error_code_ok=True.
        ret = SudoRunCommand(['kill', '-%i' % signum, str(self.pid)],
                             print_cmd=False, redirect_stdout=True,
                             redirect_stderr=True, error_code_ok=True)
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


#pylint: disable=W0622
def RunCommand(cmd, print_cmd=True, error_message=None, redirect_stdout=False,
               redirect_stderr=False, cwd=None, input=None, enter_chroot=False,
               shell=False, env=None, extra_env=None, ignore_sigint=False,
               combine_stdout_stderr=False, log_stdout_to_file=None,
               chroot_args=None, debug_level=logging.INFO,
               error_code_ok=False, kill_timeout=1, log_output=False,
               stdout_to_pipe=False, capture_output=False, quiet=False):
  """Runs a command.

  Args:
    cmd: cmd to run.  Should be input to subprocess.Popen. If a string, shell
      must be true. Otherwise the command must be an array of arguments, and
      shell must be false.
    print_cmd: prints the command before running it.
    error_message: prints out this message when an error occurs.
    redirect_stdout: returns the stdout.
    redirect_stderr: holds stderr output until input is communicated.
    cwd: the working directory to run this cmd.
    input: input to pipe into this command through stdin.
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
    chroot_args: An array of arguments for the chroot environment wrapper.
    debug_level: The debug level of RunCommand's output - applies to output
                 coming from subprocess as well.
    error_code_ok: Does not raise an exception when command returns a non-zero
                   exit code.  Instead, returns the CommandResult object
                   containing the exit code. Note: will still raise an
                   exception if the cmd file does not exist.
    kill_timeout: If we're interrupted, how long should we give the invoked
                  process to shutdown from a SIGTERM before we SIGKILL it.
                  Specified in seconds.
    log_output: Log the command and its output automatically.
    stdout_to_pipe: Redirect stdout to pipe.
    capture_output: Set |redirect_stdout| and |redirect_stderr| to True.
    quiet: Set |print_cmd| to False, |stdout_to_pipe| and |combine_stdout_stderr|
      to True.

  Returns:
    A CommandResult object.

  Raises:
    RunCommandError:  Raises exception on error with optional error_message.
  """
  if capture_output:
    redirect_stdout, redirect_stderr = True, True

  if quiet:
    print_cmd = False
    stdout_to_pipe, combine_stdout_stderr = True, True

  # Set default for variables.
  stdout = None
  stderr = None
  stdin = None
  cmd_result = CommandResult()

  mute_output = logger.getEffectiveLevel() > debug_level

  # Force the timeout to float; in the process, if it's not convertible,
  # a self-explanatory exception will be thrown.
  kill_timeout = float(kill_timeout)

  def _get_tempfile():
    try:
      return tempfile.TemporaryFile(bufsize=0)
    except EnvironmentError as e:
      if e.errno != errno.ENOENT:
        raise
      # This can occur if we were pointed at a specific location for our
      # TMP, but that location has since been deleted.  Suppress that issue
      # in this particular case since our usage gurantees deletion,
      # and since this is primarily triggered during hard cgroups shutdown.
      return tempfile.TemporaryFile(bufsize=0, dir='/tmp')

  # Modify defaults based on parameters.
  # Note that tempfiles must be unbuffered else attempts to read
  # what a separate process did to that file can result in a bad
  # view of the file.
  if log_stdout_to_file:
    stdout = open(log_stdout_to_file, 'w+')
  elif stdout_to_pipe:
    stdout = subprocess.PIPE
  elif redirect_stdout or mute_output or log_output:
    stdout = _get_tempfile()

  if combine_stdout_stderr:
    stderr = subprocess.STDOUT
  elif redirect_stderr or mute_output or log_output:
    stderr = _get_tempfile()

  # If subprocesses have direct access to stdout or stderr, they can bypass
  # our buffers, so we need to flush to ensure that output is not interleaved.
  if stdout is None or stderr is None:
    sys.stdout.flush()
    sys.stderr.flush()

  if input:
    stdin = subprocess.PIPE

  if isinstance(cmd, basestring):
    if not shell:
      raise Exception('Cannot run a string command without a shell')
    cmd = ['/bin/bash', '-c', cmd]
    shell = False
  elif shell:
    raise Exception('Cannot run an array command with a shell')

  # If we are using enter_chroot we need to use enterchroot pass env through
  # to the final command.
  env = env.copy() if env is not None else os.environ.copy()
  if enter_chroot and not IsInsideChroot():
    wrapper = ['cros_sdk']

    if chroot_args:
      wrapper += chroot_args

    if extra_env:
      wrapper.extend('%s=%s' % (k, v) for k, v in extra_env.iteritems())

    cmd = wrapper + ['--'] + cmd

  elif extra_env:
    env.update(extra_env)

  for var in constants.ENV_PASSTHRU:
    if var not in env and var in os.environ:
      env[var] = os.environ[var]

  # Print out the command before running.
  if print_cmd or log_output:
    if cwd:
      logger.log(debug_level, 'RunCommand: %s in %s', CmdToStr(cmd), cwd)
    else:
      logger.log(debug_level, 'RunCommand: %s', CmdToStr(cmd))

  cmd_result.cmd = cmd

  proc = None
  # Verify that the signals modules is actually usable, and won't segfault
  # upon invocation of getsignal.  See signals.SignalModuleUsable for the
  # details and upstream python bug.
  use_signals = signals.SignalModuleUsable()
  try:
    proc = _Popen(cmd, cwd=cwd, stdin=stdin, stdout=stdout,
                  stderr=stderr, shell=False, env=env,
                  close_fds=True)

    if use_signals:
      if ignore_sigint:
        old_sigint = signal.signal(signal.SIGINT, signal.SIG_IGN)
      else:
        old_sigint = signal.getsignal(signal.SIGINT)
        signal.signal(signal.SIGINT,
                      functools.partial(_KillChildProcess, proc, kill_timeout,
                                        cmd, old_sigint))

      old_sigterm = signal.getsignal(signal.SIGTERM)
      signal.signal(signal.SIGTERM,
                    functools.partial(_KillChildProcess, proc, kill_timeout,
                                      cmd, old_sigterm))

    try:
      (cmd_result.output, cmd_result.error) = proc.communicate(input)
    finally:
      if use_signals:
        signal.signal(signal.SIGINT, old_sigint)
        signal.signal(signal.SIGTERM, old_sigterm)

      if stdout and not log_stdout_to_file and not stdout_to_pipe:
        stdout.seek(0)
        cmd_result.output = stdout.read()
        stdout.close()

      if stderr and stderr != subprocess.STDOUT:
        stderr.seek(0)
        cmd_result.error = stderr.read()
        stderr.close()

    cmd_result.returncode = proc.returncode

    if log_output:
      if cmd_result.output:
        logger.log(debug_level, '(stdout):\n%s' % cmd_result.output)
      if cmd_result.error:
        logger.log(debug_level, '(stderr):\n%s' % cmd_result.error)

    if not error_code_ok and proc.returncode:
      msg = ('Failed command "%s", cwd=%s, extra env=%r'
             % (CmdToStr(cmd), cwd, extra_env))
      if error_message:
        msg += '\n%s' % error_message
      raise RunCommandError(msg, cmd_result)
  except OSError as e:
    estr = str(e)
    if e.errno == errno.EACCES:
      estr += '; does the program need `chmod a+x`?'
    raise RunCommandError(estr, CommandResult(cmd=cmd), exception=e)
  finally:
    if proc is not None:
      # Ensure the process is dead.
      _KillChildProcess(proc, kill_timeout, cmd, None, None, None)

  return cmd_result


# Convenience RunCommand methods.
#
# We don't use functools.partial because it binds the methods at import time,
# which doesn't work well with unit tests, since it bypasses the mock that may
# be set up for RunCommand.

def DebugRunCommand(*args, **kwargs):
  kwargs.setdefault('debug_level', logging.DEBUG)
  return RunCommand(*args, **kwargs)


class DieSystemExit(SystemExit):
  """Custom Exception used so we can intercept this if necessary."""


def Die(message, *args):
  """Emits an error message with a stack trace and halts execution.

  Args:
    message: The message to be emitted before exiting.
  """
  logger.error(message, *args)
  raise DieSystemExit(1)


def Error(message, *args, **kwargs):
  """Emits a red warning message using the logging module."""
  logger.error(message, *args, **kwargs)


#pylint: disable=W0622
def Warning(message, *args, **kwargs):
  """Emits a warning message using the logging module."""
  logger.warn(message, *args, **kwargs)


def Info(message, *args, **kwargs):
  """Emits an info message using the logging module."""
  logger.info(message, *args, **kwargs)


def Debug(message, *args, **kwargs):
  """Emits a debugging message using the logging module."""
  logger.debug(message, *args, **kwargs)


def PrintBuildbotLink(text, url, handle=None):
  """Prints out a link to buildbot."""
  text = ' '.join(text.split())
  (handle or sys.stderr).write('\n@@@STEP_LINK@%s@%s@@@\n' % (text, url))


def PrintBuildbotStepText(text, handle=None):
  """Prints out stage text to buildbot."""
  text = ' '.join(text.split())
  (handle or sys.stderr).write('\n@@@STEP_TEXT@%s@@@\n' % (text,))


def PrintBuildbotStepWarnings(handle=None):
  """Marks a stage as having warnings."""
  (handle or sys.stderr).write('\n@@@STEP_WARNINGS@@@\n')


def PrintBuildbotStepFailure(handle=None):
  """Marks a stage as having failures."""
  (handle or sys.stderr).write('\n@@@STEP_FAILURE@@@\n')


def PrintBuildbotStepName(name, handle=None):
  """Marks a step name for buildbot to display."""
  (handle or sys.stderr).write('\n@@@BUILD_STEP %s@@@\n' % name)


def ListFiles(base_dir):
  """Recursively list files in a directory.

  Args:
    base_dir: directory to start recursively listing in.

  Returns:
    A list of files relative to the base_dir path or
    An empty list of there are no files in the directories.
  """
  directories = [base_dir]
  files_list = []
  while directories:
    directory = directories.pop()
    for name in os.listdir(directory):
      fullpath = os.path.join(directory, name)
      if os.path.isfile(fullpath):
        files_list.append(fullpath)
      elif os.path.isdir(fullpath):
        directories.append(fullpath)

  return files_list


def IsInsideChroot():
  """Returns True if we are inside chroot."""
  return os.path.exists('/etc/cros_chroot_version')

def AssertInsideChroot():
  """Die if we are outside the chroot"""
  if not IsInsideChroot():
    Die('%s: please run inside the chroot', os.path.basename(sys.argv[0]))

def AssertOutsideChroot():
  """Die if we are inside the chroot"""
  if IsInsideChroot():
    Die('%s: please run outside the chroot', os.path.basename(sys.argv[0]))


def GetChromeosVersion(str_obj):
  """Helper method to parse output for CHROMEOS_VERSION_STRING.

  Args:
    str_obj: a string, which may contain Chrome OS version info.

  Returns:
    A string, value of CHROMEOS_VERSION_STRING environment variable set by
      chromeos_version.sh. Or None if not found.
  """
  if str_obj is not None:
    match = re.search(r'CHROMEOS_VERSION_STRING=([0-9_.]+)', str_obj)
    if match and match.group(1):
      Info ('CHROMEOS_VERSION_STRING = %s' % match.group(1))
      return match.group(1)

  Info ('CHROMEOS_VERSION_STRING NOT found')
  return None


def GetHostName(fully_qualified=False):
  """Return hostname of current machine, with domain if |fully_qualified|."""
  hostname = socket.gethostbyaddr(socket.gethostname())[0]

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


def TimedCommand(functor, *args, **kwargs):
  """Wrapper for simple log timing of other python functions.

  If you want to log info about how long it took to run an arbitrary command,
  you would do something like:
    TimedCommand(RunCommand, ['wget', 'http://foo'])

  Args:
    functor: The function to run.
    args: The args to pass to the function.
    kwargs: Optional args to pass to the function.
    timed_log_level: The log level to use (defaults to info).
    timed_log_msg: The message to log with timing info appended (defaults to
                   details about the call made).  It must include a %s to hold
                   the time delta details.
  """
  log_msg = kwargs.pop('timed_log_msg', '%s(*%r, **%r) took: %%s'
                       % (functor.__name__, args, kwargs))
  log_level = kwargs.pop('timed_log_level', logging.INFO)
  start = datetime.now()
  ret = functor(*args, **kwargs)
  logger.log(log_level, log_msg, datetime.now() - start)
  return ret


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
    para = 'xz'
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


def CreateTarball(target, cwd, sudo=False, compression=COMP_XZ, chroot=None,
                  inputs=None, extra_args=None, **kwargs):
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
    extra_args: A list of extra args to pass to "tar".
    kwargs: Any RunCommand options/overrides to use.

  Returns:
    The cmd_result object returned by the RunCommand invocation.
  """
  if inputs is None:
    inputs = ['.']
  if extra_args is None:
    extra_args = []
  kwargs.setdefault('debug_level', logging.DEBUG)

  comp = FindCompressor(compression, chroot=chroot)
  cmd = (['tar'] +
         extra_args +
         ['--sparse', '-I', comp, '-cf', target] +
         list(inputs))
  rc_func = SudoRunCommand if sudo else RunCommand
  return rc_func(cmd, cwd=cwd, **kwargs)


def GetInput(prompt):
  """Helper function to grab input from a user.   Makes testing easier."""
  return raw_input(prompt)


def GetChoice(prompt, options):
  """Ask user to choose an option from the list.

  Args:
    prompt: The text to display before listing options.
    options: The list of options to display.

  Returns:
    An integer.
  """
  prompt = prompt[:]

  for opt, i in zip(options, xrange(len(options))):
    prompt += '\n  [%d]: %s' % (i, opt)

  prompt = '%s\nEnter your choice to continue [0-%d]: ' % (
      prompt, len(options) - 1)

  while True:
    try:
      choice = int(GetInput(prompt))
    except ValueError:
      print 'Input value is not an integer'
      continue

    if choice < 0 or choice >= len(options):
      print 'Input value is out of range'
    else:
      break

  return choice


def BooleanPrompt(prompt="Do you want to continue?", default=True,
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
    raise ValueError("true_value and false_value must differ: got %r"
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
      print
      response = None
    except KeyboardInterrupt:
      # If the user hits CTRL+C, just exit the process.
      print
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

  if isinstance(sval, basestring):
    s = sval.lower()
    if s in ('yes', 'y', '1', 'true'):
      return True
    elif s in ('no', 'n', '0', 'false'):
      return False

  if msg is not None:
    Warning('%s: %r' % (msg, sval))
    return default
  else:
    raise ValueError('Could not decode as a boolean value: %r' % sval)


# Suppress whacked complaints about abstract class being unused.
#pylint: disable=R0921
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

  def __exit__(self, exc_type, exc, traceback):
    curpid = os.getpid()
    if curpid == self.ALTERNATE_MASTER_PID:
      self._invoking_pid = curpid
    if curpid == self._invoking_pid:
      return self._exit(exc_type, exc, traceback)

  def _enter(self):
    raise NotImplementedError(self, '_enter')

  def _exit(self, exc_type, exc, traceback):
    raise NotImplementedError(self, '_exit')


@contextlib.contextmanager
def NoOpContextManager():
  yield


def AllowDisabling(enabled, functor, *args, **kwargs):
  """Context Manager wrapper that can be used to enable/disable usage.

  This is mainly useful to control whether or not a given Context Manager
  is used.

  For example:

  with AllowDisabling(options.timeout <= 0, Timeout, options.timeout):
    ... do code w/in a timeout context..

  If options.timeout is a positive integer, then the_Timeout context manager is
  created and ran.  If it's zero or negative, then the timeout code is disabled.

  While Timeout *could* handle this itself, it's redundant having each
  implementation do this, thus the generic wrapper.
  """
  if enabled:
    return functor(*args, **kwargs)
  return NoOpContextManager()


class ContextManagerStack(object):
  """Context manager that is designed to safely allow nesting and stacking.

  Python2.7 directly supports a with syntax removing the need for this,
  although this form avoids indentation hell if there is a lot of context
  managers.

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

  def __exit__(self, exc_type, exc, traceback):
    # Exit each context manager in stack in reverse order, tracking the results
    # to know whether or not to suppress the exception raised (or to switch that
    # exception to a new one triggered by an individual handler's __exit__).
    for handler in reversed(self._stack):
      # pylint: disable=W0702
      try:
        if handler.__exit__(exc_type, exc, traceback):
          exc_type = exc = traceback = None
      except:
        exc_type, exc, traceback = sys.exc_info()

    self._stack = []

    # Return True if any exception was handled.
    if all(x is None for x in (exc_type, exc, traceback)):
      return True

    # Raise any exception that is left over from exiting all context managers.
    # Normally a single context manager would return False to allow caller to
    # re-raise the exception itself, but here the exception might have been
    # raised during the exiting of one of the individual context managers.
    raise exc_type, exc, traceback


def SetupBasicLogging(level=logging.DEBUG):
  """Sets up basic logging to use format from constants."""
  logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
  date_format = constants.LOGGER_DATE_FMT
  logging.basicConfig(level=level, format=logging_format,
                      datefmt=date_format)


class ApiMismatchError(Exception):
  """Raised by GetTargetChromiteApiVersion."""


class NoChromiteError(Exception):
  """Raised when an expected chromite installation was missing."""


def GetTargetChromiteApiVersion(buildroot, validate_version=True):
  """Get the re-exec API version of the target chromite.

  Args:
    buildroot: The directory containing the chromite to check.
    validate_version: If set to true, checks the target chromite for
      compatibility, and raises an ApiMismatchError when there is an
      incompatibility.

  Returns:
    The version number in (major, minor) tuple.

  Raises:
    May raise an ApiMismatchError if validate_version is set.
  """
  try:
    api = RunCommand(
        [constants.PATH_TO_CBUILDBOT, '--reexec-api-version'],
        cwd=buildroot, error_code_ok=True, capture_output=True)
  except RunCommandError:
    # Although error_code_ok=True was used, this exception will still be raised
    # if the executible did not exist.
    full_cbuildbot_path = os.path.join(buildroot, constants.PATH_TO_CBUILDBOT)
    if not os.path.exists(full_cbuildbot_path):
      raise NoChromiteError('No cbuildbot found in buildroot %s, expected to '
                            'find %s. ' % (buildroot, full_cbuildbot_path))
    raise

  # If the command failed, then we're targeting a cbuildbot that lacks the
  # option; assume 0:0 (ie, initial state).
  major = minor = 0
  if api.returncode == 0:
    major, minor = map(int, api.output.strip().split('.', 1))

  if validate_version and major != constants.REEXEC_API_MAJOR:
    raise ApiMismatchError(
        'The targeted version of chromite in buildroot %s requires '
        'api version %i, but we are api version %i.  We cannot proceed.'
        % (buildroot, major, constants.REEXEC_API_MAJOR))

  return major, minor


def GetChrootVersion(chroot=None, buildroot=None):
  """Extract the version of the chroot.

  Args:
    chroot: Full path to the chroot to examine.
    buildroot: If |chroot| is not set, find it relative to |buildroot|.

  Returns:
    The version of the chroot dir.
  """
  if chroot is None and buildroot is None:
    raise ValueError('need either |chroot| or |buildroot| to search')

  from chromite.lib import osutils
  if chroot is None:
    chroot = os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR)
  ver_path = os.path.join(chroot, 'etc', 'cros_chroot_version')
  try:
    return osutils.ReadFile(ver_path).strip()
  except IOError:
    Warning('could not read %s', ver_path)
    return None


def iflatten_instance(iterable, terminate_on_kls=(basestring,)):
  """Derivative of snakeoil.lists.iflatten_instance; flatten an object.

  Given an object, flatten it into a single depth iterable-
  stopping descent on objects that either aren't iterable, or match
  isinstance(obj, terminate_on_kls).

  Example:
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
    return not isinstance(item, basestring) or len(item) > 1

  if not descend_into(iterable):
    yield iterable
    return
  for item in iterable:
    if not descend_into(item):
      yield item
    else:
      for subitem in iflatten_instance(item, terminate_on_kls):
        yield subitem


# TODO: Remove this once we move to snakeoil.
def load_module(name):
  """load a module

  Args:
    name: python dotted namespace path of the module to import

  Returns:
    imported module

  Raises:
    FailedImport if importing fails
  """
  m = __import__(name)
  # __import__('foo.bar') returns foo, so...
  for bit in name.split('.')[1:]:
    m = getattr(m, bit)
  return m


def PredicateSplit(func, iterable):
  """Splits an iterable into two groups based on a predicate return value.

  Args:
    func: A functor that takes an item as its argument and returns a boolean
      value indicating which group the item belongs.
    iterable: The collection to split.

  Returns:
    A tuple containing two lists, the first containing items that func()
    returned True for, and the second containing items that func() returned
    False for.
  """
  trues, falses = [], []
  for x in iterable:
    (trues if func(x) else falses).append(x)
  return trues, falses


@contextlib.contextmanager
def Open(input, mode='r'):
  """Convenience ctx that accepts a file path or an already open file object."""
  if isinstance(input, basestring):
    with open(input, mode=mode) as f:
      yield f
  else:
    yield input


def LoadKeyValueFile(input, ignore_missing=False, multiline=False):
  """Turn a key=value file into a dict

  Note: If you're designing a new data store, please use json rather than
  this format.  This func is designed to work with legacy/external files
  where json isn't an option.

  Args:
    input: The file to read.  Can be a path or an open file object.
    ignore_missing: If the file does not exist, return an empty dict.
    multiline: Allow a value enclosed by quotes to span multiple lines.

  Returns:
    a dict of all the key=value pairs found in the file.
  """
  d = {}

  try:
    with Open(input) as f:
      key = None
      in_quotes = None
      for raw_line in f:
        line = raw_line.split('#')[0]
        if not line.strip():
          continue

        # Continue processing a multiline value.
        if multiline and in_quotes and key:
          if line.rstrip()[-1] == in_quotes:
            # Wrap up the multiline value if the line ends with a quote.
            d[key] += line.rstrip()[:-1]
            in_quotes = None
          else:
            d[key] += line
          continue

        chunks = line.split('=', 1)
        if len(chunks) != 2:
          raise ValueError('Malformed version file %r; line %r'
                           % (input, raw_line))
        key = chunks[0].strip()
        val = chunks[1].strip()
        if len(val) >= 2 and val[0] in "\"'" and val[0] == val[-1]:
          # Strip matching quotes on the same line.
          val = val[1:-1]
        elif val and multiline and val[0] in "\"'" :
          # Unmatched quote here indicates a multiline value. Do not
          # strip the '\n' at the end of the line.
          in_quotes = val[0]
          val = chunks[1].lstrip()[1:]
        d[key] = val
  except EnvironmentError as e:
    if not (ignore_missing and e.errno == errno.ENOENT):
      raise

  return d


def MemoizedSingleCall(functor):
  """Decorator for simple functor targets, caching the results

  The functor must accept no arguments beyond either a class or self (depending
  on if this is used in a classmethod/instancemethod context).  Results of the
  wrapped method will be written to the class/instance namespace in a specially
  named cached value.  All future invocations will just reuse that value.

  Note that this cache is per-process, so sibling and parent processes won't
  notice updates to the cache.
  """
  # TODO(build): Should we rebase to snakeoil.klass.cached* functionality?
  def f(obj):
    # pylint: disable=W0212
    key = f._cache_key
    val = getattr(obj, key, None)
    if val is None:
      val = functor(obj)
      setattr(obj, key, val)
    return val

  # Dummy up our wrapper to make it look like what we're wrapping,
  # and expose the underlying docstrings.
  f.__name__ = functor.__name__
  f.__module__ = functor.__module__
  f.__doc__ = functor.__doc__
  f._cache_key = '_%s_cached' % (functor.__name__.lstrip('_'),)
  return f


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
      raise inst, None, tb
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
  return '%s (%s)' % (formatdate(timeval=timeval, localtime=True),
                      time.tzname[0])


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
        logging.warning('Noticed invalid default board: |%s|. '
                        'Ignoring this default.',
                        default_board)
        default_board = None
  except IOError:
    return None

  return default_board


def GetBoard(device_board, override_board=None, force=False):
  """Gets the board name to use.

  Ask user to confirm when |override_board| and |device_board| are
  both None.

  Args:
    device_board: The board detected on the device.
    override_board: Overrides the board.
    force: Force using the default board if |device_board| is None.

  Returns:
    Returns the first non-None board in the following order:
    |override_board|, |device_board|, and GetDefaultBoard().

  Raises:
    DieSystemExit: If user enters no.
  """
  if override_board:
    return override_board

  board = device_board or GetDefaultBoard()
  if not device_board:
    msg = 'Cannot detect board name; using default board %s.' % board
    if not force and not BooleanPrompt(default=False, prolog=msg):
      Die('Exiting...')

    logging.warning(msg)

  return board


class AttributeFrozenError(Exception):
  """Raised when frozen attribute value is modified."""


class FrozenAttributesClass(type):
  """Metaclass for any class to support freezing attribute values.

  This metaclass can be used by any class to add the ability to
  freeze attribute values with the Freeze method.

  Use by adding this line in a class:
    __metaclass__ = FrozenAttributesClass
  """
  _FROZEN_ERR_MSG = 'Attribute values are frozen, cannot alter %s.'

  def __new__(mcs, clsname, bases, scope):
    # Create Freeze method that freezes current attributes.
    # pylint: disable=E1003
    if 'Freeze' in scope:
      raise TypeError('Class %s has its own Freeze method, cannot use with'
                      ' the FrozenAttributesClass metaclass.' % clsname)

    # Make sure cls will have _FROZEN_ERR_MSG set.
    scope.setdefault('_FROZEN_ERR_MSG', mcs._FROZEN_ERR_MSG)

    # Create the class.
    cls = super(FrozenAttributesClass, mcs).__new__(mcs, clsname, bases, scope)

    # Replace cls.__setattr__ with the one that honors freezing.
    orig_setattr = cls.__setattr__
    def SetAttr(obj, name, value):
      """If the object is frozen then abort."""
      # pylint: disable=W0212
      if getattr(obj, '_frozen', False):
        raise AttributeFrozenError(obj._FROZEN_ERR_MSG % name)
      if isinstance(orig_setattr, types.MethodType):
        orig_setattr(obj, name, value)
      else:
        super(cls, obj).__setattr__(name, value)
    cls.__setattr__ = SetAttr

    # Add new cls.Freeze method.
    def Freeze(obj):
      obj._frozen = True
    cls.Freeze = Freeze

    return cls


class FrozenAttributesMixin(object):
  """Alternate mechanism for freezing attributes in a class.

  If an existing class is not a new-style class then it will be unable to
  use the FrozenAttributesClass metaclass directly.  Simply use this class
  as a mixin instead to accomplish the same thing.
  """
  __metaclass__ = FrozenAttributesClass


def GetIPv4Address(dev=None, global_ip=True):
  """Returns any global/host IP address or the IP address of the given device.

  socket.gethostname() is insufficient for machines where the host files are
  not set up "correctly."  Since some of our builders may have this issue,
  this method gives you a generic way to get the address so you are reachable
  either via a VM or remote machine on the same network.

  Args:
    dev: Get the IP address of the device (e.g. 'eth0').
    global_ip: If set True, returns a globally valid IP address. Otherwise,
      returns a local IP address (default: True).
  """
  cmd = ['ip', 'addr', 'show']
  cmd += ['scope', 'global' if global_ip else 'host']
  cmd += [] if dev is None else ['dev', dev]

  result = RunCommand(cmd, print_cmd=False, capture_output=True)
  matches = re.findall(r'\binet (\d+\.\d+\.\d+\.\d+).*', result.output)
  if matches:
    return matches[0]
  Warning('Failed to find ip address in %r', result.output)
  return None


def GetSysroot(board=None):
  """Returns the sysroot for |board| or '/' if |board| is None."""
  return '/' if board is None else os.path.join('/build', board)


# Chroot helper methods; assume default 'chroot' directory name.
def ToChrootPath(path):
  """Reinterprets |path| to be used inside of chroot.

  Returns:
    A reinterpreted path if currently outside chroot or |path| if
    inside chroot.
  """
  from chromite.lib import osutils
  from chromite.lib import git
  full_path = osutils.ExpandPath(path)
  if IsInsideChroot():
    return full_path

  try:
    return git.ReinterpretPathForChroot(full_path)
  except Exception:
    raise ValueError('path %s is outside of your source tree' % path)


def FromChrootPath(path):
  """Interprets a chroot |path| to be used inside or outside chroot.

  Returns:
    If currently outside chroot, returns the reinterpreted |path| to
    be used outside chroot. Otherwise, returns |path|.
  """
  from chromite.lib import osutils
  full_path = osutils.ExpandPath(path)
  if IsInsideChroot():
    return full_path

  # Replace chroot source root with current source root, if applicable.
  if full_path.startswith(constants.CHROOT_SOURCE_ROOT):
    return os.path.join(
        constants.SOURCE_ROOT,
        full_path[len(constants.CHROOT_SOURCE_ROOT):].strip(os.path.sep))
  else:
    return os.path.join(constants.SOURCE_ROOT, constants.DEFAULT_CHROOT_DIR,
                        path.strip(os.path.sep))


def Collection(classname, **kwargs):
  """Create a new class with mutable named members.

  This is like collections.namedtuple, but mutable.  Also similar to the
  python 3.3 types.SimpleNamespace.

  Example:
    # Declare default values for this new class.
    Foo = cros_build_lib.Collection('Foo', a=0, b=10)
    # Create a new class but set b to 4.
    foo = Foo(b=4)
    # Print out a (will be the default 0) and b (will be 4).
    print('a = %i, b = %i' % (foo.a, foo.b))
  """
  def sn_init(self, **kwargs):
    """The new class's __init__ function."""
    # First verify the kwargs don't have excess settings.
    valid_keys = set(self.__slots__[1:])
    these_keys = set(kwargs.keys())
    invalid_keys = these_keys - valid_keys
    if invalid_keys:
      raise TypeError('invalid keyword arguments for this object: %r' %
                      invalid_keys)

    # Now initialize this object.
    for k in valid_keys:
      setattr(self, k, kwargs.get(k, self.__defaults__[k]))

  def sn_repr(self):
    """The new class's __repr__ function."""
    return '%s(%s)' % (classname, ', '.join(
        '%s=%r' % (k, getattr(self, k)) for k in self.__slots__[1:]))

  # Give the new class a unique name and then generate the code for it.
  classname = 'Collection_%s' % classname
  expr = '\n'.join((
      'class %(classname)s(object):',
      '  __slots__ = ["__defaults__", "%(slots)s"]',
      '  __defaults__ = {}',
  )) % {
      'classname': classname,
      'slots': '", "'.join(sorted(str(k) for k in kwargs)),
  }

  # Create the class in a local namespace as exec requires.
  namespace = {}
  exec expr in namespace
  new_class = namespace[classname]

  # Bind the helpers.
  new_class.__defaults__ = kwargs.copy()
  new_class.__init__ = sn_init
  new_class.__repr__ = sn_repr

  return new_class


def GetImageDiskPartitionInfo(image_path, unit='MB'):
  """Returns the disk partition table of an image.

  Args:
    image_path: Path to the image file.
    unit: The unit to display (e.g., 'B', 'KiB', 'KB', 'MB').
      See `parted` documentation for more info.

  Returns:
    A dictionary of ParitionInfo items with parition names as keys.
  """
  lines = RunCommand(
      ['parted', '-m', image_path, 'unit', unit, 'print'],
      capture_output=True).output.splitlines()

  # Sample output (partition #, start, end, size, file system, name, flags):
  #   /foo/chromiumos_qemu_image.bin:3360MB:file:512:512:gpt:;
  #   11:0.03MB:8.42MB:8.39MB::RWFW:;
  #   6:8.42MB:8.42MB:0.00MB::KERN-C:;
  #   7:8.42MB:8.42MB:0.00MB::ROOT-C:;
  #   9:8.42MB:8.42MB:0.00MB::reserved:;
  #   10:8.42MB:8.42MB:0.00MB::reserved:;
  #   2:10.5MB:27.3MB:16.8MB::KERN-A:;
  #   4:27.3MB:44.0MB:16.8MB::KERN-B:;
  #   8:44.0MB:60.8MB:16.8MB:ext4:OEM:;
  #   12:128MB:145MB:16.8MB:fat16:EFI-SYSTEM:boot;
  #   5:145MB:2292MB:2147MB::ROOT-B:;
  #   3:2292MB:4440MB:2147MB:ext2:ROOT-A:;
  #   1:4440MB:7661MB:3221MB:ext4:STATE:;
  keys = ['number', 'start', 'end', 'size', 'file_system', 'name', 'flags']
  PartitionInfo = collections.namedtuple('PartitionInfo', keys)

  table = {}
  for line in lines:
    match = re.match(r'(.*:.*:.*:.*:.*:.*:.*);', line)
    if match:
      d = dict(zip(keys, match.group(1).split(':')))
      # Disregard any non-numeric partition number (e.g. the file path).
      if d['number'].isdigit():
        for key in ['start', 'end', 'size']:
          d[key] = float(d[key][:-len(unit)])

        table[d['name']] = PartitionInfo(**d)

  return table
