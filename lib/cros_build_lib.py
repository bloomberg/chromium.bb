# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

from datetime import datetime
import errno
import hashlib
import logging
import os
import re
import signal
import socket
# pylint: disable=W0402
import string
import subprocess
import sys
import tempfile
import time
import xml.sax
import functools
import contextlib

# TODO(build): Fix this.
# This should be absolute import, but that requires fixing all
# relative imports first.
_path = os.path.realpath(__file__)
_path = os.path.normpath(os.path.join(os.path.dirname(_path), '..', '..'))
sys.path.insert(0, _path)
from chromite.lib import signals
from chromite.buildbot import constants
# Now restore it so that relative scripts don't get cranky.
sys.path.pop(0)
del _path

STRICT_SUDO = False

_STDOUT_IS_TTY = hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()
EXTERNAL_GERRIT_SSH_REMOTE = 'gerrit'


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
  extra_env = kwds.pop('extra_env', {}).copy()
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

  # TODO(sosa): gpylint complains about redefining built-in 'input'.
  #   Can we rename this variable?
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
  """Marks a stage as having warnings."""
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


def FindRepoDir(path=None):
  """Returns the nearest higher-level repo dir from the specified path.

  Args:
    path: The path to use. Defaults to cwd.
  """
  if path is None:
    path = os.getcwd()
  path = os.path.abspath(path)
  while path != '/':
    repo_dir = os.path.join(path, '.repo')
    if os.path.isdir(repo_dir):
      return repo_dir
    path = os.path.dirname(path)
  return None


def FindRepoCheckoutRoot(path=None):
  """Get the root of your repo managed checkout."""
  repo_dir = FindRepoDir(path)
  if repo_dir:
    return os.path.dirname(repo_dir)
  else:
    return None


def GetProjectDir(cwd, project):
  """Returns the absolute path to a project.

  Args:
    cwd: a directory within a repo-managed checkout.
    project: the name of the project to get the path for.
  """
  return ManifestCheckout.Cached(cwd).GetProjectPath(project, True)


def IsDirectoryAGitRepoRoot(cwd):
  """Checks if there's a git repo rooted at a directory."""
  return os.path.isdir(os.path.join(cwd, '.git'))


def ReinterpretPathForChroot(path):
  """Returns reinterpreted path from outside the chroot for use inside.

  Args:
    path: The path to reinterpret.  Must be in src tree.
  """
  root_path = os.path.join(FindRepoDir(path), '..')

  path_abs_path = os.path.abspath(path)
  root_abs_path = os.path.abspath(root_path)

  # Strip the repository root from the path and strip first /.
  relative_path = path_abs_path.replace(root_abs_path, '')[1:]

  if relative_path == path_abs_path:
    raise Exception('Error: path is outside your src tree, cannot reinterpret.')

  new_path = os.path.join('/home', os.getenv('USER'), 'trunk', relative_path)
  return new_path


_HEX_CHARS = set(string.hexdigits)
def IsSHA1(value, full=True):
  """Returns True if the given value looks like a sha1.

  If full is True, then it must be full length- 40 chars.  If False, >=6, and
  <40."""
  if not all(x in _HEX_CHARS for x in value):
    return False
  l = len(value)
  if full:
    return l == 40
  return l >= 6 and l <= 40


def IsRefsTags(value):
  """Return True if the given value looks like a tag.

  Currently this is identified via refs/tags/ prefixing."""
  return value.startswith("refs/tags/")


def GetGitRepoRevision(cwd, branch='HEAD'):
  """Find the revision of a branch.

  Defaults to current branch.
  """
  return RunGitCommand(cwd, ['rev-parse', branch]).output.strip()


def DoesCommitExistInRepo(cwd, commit_hash):
  """Determine if commit object exists in a repo.

  Args:
    cwd: A directory within the project repo.
    commit_hash: The hash of the commit object to look for.
  """
  return 0 == RunGitCommand(cwd, ['rev-list', '-n1', commit_hash],
      error_code_ok=True).returncode


def DoesLocalBranchExist(repo_dir, branch):
  """Returns True if the local branch exists.

  Args:
    repo_dir: Directory of the git repository to check.
    branch: The name of the branch to test for.
  """
  return os.path.isfile(
      os.path.join(repo_dir, '.git/refs/heads',
                   branch.lstrip('/')))


def GetCurrentBranch(cwd):
  """Returns current branch of a repo, and None if repo is on detached HEAD."""
  try:
    ret = RunGitCommand(cwd, ['symbolic-ref', '-q', 'HEAD'])
    return StripLeadingRefsHeads(ret.output.strip(), False)
  except RunCommandError, e:
    if e.result.returncode != 1:
      raise
    return None


def StripLeadingRefsHeads(ref, strict=True):
  """Remove leading 'refs/heads/' from a ref name.

  If strict is True, an Exception is thrown if the ref doesn't start with
  refs/heads.  If strict is False, the original ref is returned.
  """
  if not ref.startswith('refs/heads/') and strict:
    raise Exception('Ref name %s does not start with refs/heads/' % ref)

  return ref.replace('refs/heads/', '')


def StripLeadingRefs(ref):
  """Remove leading 'refs/heads', 'refs/remotes/[^/]+/' from a ref name."""
  ref = StripLeadingRefsHeads(ref, False)
  if ref.startswith("refs/remotes/"):
    return ref.split("/", 3)[-1]
  return ref


class Manifest(object):
  """SAX handler that parses the manifest document.

  Properties:
    default: the attributes of the <default> tag.
    projects: a dictionary keyed by project name containing the attributes of
              each <project> tag.
  """

  _instance_cache = {}

  def __init__(self, source, manifest_include_dir=None):
    """Initialize this instance.

    Args:
      source: The path to the manifest to parse.  May be a file handle.
      manifest_include_dir: If given, this is where to start looking for
        include targets.
    """

    self.default = {}
    self.projects = {}
    self.remotes = {}
    self.includes = []
    self.revision = None
    self.manifest_include_dir = manifest_include_dir
    self._RunParser(source)
    self.includes = tuple(self.includes)

  def _RunParser(self, source, finalize=True):
    parser = xml.sax.make_parser()
    handler = xml.sax.handler.ContentHandler()
    handler.startElement = self._ProcessElement
    parser.setContentHandler(handler)
    parser.parse(source)
    if finalize:
      # Rewrite projects mixing defaults in and adding our attributes.
      for data in self.projects.itervalues():
        self._FinalizeProjectData(data)

  def _ProcessElement(self, name, attrs):
    """Stores the default manifest properties and per-project overrides."""
    attrs = dict(attrs.items())
    if name == 'default':
      self.default = attrs
    elif name == 'remote':
      self.remotes[attrs['name']] = attrs
    elif name == 'project':
      self.projects[attrs['name']] = attrs
    elif name == 'manifest':
      self.revision = attrs.get('revision')
    elif name == 'include':
      if self.manifest_include_dir is None:
        raise OSError(
            errno.ENOENT, "No manifest_include_dir given, but an include was "
            "encountered; attrs=%r" % (attrs,))
      # Include is calculated relative to the manifest that has the include;
      # thus set the path temporarily to the dirname of the target.
      original_include_dir = self.manifest_include_dir
      include_path = os.path.realpath(
          os.path.join(original_include_dir, attrs['name']))
      self.includes.append((attrs['name'], include_path))
      try:
        # Temporarily mangle the pathway for the context of this includes
        # processing; do this so that any sub includes calculate from
        # relative to that manifest.
        self.manifest_include_dir = include_path
        self._RunParser(include_path, finalize=False)
      finally:
        self.manifest_include_dir = original_include_dir


  def ProjectExists(self, project):
    """Returns True if a project is in this manifest."""
    return os.path.normpath(project) in self.projects

  def GetProjectPath(self, project):
    """Returns the relative path for a project.

    Raises:
      KeyError if the project isn't known."""
    return self.projects[os.path.normpath(project)]['path']

  def _FinalizeProjectData(self, attrs):
    for key in ('remote', 'revision'):
      attrs.setdefault(key, self.default.get(key))

    remote = attrs['remote']
    assert remote in self.remotes

    local_rev = rev = attrs['revision']
    if rev.startswith('refs/heads/'):
      local_rev = 'refs/remotes/%s/%s' % (remote, StripLeadingRefsHeads(rev))

    attrs['local_revision'] = local_rev

    attrs['pushable'] = remote in constants.CROS_REMOTES
    if attrs['pushable']:
      if remote == constants.EXTERNAL_REMOTE:
        attrs['push_remote'] = EXTERNAL_GERRIT_SSH_REMOTE
        if rev.startswith('refs/heads/'):
          attrs['push_remote_local'] = 'refs/remotes/%s/%s' % (
              EXTERNAL_GERRIT_SSH_REMOTE, StripLeadingRefsHeads(rev))
        else:
          attrs['push_remote_local'] = rev
      elif remote == constants.INTERNAL_REMOTE:
        # For cros-internal, it's already accessing gerrit directly; thus
        # just use that.
        attrs['push_remote'] = attrs['remote']
        attrs['push_remote_local'] = attrs['local_revision']

      attrs['push_remote_url'] = constants.CROS_REMOTES[remote]
      attrs['push_url'] = '%s/%s' % (attrs['push_remote_url'], attrs['name'])
    groups = set(attrs.get('groups', 'default').replace(',', ' ').split())
    groups.add('default')
    attrs['groups'] = frozenset(groups)

    # Compute the local ref space.
    # Sanitize a couple path fragments to simplify assumptions in this
    # class, and in consuming code.
    attrs.setdefault('path', attrs['name'])
    for key in ('name', 'path'):
      attrs[key] = os.path.normpath(attrs[key])

  def GetAttributeForProject(self, project, attribute):
    """Gets an attribute for a project, falling back to defaults if needed."""
    return self.projects[project].get(attribute)

  def GetProjectsLocalRevision(self, project):
    """Returns the upstream defined revspec for a project.

    Args:
      project: Which project we're looking at.
    """
    return self.GetAttributeForProject(project, 'local_revision')

  def AssertProjectIsPushable(self, project):
    """Verify that the project has push_* attributes populated."""
    data = self.projects[project]
    if not data['pushable']:
      raise AssertionError('Remote %s is not pushable.' % data['remote'])

  @staticmethod
  def _GetManifestHash(source, ignore_missing=False):
    if isinstance(source, basestring):
      try:
        # TODO(build): convert this to osutils.ReadFile once these
        # classes are moved out into their own module (if possible;
        # may still be cyclic).
        with open(source, 'rb') as f:
          # pylint: disable=E1101
          return hashlib.md5(f.read()).hexdigest()
      except EnvironmentError, e:
        if e.errno != errno.ENOENT or not ignore_missing:
          raise
    source.seek(0)
    # pylint: disable=E1101
    md5 = hashlib.md5(source.read()).hexdigest()
    source.seek(0)
    return md5

  @classmethod
  def Cached(cls, source, manifest_include_dir=None):
    """Return an instance, reusing an existing one if possible.

    May be a seekable filehandle, or a filepath.
    See __init__ for an explanation of these arguments.
    """

    md5 = cls._GetManifestHash(source)
    obj, sources = cls._instance_cache.get(md5, (None, ()))
    if manifest_include_dir is None and sources:
      # We're being invoked in a different way than the orignal
      # caching; disregard the cached entry.
      # Most likely, the instantiation will explode; let it fly.
      obj, sources = None, ()
    for include_target, target_md5 in sources:
      if cls._GetManifestHash(include_target, True) != target_md5:
        obj = None
        break
    if obj is None:
      obj = cls(source, manifest_include_dir=manifest_include_dir)
      sources = tuple((abspath, cls._GetManifestHash(abspath))
                      for (target, abspath) in obj.includes)
      cls._instance_cache[md5] = (obj, sources)

    return obj


class ManifestCheckout(Manifest):
  """A Manifest Handler for a specific manifest checkout."""

  _instance_cache = {}

  # pylint: disable=W0221
  def __init__(self, path, manifest_path=None, search=True):
    """Initialize this instance.

    Args:
      path: Path into a manifest checkout (doesn't have to be the root).
      manifest_path: If supplied, the manifest to use.  Else the manifest
        in the root of the checkout is used.  May be a seekable file handle.
      search: If True, the path can point into the repo, and the root will
        be found automatically.  If False, the path *must* be the root, else
        an OSError ENOENT will be thrown.
    Raises:
      OSError: if a failure occurs.
    """
    self.root, manifest_path = self._NormalizeArgs(
        path, manifest_path, search=search)

    self.manifest_path = os.path.realpath(manifest_path)
    manifest_include_dir = os.path.dirname(self.manifest_path)
    self.manifest_branch = self._GetManifestsBranch(self.root)
    self.default_branch = 'refs/remotes/m/%s' % self.manifest_branch
    self._content_merging = {}
    self.configured_groups = self._GetManifestGroups(self.root)
    Manifest.__init__(self, self.manifest_path,
                      manifest_include_dir=manifest_include_dir)

  @staticmethod
  def _NormalizeArgs(path, manifest_path=None, search=True):
    root = FindRepoCheckoutRoot(path)
    if root is None:
      raise OSError(errno.ENOENT, "Couldn't find repo root: %s" % (path,))
    root = os.path.normpath(os.path.realpath(root))
    if not search:
      if os.path.normpath(os.path.realpath(path)) != root:
        raise OSError(errno.ENOENT, "Path %s is not a repo root, and search "
                      "is disabled." % path)
    if manifest_path is None:
      manifest_path = os.path.join(root, '.repo', 'manifest.xml')
    return root, manifest_path

  def GetProjectsLocalRevision(self, project, fallback=True):
    """Returns the upstream defined revspec for a project.

    Args:
      project: Which project we're looking at.
      fallback: If True, return the revision for revision locked manifests.
        If False, remotes/m/<default_branch> is returned.
    """
    ref = Manifest.GetProjectsLocalRevision(self, project)
    if ref.startswith("refs/") or not fallback:
      return ref
    # Revlocked manifests return sha1s; use the repo defined branch
    # so tracking is supported.
    return self.default_branch

  def ProjectIsContentMerging(self, project):
    """Returns whether the given project has content merging enabled in git.

    Note this functionality should *only* be used against a remote that is
    known to be >=gerrit-2.2; <gerrit-2.2 lacks the required branch holding
    this data thus will trigger a RunCommandError.

    Returns:
      True if content merging is enabled.
    Raises:
      RunCommandError: If the branch can't be fetched due to network
        conditions or if this was invoked against a <gerrit-2.2 server,
        or a mirror that has refs/meta/config stripped from it."""
    result = self._content_merging.get(project)
    if result is None:
      self.AssertProjectIsPushable(project)
      data = self.projects[project]
      self._content_merging[project] = result = _GitRepoIsContentMerging(
          data['local_path'], data['push_remote'])
    return result

  def FindProjectFromPath(self, path):
    """Find the associated projects for a given pathway.

    The pathway can either be to the root of a project, or within the
    project itself (chromite/buildbot for example).  It may be relative
    to the repo root, or an absolute path.  If it is absolute path,
    it's the callers responsibility to ensure the pathway intersects
    the root of the checkout.

    Returns:
      None if no project is found, else the project."""
    # Realpath everything sans the target to keep people happy about
    # how symlinks are handled; exempt the final node since following
    # through that is unlikely even remotely desired.
    tmp = os.path.join(self.root, os.path.dirname(path))
    path = os.path.join(os.path.realpath(tmp), os.path.basename(path))
    path = os.path.normpath(path) + '/'
    candidates = [(x['path'], name) for name, x in self.projects.iteritems()
                  if path.startswith(x['local_path'] + '/')]
    if not candidates:
      return None
    # That which has the greatest common path prefix is the owner of
    # the given pathway, thus we return that.
    return sorted(candidates)[-1][1]

  def _FinalizeProjectData(self, attrs):
    Manifest._FinalizeProjectData(self, attrs)
    attrs['local_path'] = os.path.join(self.root, attrs['path'])

  @staticmethod
  def _GetManifestGroups(root):
    """Discern which manifest groups were enabled for this checkout."""
    path = os.path.join(root, '.repo', 'manifests.git')
    try:
      result = RunGitCommand(path, ['config', '--get', 'manifest.groups'])
    except RunCommandError, e:
      if e.result.returncode == 1:
        # Value wasn't found, which is fine.
        return frozenset(['default'])
      # If exit code 2, multiple values matched (broken checkout).  Anything
      # else, git internal error.
      raise

    result = result.output.replace(',', ' ').split()
    if not result:
      result = ['default']
    return frozenset(result)

  @staticmethod
  def _GetManifestsBranch(root):
    """Get the tracking branch of the manifest repository.

    Returns:
      The branch name.
    """
    # Suppress the normal "if it ain't refs/heads, we don't want none o' that"
    # check for the merge target; repo writes the ambigious form of the branch
    # target for `repo init -u url -b some-branch` usages (aka, 'master'
    # instead of 'refs/heads/master').
    path = os.path.join(root, '.repo', 'manifests')
    current_branch = GetCurrentBranch(path)
    if current_branch != 'default':
      raise OSError(errno.ENOENT,
                    "Manifest repository at %s is checked out to %s.  "
                    "It should be checked out to 'default'."
                    % (root, 'detached HEAD' if current_branch is None
                       else current_branch))

    result = GetTrackingBranchViaGitConfig(
        path, 'default', allow_broken_merge_settings=True, for_checkout=False)

    if result is not None:
      return StripLeadingRefsHeads(result[1], False)

    raise OSError(errno.ENOENT,
                  "Manifest repository at %s is checked out to 'default', but "
                  "the git tracking configuration for that branch is broken; "
                  "failing due to that." % (root,))

  def GetProjectPath(self, project, absolute=False):
    """Returns the path for a project.

    Args:
      project: Project to get the path for.
      absolute:  If True, return an absolute pathway.  If False,
        relative pathway.

    Raises:
      KeyError if the project isn't known."""
    path = Manifest.GetProjectPath(self, project)
    if absolute:
      return os.path.join(self.root, path)
    return path

  # pylint: disable=W0221
  @classmethod
  def Cached(cls, path, manifest_path=None, search=True):
    """Return an instance, reusing an existing one if possible.

    Args:
      path: The pathway into a checkout; the root will be found automatically.
      manifest_path: if given, the manifest.xml to use instead of the
        checkouts internal manifest.  Use with care.
      search: If True, the path can point into the repo, and the root will
        be found automatically.  If False, the path *must* be the root, else
        an OSError ENOENT will be thrown.
    """
    root, manifest_path = cls._NormalizeArgs(path, manifest_path,
                                             search=search)

    md5 = cls._GetManifestHash(manifest_path)
    obj, sources = cls._instance_cache.get((root, md5), (None, ()))
    for include_target, target_md5 in sources:
      if cls._GetManifestHash(include_target, True) != target_md5:
        obj = None
        break
    if obj is None:
      obj = cls(manifest_path)
      sources = tuple((abspath, cls._GetManifestHash(abspath))
                      for (target, abspath) in obj.includes)
      cls._instance_cache[(root, md5)] = (obj, sources)
    return obj

def _GitRepoIsContentMerging(git_repo, remote):
  """Identify if the given git repo has content merging marked.

  This is a gerrit >=2.2 bit of functionality; specifically, the content
  merging configuration is stored in a specially crafted branch which
  we access.  If the branch is fetchable, we either return True or False.

  Args:
    git_repo: The local path to the git repository to inspect.
    remote: The configured remote to use from the given git repository.

  Returns:
    True if content merging is enabled, False if not.
  Raises:
    RunCommandError: Thrown if fetching fails due to either the namespace
      not existing, or a network error intervening.
  """
  # Need to use the manifest to get upstream gerrit; also, if upstream
  # doesn't provide a refs/meta/config for the repo, this will fail.
  RunGitCommand(
      git_repo, ['fetch', remote, 'refs/meta/config:refs/meta/config'])

  content = RunGitCommand(
      git_repo, ['show', 'refs/meta/config:project.config'],
      error_code_ok=True)

  if content.returncode != 0:
    return False

  try:
    result = RunGitCommand(
        git_repo, ['config', '-f', '/dev/stdin', '--get',
                   'submit.mergeContent'], input=content.output)
    return result.output.strip().lower() == 'true'
  except RunCommandError, e:
    # If the field isn't set at all, exit code is 1.
    # Anything else is a bad invocation or an indecipherable state.
    if e.result.returncode != 1:
      raise

  return False

def RunGitCommand(git_repo, cmd, **kwds):
  """RunCommandCaptureOutput wrapper for git commands.

  This suppresses print_cmd, and suppresses output by default.  Git
  functionality w/in this module should use this unless otherwise
  warranted, to standardize git output (primarily, keeping it quiet
  and being able to throw useful errors for it).

  Args:
    git_repo: Pathway to the git repo to operate on.
    cmd: A sequence of the git subcommand to run.  The 'git' prefix is
      added automatically.  If you wished to run 'git remote update',
      this would be ['remote', 'update'] for example.
    kwds: Any RunCommand options/overrides to use.
  Returns:
    A CommandResult object."""
  kwds.setdefault('print_cmd', False)
  Debug("RunGitCommand(%r, %r, **%r)", git_repo, cmd, kwds)
  return RunCommandCaptureOutput(['git'] + cmd, cwd=git_repo, **kwds)


def GetProjectUserEmail(git_repo):
  """Get the email configured for the project ."""
  output = RunGitCommand(git_repo, ['var', 'GIT_COMMITTER_IDENT']).output
  m = re.search('<([^>]*)>', output.strip())
  return m.group(1) if m else None


def GitMatchBranchName(git_repo, pattern, namespace=''):
  """Return branches who match the specified regular expression.

  Args:
    git_repo: The git repository to operate upon.
    pattern: The regexp to search with.
    namespace: The namespace to restrict search to (e.g. 'refs/heads/').
  Returns:
    List of matching branch names (with |namespace| trimmed).
  """
  match = re.compile(pattern, flags=re.I)
  output = RunGitCommand(git_repo, ['ls-remote', git_repo,
                                    namespace + '*']).output
  branches = [x.split()[1] for x in output.splitlines()]
  branches = [x[len(namespace):] for x in branches if x.startswith(namespace)]
  return [x for x in branches if match.search(x)]


class AmbiguousBranchName(Exception):
  """Error if given branch name matches too many branches."""


def GitMatchSingleBranchName(*args, **kwds):
  """Match exactly one branch name, else throw an exception.

  Args:
    See GitMatchBranchName for more details; all args are passed on.
  Returns:
    The branch name.
  Raises:
    raise AmbiguousBranchName if we did not match exactly one branch.
  """
  ret = GitMatchBranchName(*args, **kwds)
  if len(ret) != 1:
    raise AmbiguousBranchName('Did not match exactly 1 branch: %r' % ret)
  return ret[0]


def GetTrackingBranchViaGitConfig(git_repo, branch, for_checkout=True,
                                  allow_broken_merge_settings=False,
                                  recurse=10):
  """Pull the remote and upstream branch of a local branch

  Args:
    git_repo: The git repository to operate upon.
    branch: The branch to inspect.
    for_checkout: Whether to return localized refspecs, or the remote's
      view of it.
    allow_broken_merge_settings: Repo in a couple of spots writes invalid
      branch.mybranch.merge settings; if these are encountered, they're
      normally treated as an error and this function returns None.  If
      this option is set to True, it suppresses this check.
    recurse: If given and the target is local, then recurse through any
      remote=. (aka locals).  This is enabled by default, and is what allows
      developers to have multiple local branches of development dependent
      on one another; disabling this makes that work flow impossible,
      thus disable it only with good reason.  The value given controls how
      deeply to recurse.  Defaults to tracing through 10 levels of local
      remotes. Disabling it is a matter of passing 0.

  Returns:
    A tuple of the remote and the ref name of the tracking branch, or
    None if it couldn't be found.
  """
  try:
    cmd = ['config', '--get-regexp',
           r'branch\.%s\.(remote|merge)' % re.escape(branch)]
    data = RunGitCommand(git_repo, cmd).output.splitlines()

    prefix = 'branch.%s.' % (branch,)
    data = [x.split() for x in data]
    vals = dict((x[0][len(prefix):], x[1]) for x in data)
    if len(vals) != 2:
      if not allow_broken_merge_settings:
        return None
      elif 'merge' not in vals:
        # There isn't anything we can do here.
        return None
      elif 'remote' not in vals:
        # Repo v1.9.4 and up occasionally invalidly leave the remote out.
        # Only occurs for the manifest repo fortunately.
        vals['remote'] = 'origin'
    remote, rev = vals['remote'], vals['merge']
    # Suppress non branches; repo likes to write revisions and tags here,
    # which is wrong (git hates it, nor will it honor it).
    if rev.startswith('refs/remotes/'):
      if for_checkout:
        return remote, rev
      # We can't backtrack from here, or at least don't want to.
      # This is likely refs/remotes/m/ which repo writes when dealing
      # with a revision locked manifest.
      return None
    if not rev.startswith('refs/heads/'):
      # We explicitly don't allow pushing to tags, nor can one push
      # to a sha1 remotely (makes no sense).
      if not allow_broken_merge_settings:
        return None
    elif remote == '.':
      if recurse == 0:
        raise Exception(
            "While tracing out tracking branches, we recursed too deeply: "
            "bailing at %s" % branch)
      return GetTrackingBranchViaGitConfig(
          git_repo, StripLeadingRefsHeads(rev), for_checkout=for_checkout,
          allow_broken_merge_settings=allow_broken_merge_settings,
          recurse=recurse - 1)
    elif for_checkout:
      rev = 'refs/remotes/%s/%s' % (remote, StripLeadingRefsHeads(rev))
    return remote, rev
  except RunCommandError as e:
    # 1 is the retcode for no matches.
    if e.result.returncode != 1:
      raise
  return None


def GetTrackingBranchViaManifest(git_repo, for_checkout=True, for_push=False,
                                 manifest=None):
  """Gets the appropriate push branch via the manifest if possible.

  Args:
    git_repo: The git repo to operate upon.
    for_checkout: Whether to return localized refspecs, or the remote's
      view of it.  Note that depending on the remote, the remote may differ
      if for_push is True or set to False.
    for_push: Controls whether the remote and refspec returned is explicitly
      for pushing.
    manifest: A Manifest instance if one is available, else a
      ManifestCheckout is created and used.

  Returns:
    A tuple of a git target repo and the remote ref to push to, or
    None if it couldnt be found.  If for_checkout, then it returns
    the localized version of it.
  """
  try:
    if manifest is None:
      manifest = ManifestCheckout.Cached(git_repo)

    project = manifest.FindProjectFromPath(git_repo)
    if project is None:
      return None

    if for_push:
      manifest.AssertProjectIsPushable(project)

    data = manifest.projects[project]
    if for_push:
      remote = data['push_remote']
    else:
      remote = data['remote']

    if for_checkout:
      revision = data['local_revision']
      if for_push:
        revision = data['push_remote_local']
    else:
      revision = data['revision']
      if not revision.startswith("refs/heads/"):
        return None

    return remote, revision
  except EnvironmentError, e:
    if e.errno != errno.ENOENT:
      raise
  return None


def GetTrackingBranch(git_repo, branch=None, for_checkout=True, fallback=True,
                      manifest=None, for_push=False):
  """Gets the appropriate push branch for the specified directory.

  This function works on both repo projects and regular git checkouts.

  Assumptions:
   1. We assume the manifest defined upstream is desirable.
   2. No manifest?  Assume tracking if configured is accurate.
   3. If none of the above apply, you get 'origin', 'master' or None,
      depending on fallback.

  Args:
    git_repo: Git repository to operate upon.
    for_checkout: Whether to return localized refspecs, or the remotes
      view of it.
    fallback: If true and no remote/branch could be discerned, return
      'origin', 'master'.  If False, you get None.
      Note that depending on the remote, the remote may differ
      if for_push is True or set to False.
    for_push: Controls whether the remote and refspec returned is explicitly
      for pushing.
    manifest: A Manifest instance if one is available, else a
      ManifestCheckout is created and used.

  Returns:
    A tuple of a git target repo and the remote ref to push to.
  """

  result = GetTrackingBranchViaManifest(git_repo, for_checkout=for_checkout,
                                        manifest=manifest, for_push=for_push)
  if result is not None:
    return result

  if branch is None:
    branch = GetCurrentBranch(git_repo)
  if branch:
    result = GetTrackingBranchViaGitConfig(git_repo, branch,
                                           for_checkout=for_checkout)
    if result is not None:
      if (result[1].startswith('refs/heads/') or
          result[1].startswith('refs/remotes/')):
        return result

  if not fallback:
    return None
  if for_checkout:
    return 'origin', 'refs/remotes/origin/master'
  return 'origin', 'master'


def CreatePushBranch(branch, git_repo, sync=True, remote_push_branch=None):
  """Create a local branch for pushing changes inside a repo repository.

    Args:
      branch: Local branch to create.
      git_repo: Git repository to create the branch in.
      sync: Update remote before creating push branch.
      remote_push_branch: A tuple of the (remote, branch) to push to. i.e.,
                          ('cros', 'master').  By default it tries to
                          automatically determine which tracking branch to use
                          (see GetTrackingBranch()).
  """
  if not remote_push_branch:
    remote, push_branch = GetTrackingBranch(git_repo, for_push=True)
  else:
    remote, push_branch = remote_push_branch

  if sync:
    RetryCommand(RunGitCommand, 3, git_repo, ['remote', 'update', remote],
                 sleep=10, retry_on=(1,))

  RunGitCommand(git_repo, ['checkout', '-B', branch, '-t', push_branch])


def SyncPushBranch(git_repo, remote, rebase_target):
  """Sync and rebase a local push branch to the latest remote version.

    Args:
      git_repo: Git repository to rebase in.
      remote: The remote returned by GetTrackingBranch(for_push=True)
      rebase_target: The branch name returned by GetTrackingBranch().  Must
        start with refs/remotes/ (specifically must be a proper remote
        target rather than an ambiguous name).
  """
  if not rebase_target.startswith("refs/remotes/"):
    raise Exception(
        "Was asked to rebase to a non branch target w/in the push pathways.  "
        "This is highly indicative of an internal bug.  remote %s, rebase %s"
        % (remote, rebase_target))

  RetryCommand(RunGitCommand, 3, git_repo, ['remote', 'update', remote],
               sleep=10, retry_on=(1,))

  try:
    RunGitCommand(git_repo, ['rebase', rebase_target])
  except RunCommandError:
    # Looks like our change conflicts with upstream. Cleanup our failed
    # rebase.
    RunGitCommand(git_repo, ['rebase', '--abort'], error_code_ok=True)
    raise


def GitPushWithRetry(branch, git_repo, dryrun=False, retries=5):
  """General method to push local git changes.

    This method only works with branches created via the CreatePushBranch
    function.

    Args:
      branch: Local branch to push.  Branch should have already been created
        with a local change committed ready to push to the remote branch.  Must
        also already be checked out to that branch.
      git_repo: Git repository to push from.
      dryrun: Git push --dry-run if set to True.
      retries: The number of times to retry before giving up, default: 5

    Raises:
      GitPushFailed if push was unsuccessful after retries
  """
  remote, ref = GetTrackingBranch(git_repo, branch, for_checkout=False,
                                  for_push=True)
  # Don't like invoking this twice, but there is a bit of API
  # impedence here; cros_mark_as_stable
  _, local_ref = GetTrackingBranch(git_repo, branch, for_push=True)

  if not ref.startswith("refs/heads/"):
    raise Exception("Was asked to push to a non branch namespace: %s" % (ref,))

  push_command = ['push', remote, '%s:%s' % (branch, ref)]
  Debug("Trying to push %s to %s:%s", git_repo, branch, ref)

  if dryrun:
    push_command.append('--dry-run')
  for retry in range(1, retries + 1):
    SyncPushBranch(git_repo, remote, local_ref)
    try:
      RunGitCommand(git_repo, push_command)
      break
    except RunCommandError:
      if retry < retries:
        Warning('Error pushing changes trying again (%s/%s)', retry, retries)
        time.sleep(5 * retry)
        continue
      raise

  Info("Successfully pushed %s to %s:%s", git_repo, branch, ref)


def GitCleanAndCheckoutUpstream(git_repo, refresh_upstream=True):
  """Remove all local changes and checkout the latest origin.

  All local changes in the supplied repo will be removed. The branch will
  also be switched to a detached head pointing at the latest origin.

  Args:
    git_repo: Directory of git repository.
    refresh_upstream: If True, run a remote update prior to checking it out.
  """
  remote, local_upstream = GetTrackingBranch(git_repo,
                                             for_push=refresh_upstream)
  RunGitCommand(git_repo, ['am', '--abort'], error_code_ok=True)
  RunGitCommand(git_repo, ['rebase', '--abort'], error_code_ok=True)
  if refresh_upstream:
    RetryCommand(RunGitCommand, 3, git_repo, ['remote', 'update', remote],
                 sleep=10, retry_on=(1,))
  RunGitCommand(git_repo, ['clean', '-df'])
  RunGitCommand(git_repo, ['reset', '--hard', 'HEAD'])
  RunGitCommand(git_repo, ['checkout', local_upstream])


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
    #pylint: disable=W0703
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
      try:
        if handler.__exit__(exc_type, exc, traceback):
          exc_type = exc = traceback = None
      # pylint: disable=W0702
      except:
        exc_type, exc, traceback = sys.exc_info()
    if all(x is None for x in (exc_type, exc, traceback)):
      return True

    self._stack = []

    raise exc_type, exc, traceback


def GetChromiteTrackingBranch():
  """Returns the remote branch associated with chromite."""
  cwd = os.path.dirname(os.path.realpath(__file__))
  result = GetTrackingBranch(cwd, for_checkout=False, fallback=False)
  if result:
    _remote, branch = result
    if branch.startswith('refs/heads/'):
      # Normal scenario.
      return StripLeadingRefsHeads(branch)
    # Reaching here means it was refs/remotes/m/blah, or just plain invalid,
    # or that we're on a detached head in a repo not managed by chromite.

  # Manually try the manifest next.
  try:
    manifest = ManifestCheckout.Cached(cwd)
    # Ensure the manifest knows of this checkout.
    if manifest.FindProjectFromPath(cwd):
      return manifest.manifest_branch
  except EnvironmentError, e:
    if e.errno != errno.ENOENT:
      raise

  # Not a manifest checkout.
  Warning(
      "Chromite checkout at %s isn't controlled by repo, nor is it on a "
      "branch (or if it is, the tracking configuration is missing or broken).  "
      "Falling back to assuming the chromite checkout is derived from "
      "'master'; this *may* result in breakage." % cwd)
  return 'master'


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
