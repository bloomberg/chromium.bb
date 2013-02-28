# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

import contextlib
from datetime import datetime
from email.utils import formatdate
import errno
import functools
import json
import logging
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib

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


class CommandResult(object):
  """An object to store various attributes of a child process."""

  def __init__(self, cmd=None, error=None, output=None, returncode=None):
    self.cmd = cmd
    self.error = error
    self.output = output
    self.returncode = returncode


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

    Args:
      Each individual argument is the literal name of an attribute
      on the result object; if False, that value is ignored for adding
      to this string content.  If true, it'll be incorporated.
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


def SudoRunCommand(cmd, user='root', **kwds):
  """Run a command via sudo.

  Client code must use this rather than coming up with their own RunCommand
  invocation that jams sudo in- this function is used to enforce certain
  rules in our code about sudo usage, and as a potential auditing point.

  Args:
    cmd: The command to run.  See RunCommand for rules of this argument-
         SudoRunCommand purely prefixes it with sudo.
    user: The user to run the command as.
    kwds: See RunCommand options, it's a direct pass thru to it.
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

  strict = kwds.pop('strict', True)

  if user == 'root' and os.geteuid() == 0:
    return RunCommand(cmd, **kwds)

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
  extra_env = kwds.pop('extra_env', None)
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
    shell = kwds.pop('shell', False)
    if not shell:
      raise Exception('Cannot run a string command without a shell')
    sudo_cmd.extend(['/bin/bash', '-c', cmd])
  else:
    sudo_cmd.extend(cmd)

  return RunCommand(sudo_cmd, **kwds)


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
    except EnvironmentError, e:
      Warning('Ignoring unhandled exception in _KillChildProcess: %s', e)

    # Ensure our child process has been reaped.
    proc.wait()

  if not signals.RelaySignal(original_handler, signum, frame):
    # Mock up our own, matching exit code for signaling.
    cmd_result = CommandResult(cmd=cmd, returncode=signum << 8)
    raise TerminateRunCommandError('Received signal %i' % signum, cmd_result)


class _Popen(subprocess.Popen):

  """
  subprocess.Popen derivative customized for our usage.

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
    except EnvironmentError, e:
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
def RunCommand(cmd, print_cmd=True, error_ok=False, error_message=None,
               redirect_stdout=False, redirect_stderr=False,
               cwd=None, input=None, enter_chroot=False, shell=False,
               env=None, extra_env=None, ignore_sigint=False,
               combine_stdout_stderr=False, log_stdout_to_file=None,
               chroot_args=None, debug_level=logging.INFO,
               error_code_ok=False, kill_timeout=1, log_output=False):
  """Runs a command.

  Args:
    cmd: cmd to run.  Should be input to subprocess.Popen. If a string, shell
      must be true. Otherwise the command must be an array of arguments, and
      shell must be false.
    print_cmd: prints the command before running it.
    error_ok: ***DEPRECATED, use error_code_ok instead***
              Does not raise an exception on any errors.
    error_message: prints out this message when an error occurs.
    redirect_stdout: returns the stdout.
    redirect_stderr: holds stderr output until input is communicated.
    cwd: the working directory to run this cmd.
    input: input to pipe into this command through stdin.
    enter_chroot: this command should be run from within the chroot.  If set,
      cwd must point to the scripts directory.
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
      If combine_stdout_stderr is set to True, then stderr will also be logged
      to the specified file.
    chroot_args: An array of arguments for the chroot environment wrapper.
    debug_level: The debug level of RunCommand's output - applies to output
                 coming from subprocess as well.
    error_code_ok: Does not raise an exception when command returns a non-zero
                   exit code.  Instead, returns the CommandResult object
                   containing the exit code.
    kill_timeout: If we're interrupted, how long should we give the invoked
                  process to shutdown from a SIGTERM before we SIGKILL it.
                  Specified in seconds.
    log_output: Log the command and its output automatically.
  Returns:
    A CommandResult object.

  Raises:
    RunCommandError:  Raises exception on error with optional error_message.
  """
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
    except EnvironmentError, e:
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
  if enter_chroot:
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
    # Note we reformat the argument into a form that can be directly
    # copy/pasted into a term- thus the map(repr, cmd) bit needs to stay.
    if cwd:
      logger.log(debug_level, 'RunCommand: %s in %s',
                 ' '.join(map(repr, cmd)), cwd)
    else:
      logger.log(debug_level, 'RunCommand: %r', ' '.join(map(repr, cmd)))

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

      if stdout and not log_stdout_to_file:
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

    if error_ok:
      logger.warning("error_ok is deprecated; use error_code_ok instead."
                     "error_ok will be removed in Q1 2013.  Was invoked "
                     "with args=%r", cmd)

    if not error_ok and not error_code_ok and proc.returncode:
      msg = 'Failed command "%r", cwd=%s, extra env=%r' % (cmd, cwd, extra_env)
      if error_message:
        msg += '\n%s' % error_message
      raise RunCommandError(msg, cmd_result)
  # TODO(sosa): is it possible not to use the catch-all Exception here?
  except OSError, e:
    estr = str(e)
    if e.errno == errno.EACCES:
      estr += '; does the program need `chmod a+x`?'
    if not error_ok:
      raise RunCommandError(estr, CommandResult(cmd=cmd),
                            exception=e)
    else:
      Warning(estr)
  except Exception, e:
    if not error_ok:
      raise
    else:
      Warning(str(e))
  finally:
    if proc is not None:
      # Ensure the process is dead.
      _KillChildProcess(proc, kill_timeout, cmd, None, None, None)

  return cmd_result


# Convenience RunCommand methods
DebugRunCommand = functools.partial(RunCommand, debug_level=logging.DEBUG)


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
  return os.path.exists('/etc/debian_chroot')

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
    match = re.search('CHROMEOS_VERSION_STRING=([0-9_.]+)', str_obj)
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
  """Return domain of current machine, or None if there is no domain."""
  hostname = GetHostName(fully_qualified=True)
  domain = hostname.partition('.')[2]
  return domain if domain else None


class RetriesExhausted(Exception):
  "Exception thrown when all retry attempts are exhausted and no error occured"


def RetryInvocation(return_handler, exc_handler, max_retry, functor, *args,
                    **kwds):
  """Generic retry loop w/ optional break out depending on exceptions.

  Generally speaking you likely want RetryException or RetryReturned
  rather than this; they're wrappers around this and are friendlier for
  end usage.

  Arguments:
    return_handler: A functor invoked with the returned results from
      functor(*args, **kwds).  If it returns True, then a retry
      is attempted.  If False, the result is returned.
      If this value is None, then no retries are attempted for
      non-excepting invocations of functor(*args, **kwds) .
    exc_handler: A functor invoked w/ the exception instance that
      functor(*args, **kwds) threw.  If it returns True, then a
      retry is attempted.  If False, the exception is re-raised.
      If this value is None, then no exception based retries will
      occur.
    max_retry: A positive integer representing how many times to retry
      the command before giving up.  Worst case, the command is invoked
      (max_retry + 1) times before failing.
    functor: A callable to pass args and kargs to.
    args: Positional args passed to functor.
    kwds: Optional args passed to functor.
    sleep: Optional keyword.  Multiplier for how long to sleep between
      retries; will delay (1*sleep) the first time, then (2*sleep),
      continuing via attempt * sleep.
  Returns:
    Whatever functor(*args, **kwds) returns.
  Raises:
    Exception:  Whatever exceptions functor(*args, **kwds) throws and
      isn't suppressed is raised.  Note that the first exception encountered
      is what's thrown; in the absense of an exception (meaning ran out
      of retries based on testing the result), a generic RetriesExhausted
      exception is thrown.
  """

  if max_retry < 0:
    raise ValueError("max_retry needs to be zero or more: %s" % max_retry)
  sleep = kwds.pop('sleep', 0)

  stopper = lambda x: False
  return_handler = stopper if return_handler is None else return_handler
  exc_handler = stopper if exc_handler is None else exc_handler

  exc_info = None
  for attempt in xrange(max_retry + 1):
    if attempt and sleep:
      time.sleep(sleep * attempt)
    try:
      ret = functor(*args, **kwds)
      if not return_handler(ret):
        return ret
    except Exception, e:
      # Note we're not snagging BaseException, so MemoryError/KeyboardInterrupt
      # and friends don't enter this except block.
      if not exc_handler(e):
        raise
      # We intentionally ignore any failures in later attempts since we'll
      # throw the original failure if all retries fail.
      if exc_info is None:
        exc_info = sys.exc_info()

  #pylint: disable=E0702
  if exc_info is None:
    raise RetriesExhausted(max_retry, functor, args, kwds)
  raise exc_info[0], exc_info[1], exc_info[2]


def RetryReturned(ret_retry, max_retry, functor, *args, **kwds):
  """Convience wrapper for RetryInvocation based on the returned value.

  Specifically:
  return RetryInvocation(ret_retry, None, max_retry, functor, *args, **kwds)

  For details of arguments, exceptions, etc, see RetryInvocation.
  """
  return RetryInvocation(ret_retry, None, max_retry, functor, *args, **kwds)


def RetryException(exc_retry, max_retry, functor, *args, **kwds):
  """Convience wrapper for RetryInvocation base on exceptions.

  Specifically:
  return RetryInvocation(None, exc_retry, max_retry, functor, *args, **kwds)

  Args:
    exc_retry: Either a class (or tuple of classes), or a callable
      that is given the raised exception.  If the raise exception
      is the given class(es) or exc_retry(exc) results in True, a
      retry will be attempted.  If False for either, the exception is
      raised.
    *: See RetryInvocation.
  """
  if isinstance(exc_retry, (tuple, type)):
    #pylint: disable=E0102
    def exc_retry(exc, values=exc_retry):
      return isinstance(exc, values)
  return RetryInvocation(None, exc_retry, max_retry, functor, *args, **kwds)


def RetryCommand(functor, max_retry, *args, **kwds):
  """Wrapper for RunCommand that will retry a command

  Arguments:
    functor: RunCommand function to run; retries will only occur on
      RunCommandError exceptions being thrown.
    max_retry: A positive integer representing how many times to retry
      the command before giving up.  Worst case, the command is invoked
      (max_retry + 1) times before failing.
    sleep: Optional keyword.  Multiplier for how long to sleep between
      retries; will delay (1*sleep) the first time, then (2*sleep),
      continuing via attempt * sleep.
    retry_on: If given, it must support containment (ie, lists, sets, etc),
      and retry will only be continued for the given exit codes,
      failing immediately if the exit code isn't one of the allowed
      retry codes.
    args: Positional args passed to RunCommand; see RunCommand for specifics.
    kwds: Optional args passed to RunCommand; see RunCommand for specifics.
  Returns:
    A RunCommandResult object.
  Raises:
    Exception:  Raises RunCommandError on error with optional error_message.
  """
  values = kwds.pop('retry_on', set(xrange(255)))
  def do_retry(exc):
    return isinstance(exc, RunCommandError) and exc.result.returncode in values
  return RetryException(do_retry, max_retry, functor, *args, **kwds)


def RunCommandWithRetries(max_retry, *args, **kwds):
  """Wrapper for RunCommand that will retry a command

  Arguments:
    See RetryCommand and RunCommand; This is just a wrapper around it.
  Returns:
    A RunCommandResult object.
  Raises:
    Exception:  Raises RunCommandError on error with optional error_message.
  """
  return RetryCommand(RunCommand, max_retry, *args, **kwds)


def RunCommandCaptureOutput(cmd, **kwds):
  """Wrapper for RunCommand that captures output.

  This wrapper calls RunCommand with redirect_stdout=True and
  redirect_stderr=True. This is for convenience.

  Arguments:
    cmd: The command to run.
    kwds: Optional args passed to RunCommand; see RunCommand for specifics.
  """
  return RunCommand(cmd, redirect_stdout=kwds.pop('redirect_stdout', True),
                    redirect_stderr=kwds.pop('redirect_stderr', True), **kwds)


def TimedCommand(functor, *args, **kwargs):
  """Wrapper for simple log timing of other python functions.

  If you want to log info about how long it took to run an arbitrary command,
  you would do something like:
    TimedCommand(RunCommand, ['wget', 'http://foo'])

  Arguments:
    functor: The function to run.
    args: The args to pass to the function.
    kwds: Optional args to pass to the function.
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

  Arguments:
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
                  inputs=None, extra_args=None, **kwds):
  """Create a tarball.  Executes 'tar' on the commandline.

  Arguments:
    target: The path of the tar file to generate.
    cwd: The directory to run the tar command.
    sudo: Whether to run with "sudo".
    compression: The type of compression desired.  See the FindCompressor
      function for details.
    chroot: See FindCompressor().
    inputs: A list of files or directories to add to the tarball.  If unset,
      defaults to ".".
    extra_args: Extra args to pass to "tar".
    kwds: Any RunCommand options/overrides to use.

  Returns:
    The cmd_result object returned by the RunCommand invocation.
  """
  if inputs is None:
    inputs = ['.']
  if extra_args is None:
    extra_args = []
  kwds.setdefault('debug_level', logging.DEBUG)

  comp = FindCompressor(compression, chroot=chroot)
  cmd = ['tar'] + extra_args + ['-I', comp, '-cf', target] + inputs
  rc_func = SudoRunCommand if sudo else RunCommand
  return rc_func(cmd, cwd=cwd, **kwds)


def GetInput(prompt):
  """Helper function to grab input from a user.   Makes testing easier."""
  return raw_input(prompt)


def BooleanPrompt(prompt="Do you want to continue?", default=True,
                  true_value='yes', false_value='no'):
  """Helper function for processing boolean choice prompts.

  Args:
    prompt: The question to present to the user.
    default: Boolean to return if the user just presses enter.
    true_value: The text to display that represents a True returned.
    false_value: The text to display that represents a False returned.
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
  while True:
    response = GetInput(prompt).lower()
    if not response:
      return default
    if true_value.startswith(response):
      if not false_value.startswith(response):
        return True
      # common prefix between the two...
    elif false_value.startswith(response):
      return False


# Suppress whacked complaints about abstract class being unused.
#pylint: disable=R0921
class MasterPidContextManager(object):

  """
  Class for context managers that need to run their exit
  strictly from within the same PID.
  """

  # In certain cases we actually want this ran outside
  # of the main pid- specifically in backup processes
  # doing cleanup.

  _ALTERNATE_MASTER_PID = None

  def __init__(self):
    self._invoking_pid = None

  def __enter__(self):
    self._invoking_pid = os.getpid()
    return self._enter()

  def __exit__(self, exc_type, exc, traceback):
    curpid = os.getpid()
    if curpid == self._ALTERNATE_MASTER_PID:
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


def AllowDisabling(enabled, functor, *args, **kwds):
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
    return functor(*args, **kwds)
  return NoOpContextManager()


class TimeoutError(Exception):
  """Raises when code within SubCommandTimeout has been run too long."""


@contextlib.contextmanager
def SubCommandTimeout(max_run_time):
  """ContextManager that alarms if code is ran for too long.

  Unlike Timeout, SubCommandTimeout can run nested and raises a TimeoutException
  if the timeout is reached. SubCommandTimeout can also nest underneath
  Timeout.

  Args:
    max_run_time: Number (integer) of seconds to wait before sending SIGALRM.
  """
  max_run_time = int(max_run_time)
  if max_run_time <= 0:
    raise ValueError("max_run_time must be greater than zero")

  # pylint: disable=W0613
  def kill_us(sig_num, frame):
    raise TimeoutError("Timeout occurred- waited %i seconds." % max_run_time)

  original_handler = signal.signal(signal.SIGALRM, kill_us)
  previous_time = int(time.time())

  # Signal the min in case the leftover time was smaller than this timeout.
  remaining_timeout = signal.alarm(0)
  if remaining_timeout:
    signal.alarm(min(remaining_timeout, max_run_time))
  else:
    signal.alarm(max_run_time)

  try:
    yield
  finally:
    # Cancel the alarm request and restore the original handler.
    signal.alarm(0)
    signal.signal(signal.SIGALRM, original_handler)

    # Ensure the previous handler will fire if it was meant to.
    if remaining_timeout > 0:
      # Signal the previous handler if it would have already passed.
      time_left = remaining_timeout - (int(time.time()) - previous_time)
      if time_left <= 0:
        signals.RelaySignal(original_handler, signal.SIGALRM, None)
      else:
        signal.alarm(time_left)


@contextlib.contextmanager
def Timeout(max_run_time):
  """ContextManager that alarms if code is run for too long.

  This implementation is fairly simple, thus multiple timeouts
  cannot be active at the same time.

  Additionally, if the timeout has elapsed, it'll trigger a SystemExit
  exception w/in the invoking code, ultimately propagating that past
  itself.  If the underlying code tries to suppress the SystemExit, once
  a minute it'll retrigger SystemExit until control is returned to this
  manager.

  Args:
    max_run_time: a positive integer.
  """
  max_run_time = int(max_run_time)
  if max_run_time <= 0:
    raise ValueError("max_run_time must be greater than zero")

  # pylint: disable=W0613
  def kill_us(sig_num, frame):
    # While this SystemExit *should* crash it's way back up the
    # stack to our exit handler, we do have live/production code
    # that uses blanket except statements which could suppress this.
    # As such, keep scheduling alarms until our exit handler runs.
    # Note that there is a potential conflict via this code, and
    # RunCommand's kill_timeout; thus we set the alarming interval
    # fairly high.
    signal.alarm(60)
    raise SystemExit("Timeout occurred- waited %i seconds, failing."
                     % max_run_time)

  original_handler = signal.signal(signal.SIGALRM, kill_us)
  remaining_timeout = signal.alarm(max_run_time)
  if remaining_timeout:
    # Restore things to the way they were.
    signal.signal(signal.SIGALRM, original_handler)
    signal.alarm(remaining_timeout)
    # ... and now complain.  Unfortunately we can't easily detect this
    # upfront, thus the reset dance above.
    raise Exception("_Timeout cannot be used in parallel to other alarm "
                    "handling code; failing")
  try:
    yield
  finally:
    # Cancel the alarm request and restore the original handler.
    signal.alarm(0)
    signal.signal(signal.SIGALRM, original_handler)


def TimeoutDecorator(max_time):
  """Decorator used to ensure a func is interrupted if it's running too long."""
  def decorator(functor):
    def wrapper(self, *args, **kwds):
      with Timeout(max_time):
        return functor(self, *args, **kwds)

    wrapper.__module__ = functor.__module__
    wrapper.__name__ = functor.__name__
    wrapper.__doc__ = functor.__doc__
    return wrapper
  return decorator


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
  with statements."""

  def __init__(self):
    self._stack = []

  def Add(self, functor, *args, **kwds):
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
      obj = functor(*args, **kwds)
      return obj
    finally:
      if obj is not None:
        obj.__enter__()
      self._stack.append(obj)

  def __enter__(self):
    # Nothing to do in this case, since we've already done
    # our entrances.
    return self

  def __exit__(self, exc_type, exc, traceback):
    # Run it in reverse order, tracking the results
    # so we know whether or not to suppress the exception raised
    # (or to switch that exception to a new one triggered by a handlers
    # __exit__).
    for handler in reversed(self._stack):
      # pylint: disable=W0702
      try:
        if handler.__exit__(exc_type, exc, traceback):
          exc_type = exc = traceback = None
      except:
        exc_type, exc, traceback = sys.exc_info()
    if all(x is None for x in (exc_type, exc, traceback)):
      return True

    self._stack = []

    raise exc_type, exc, traceback


def WaitForCondition(func, period, timeout):
  """Periodically run a function, waiting in between runs.

  Continues to run until the function returns a 'True' value.

  Arguments:
    func: The function to run to test for condition.  Returns True to indicate
          condition was met.
    period: How long to wait in between testing of condition.
    timeout: The maximum amount of time to wait.

  Raises:
    TimeoutError when the timeout is exceeded.
  """
  assert period >= 0
  with SubCommandTimeout(timeout):
    timestamp = time.time()
    while not func():
      time_remaining = period - int(time.time() - timestamp)
      if time_remaining > 0:
        time.sleep(time_remaining)
      timestamp = time.time()


def RunCurl(args, **kwargs):
  """Runs curl and wraps around all necessary hacks."""
  cmd = ['curl']
  cmd.extend(args)

  # These values were discerned via scraping the curl manpage; they're all
  # retry related (dns failed, timeout occurred, etc, see  the manpage for
  # exact specifics of each).
  # Note we allow 22 to deal w/ 500's- they're thrown by google storage
  # occasionally.
  # Finally, we do not use curl's --retry option since it generally doesn't
  # actually retry anything; code 18 for example, it will not retry on.
  retriable_exits = frozenset([5, 6, 7, 15, 18, 22, 26, 28, 52, 56])
  try:
    return RunCommandWithRetries(5, cmd, sleep=3, retry_on=retriable_exits,
                                 **kwargs)
  except RunCommandError, e:
    code = e.result.returncode
    if code in (51, 58, 60):
      # These are the return codes of failing certs as per 'man curl'.
      Die('Download failed with certificate error? Try "sudo c_rehash".')
    else:
      try:
        return RunCommandWithRetries(5, cmd, sleep=60, retry_on=retriable_exits,
                                     **kwargs)
      except RunCommandError, e:
        Die("Curl failed w/ exit code %i", code)


def SetupBasicLogging():
  """Sets up basic logging to use format from constants."""
  logging_format = '%(asctime)s - %(filename)s - %(levelname)-8s: %(message)s'
  date_format = constants.LOGGER_DATE_FMT
  logging.basicConfig(level=logging.DEBUG, format=logging_format,
                      datefmt=date_format)


class ApiMismatchError(Exception):
  """Raised by GetTargetChromiteApiVersion."""
  pass


def GetTargetChromiteApiVersion(buildroot, validate_version=True):
  """Get the re-exec API version of the target chromite.

  Arguments:
    buildroot: The directory containing the chromite to check.
    validate_version:  If set to true, checks the target chromite for
      compatibility, and raises an ApiMismatchError when there is an
      incompatibility.

  Raises:
    May raise an ApiMismatchError if validate_version is set.

  Returns the version number in (major, minor) tuple.
  """
  api = RunCommandCaptureOutput(
      [constants.PATH_TO_CBUILDBOT, '--reexec-api-version'],
      cwd=buildroot, error_code_ok=True)
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

  Arguments:
    name: python dotted namespace path of the module to import

  Raises:
    FailedImport if importing fails

  Returns:
    imported module
  """
  m = __import__(name)
  # __import__('foo.bar') returns foo, so...
  for bit in name.split('.')[1:]:
    m = getattr(m, bit)
  return m


def PredicateSplit(func, iterable):
  """Splits an iterable into two groups based on a predicate return value.

  Arguments:
    func:  A functor that takes an item as its argument and returns a boolean
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


def TreeOpen(status_url, sleep_timeout, max_timeout=600):
  """Returns True if the tree is open or throttled.

  At the highest level this function checks to see if the Tree is Open.
  However, it also does a robustified wait as the server hosting the tree
  status page is known to be somewhat flaky and these errors can be handled
  with multiple retries.  In addition, it waits around for the Tree to Open
  based on |max_timeout| to give a greater chance of returning True as it
  expects callees to want to do some operation based on a True value.
  If a caller is not interested in this feature they should set |max_timeout|
  to 0.

  Args:
    status_url: The status url to check i.e.
      'https://status.appspot.com/current?format=json'
    sleep_timeout: How long to sleep when periodically polling for tree open
      status.
    max_timeout: The max length to wait for the tree to open.
  """
  # Limit sleep interval to max_timeout if set.
  if max_timeout > 0:
    sleep_timeout = min(max_timeout, sleep_timeout)

  def _SleepWithExponentialBackOff(current_sleep):
    """Helper function to sleep with exponential backoff."""
    time.sleep(current_sleep)
    return current_sleep * 2

  def _CanSubmit(status_url):
    """Returns the JSON dictionary response from the status url."""
    max_attempts = 5
    current_sleep = 1
    for _ in range(max_attempts):
      try:
        # Check for successful response code.
        response = urllib.urlopen(status_url)
        if response.getcode() == 200:
          data = json.load(response)
          return data['general_state'] in ('open', 'throttled')

      # We remain robust against IOError's and retry.
      except IOError as e:
        logging.error('Could not reach %s: %r', status_url, e)

      current_sleep = _SleepWithExponentialBackOff(current_sleep)
    else:
      # We go ahead and say the tree is open if we can't get the status.
      Warning('Could not get a status from %s', status_url)
      return True

  # Loop until either we run out of time or the tree is open.
  end_time = time.time() + max_timeout
  while True:
    time_left = end_time - time.time()
    if _CanSubmit(status_url):
      return True
    elif time_left <= 0:
      return False
    Info('Waiting for the tree to open (%d minutes left)...', time_left / 60)
    time.sleep(sleep_timeout)


@contextlib.contextmanager
def _Open(input):
  """Convenience ctx that accepts a file path or an already open file object."""
  if isinstance(input, basestring):
    with open(input) as f:
      yield f
  else:
    yield input


def LoadKeyValueFile(input, ignore_missing=False):
  """Turn a key=value file into a dict

  Note: If you're designing a new data store, please use json rather than
  this format.  This func is designed to work with legacy/external files
  where json isn't an option.

  Args:
    input: The file to read.  Can be a path or an open file object.
    ignore_missing: If the file does not exist, return an empty dict.
  Returns:
    a dict of all the key=value pairs found in the file.
  """
  d = {}

  try:
    with _Open(input) as f:
      for raw_line in f:
        line = raw_line.split('#')[0].strip()
        if not line:
          continue
        chunks = line.split('=', 1)
        if len(chunks) != 2:
          raise ValueError('Malformed version file; line %r' % raw_line)
        val = chunks[1].strip()
        if len(val) > 1 and val[0] in "\"'" and val[0] == val[-1]:
          # Only strip quotes if the first & last one match.
          val = val[1:-1]
        d[chunks[0].strip()] = val
  except EnvironmentError, e:
    if not (ignore_missing and e.errno == errno.ENOENT):
      raise

  return d


def SafeRun(functors, combine_exceptions=False):
  """Executes a list of functors, continuing on exceptions.

  Arguments:
    functors: An iterable of functors to call.
    combine_exceptions: If set, and multipole exceptions are encountered,
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

  Arguments:
    timeval: Floating point time value as accepted by gmtime()/localtime(),
             otherwise the current time is used.
  Returns:
    A string format such as 'Wed, 20 Feb 2013 15:25:15 -0500 (EST)'
  """
  return '%s (%s)' % (formatdate(timeval=timeval, localtime=True),
                      time.tzname[0])
