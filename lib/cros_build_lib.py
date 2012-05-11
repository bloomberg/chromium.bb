# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

import errno
import hashlib
import logging
import os
import re
import signal
import socket
import subprocess
import sys
import tempfile
import time
import xml.sax
import functools
import contextlib

# TODO(build, ferringb): Fix this.
# This should be absolute import, but that requires fixing all
# relative imports first.
import signals

STRICT_SUDO = False

_STDOUT_IS_TTY = hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()
YES = 'yes'
NO = 'no'
GERRIT_SSH_REMOTE = 'gerrit'


# Set up basic logging information for all modules that use logging.
logging.basicConfig(
    level=logging.DEBUG,
    format='%(levelname)s: %(asctime)s: %(message)s',
    datefmt='%H:%M:%S')


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
  def __init__(self, msg, result):
    self.result = result
    self.msg = msg
    Exception.__init__(self, msg)
    self.args = (msg, result)

  def __str__(self):
    items = []
    if self.result.error:
      items.append(self.result.error)
    if self.result.output:
      items.append(self.result.output)
    items.append(self.msg)
    out = '\n'.join(items)
    # Python doesn't let you include non-ascii characters in error messages.
    # To be safe, replace all non-ascii characters with xml-escaped versions.
    return out.decode('utf-8').encode('ascii', 'xmlcharrefreplace')

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

  if kwds.pop("strict", True) and STRICT_SUDO:
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
  sudo_cmd.extend('%s=%s' % (k, v)
                  for k, v in kwds.pop('extra_env', {}).iteritems())

  # Finally, block people from passing options to sudo.
  sudo_cmd.append('--')

  if isinstance(cmd, basestring):
    sudo_cmd = '%s %s' % (' '.join(sudo_cmd), cmd)
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
      print 'Ignoring unhandled exception in _KillChildProcess: %s' % (e,)

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


def RunCommand(cmd, print_cmd=True, error_ok=False, error_message=None,
               redirect_stdout=False, redirect_stderr=False,
               cwd=None, input=None, enter_chroot=False, shell=False,
               env=None, extra_env=None, ignore_sigint=False,
               combine_stdout_stderr=False, log_stdout_to_file=None,
               chroot_args=None, debug_level=logging.INFO,
               error_code_ok=False, kill_timeout=1):
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

  # Modify defaults based on parameters.
  # Note that tempfiles must be unbuffered else attempts to read
  # what a separate process did to that file can result in a bad
  # view of the file.
  if log_stdout_to_file:
    stdout = open(log_stdout_to_file, 'w+')
  elif redirect_stdout or mute_output:
    stdout = tempfile.TemporaryFile(bufsize=0)

  if combine_stdout_stderr:
    stderr = subprocess.STDOUT
  elif redirect_stderr or mute_output:
    stderr = tempfile.TemporaryFile(bufsize=0)

  # Work around broken buffering usage in cbuildbot and consumers via
  # forcing the current buffer to be flushed.  Without this, any leading
  # info/error/whatever messages describing this invocation may be left
  # sitting in the buffer, while the invoked program than writes straight
  # to the duped fd; end result, any header we output can land after the
  # actual invocation.  As such, just flush to work around broken code
  # elsewhere.
  if stdout is not None:
    sys.stdout.flush()
  if stderr is not None:
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
  if enter_chroot:
    wrapper = ['cros_sdk']

    if chroot_args:
      wrapper += chroot_args

    if extra_env:
      wrapper.extend('%s=%s' % (k, v) for k, v in extra_env.iteritems())

    cmd = wrapper + ['--'] + cmd

  elif extra_env:
    if env is not None:
      env = env.copy()
    else:
      env = os.environ.copy()

    env.update(extra_env)

  # Print out the command before running.
  if print_cmd:
    if cwd:
      logger.log(debug_level, 'RunCommand: %r in %s', cmd, cwd)
    else:
      logger.log(debug_level, 'RunCommand: %r', cmd)

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

    if not error_ok and not error_code_ok and proc.returncode:
      msg = 'Failed command "%r", cwd=%s, extra env=%r' % (cmd, cwd, extra_env)
      if error_message:
        msg += '\n%s' % error_message
      raise RunCommandError(msg, cmd_result)
  # TODO(sosa): is it possible not to use the catch-all Exception here?
  except OSError, e:
    if not error_ok:
      raise RunCommandError(str(e), CommandResult(cmd=cmd))
    else:
      Warning(str(e))
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


def Die(message, *args):
  """Emits an error message with a stack trace and halts execution.

  Args:
    message: The message to be emitted before exiting.
  """
  logger.exception(message, *args)
  sys.exit(1)


def Error(message, *args, **kwargs):
  """Emits a red warning message using the logging module."""
  logger.error(message, *args, **kwargs)


def Warning(message, *args, **kwargs):
  """Emits a warning message using the logging module."""
  logger.warn(message, *args, **kwargs)


# This command is deprecated in favor of operation.Info()
# It is left here for the moment so people are aware what happened.
# The reason is that this is not aware of the terminal output restrictions such
# as verbose, quiet and subprocess output. You should not be calling this.
def Info(message, *args, **kwargs):
  """Emits an info message using the logging module."""
  logger.info(message, *args, **kwargs)


def Debug(message, *args, **kwargs):
  """Emits a debugging message using the logging module."""
  logger.debug(message, *args, **kwargs)


def PrintBuildbotLink(text, url):
  """Prints out a link to buildbot."""
  print '\n@@@STEP_LINK@%(text)s@%(url)s@@@' % { 'text': text, 'url': url }


def PrintBuildbotStepText(text):
  """Prints out stage text to buildbot."""
  print '\n@@@STEP_TEXT@%(text)s@@@' % { 'text': text }


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


def GetSrcRoot():
  """Get absolute path to src/scripts/ directory.

  Assuming test script will always be run from descendant of src/scripts.

  Returns:
    A string, absolute path to src/scripts directory. None if not found.
  """
  src_root = None
  match_str = '/src/scripts/'
  test_script_path = os.path.abspath('.')

  path_list = re.split(match_str, test_script_path)
  if path_list:
    src_root = os.path.join(path_list[0], match_str.strip('/'))
    Info ('src_root = %r' % src_root)
  else:
    Info ('No %r found in %r' % (match_str, test_script_path))

  return src_root


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


def GetOutputImageDir(board, cros_version):
  """Construct absolute path to output image directory.

  Args:
    board: a string.
    cros_version: a string, Chrome OS version.

  Returns:
    a string: absolute path to output directory.
  """
  src_root = GetSrcRoot()
  rel_path = 'build/images/%s' % board
  # ASSUME: --build_attempt always sets to 1
  version_str = '-'.join([cros_version, 'a1'])
  output_dir = os.path.join(os.path.dirname(src_root), rel_path, version_str)
  Info ('output_dir = %s' % output_dir)
  return output_dir


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


def IsProjectInternal(cwd, project):
  """Checks if project is internal."""
  build_root = FindRepoCheckoutRoot(cwd)
  handler = ParseFullManifest(build_root)
  remote = handler.GetAttributeForProject(project, 'remote')
  if not remote:
    raise Exception('Project %s has no remotes specified in manifest!'
                    % project)
  elif remote not in ('cros', 'cros-internal'):
    raise Exception("Project %s remote is neither 'cros' nor 'cros-internal'"
                     % project)

  return remote == 'cros-internal'


def DoesProjectExist(cwd, project):
  """Returns whether the project exists in the repository.

  Args:
    cwd: a directory within a repo-managed checkout.
    project: the name of the project
  """
  build_root = FindRepoCheckoutRoot(cwd)
  handler = ParseFullManifest(build_root)
  return project in handler.projects


def GetProjectDir(cwd, project):
  """Returns the absolute path to a project.

  Args:
    cwd: a directory within a repo-managed checkout.
    project: the name of the project to get the path for.
  """
  build_root = FindRepoCheckoutRoot(cwd)
  handler = ParseFullManifest(build_root)
  return os.path.join(build_root, handler.projects[project]['path'])


def IsDirectoryAGitRepoRoot(cwd):
  """Checks if there's a git repo rooted at a directory."""
  return os.path.isdir(os.path.join(cwd, '.git'))


def IsProjectManagedByRepo(cwd):
  """Checks if the git repo rooted at a directory is managed by 'repo'"""
  repo_dir = os.path.realpath(FindRepoDir(cwd))
  git_object_dir = os.path.realpath(os.path.join(cwd, '.git/objects'))
  return git_object_dir.startswith(repo_dir)


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


def GetGitRepoRevision(cwd, branch='HEAD'):
  """Find the revision of a branch.

  Defaults to current branch.
  """
  result = RunCommand(['git', 'rev-parse', branch], cwd=cwd,
                      redirect_stdout=True)
  return result.output.strip()


def DoesCommitExistInRepo(cwd, commit_hash):
  """Determine if commit object exists in a repo.

  Args:
    cwd: A directory within the project repo.
    commit_hash: The hash of the commit object to look for.
  """
  result = RunCommand(['git', 'log', '-n1', commit_hash], error_code_ok=True,
                      cwd=cwd)
  return result.returncode == 0


def DoesLocalBranchExist(repo_dir, branch):
  """Returns True if the local branch exists.

  Args:
    repo_dir: Directory of the git repository to check.
    branch: The name of the branch to test for.
  """
  return branch in os.listdir(os.path.join(repo_dir, '.git/refs/heads'))


def GetCurrentBranch(cwd):
  """Returns current branch of a repo, and None if repo is on detached HEAD."""
  try:
    current_branch = RunCommand(['git', 'symbolic-ref', '-q', 'HEAD'], cwd=cwd,
                                redirect_stdout=True,
                                print_cmd=False).output.strip()
    current_branch = current_branch.replace('refs/heads/', '')
  except RunCommandError:
    return None
  return current_branch


def StripLeadingRefsHeads(ref):
  """Remove leading 'refs/heads' from a ref name."""
  if not ref.startswith('refs/heads/'):
    raise Exception('Ref name %s does not start with refs/heads/' % ref)

  return ref.replace('refs/heads/', '')


class ManifestHandler(xml.sax.handler.ContentHandler):
  """SAX handler that parses the manifest document.

  Properties:
    default: the attributes of the <default> tag.
    projects: a dictionary keyed by project name containing the attributes of
              each <project> tag.
  """
  def __init__(self):
    self.default = None
    self.projects = {}

  @classmethod
  def ParseManifest(cls, manifest_path):
    """Returns a handler with the parsed results of the manifest."""
    parser = xml.sax.make_parser()
    handler = cls()
    parser.setContentHandler(handler)
    parser.parse(manifest_path)
    return handler

  def startElement(self, name, attributes):
    """Stores the default manifest properties and per-project overrides."""
    if name == 'default':
      self.default = attributes
    if name == 'project':
      self.projects[attributes['name']] = attributes

  def GetAttributeForProject(self, project, attribute):
    """Gets an attribute for a project, falling back to defaults if needed."""
    return self.projects[project].get(attribute, self.default.get(attribute))


# We cache these results mainly since our calling patterns are fairly
# inefficient about holding onto previously parsed manifest instances.
# Longer term, this should be removed.
_MANIFEST_CACHE = {}
def ParseFullManifest(buildroot):
  """Parse the full manifest for the specified buildroot.

  Args:
    buildroot: The root directory of the repo-managed checkout.

  Returns:
    A ManifestHandler object, pointing at the full manifest.
  """
  manifest_path = os.path.join(buildroot, '.repo', 'manifests/full.xml')
  with open(manifest_path, 'rb') as f:
    md5 = hashlib.md5(f.read()).hexdigest()

  obj = _MANIFEST_CACHE.get(md5)
  if obj is None:
    obj = _MANIFEST_CACHE[md5] = ManifestHandler.ParseManifest(manifest_path)

  return obj


def GetProjectManifestBranch(buildroot, project):
  """Return the branch specified in the manifest for a project.

  Args:
    buildroot: The root directory of the repo-managed checkout.
    project: The name of the project.

  Returns:
    A tuple of the remote and ref name specified in the manifest - i.e.,
    ('cros', 'refs/heads/master').
  """
  handler = ParseFullManifest(buildroot)
  return tuple(handler.GetAttributeForProject(project, key)
               for key in ['remote', 'revision'])


def GetProjectUserEmail(cwd, quiet=False):
  """Get the email configured for the project ."""
  output = RunCommand(['git', 'var', 'GIT_COMMITTER_IDENT'],
                      redirect_stdout=True, print_cmd=(not quiet),
                      cwd=cwd).output.strip()
  m = re.search('<([^>]*)>', output)
  return m.group(1) if m else None



def GetManifestDefaultBranch(cwd):
  """Gets the manifest checkout branch from the manifest."""
  manifest = RunCommand(['repo', 'manifest', '-o', '-'], print_cmd=False,
                        redirect_stdout=True, cwd=cwd).output
  m = re.search(r'<default[^>]*revision="(refs/heads/[^"]*)"', manifest)
  assert m, "Can't find default revision in manifest"
  ref = m.group(1)
  return StripLeadingRefsHeads(ref)


class NoTrackingBranchException(Exception):
  """Raised by GetTrackingBranch."""
  pass


def GetTrackingBranch(branch, cwd):
  """Get the tracking branch of a branch.

  Returns:
    A tuple of the remote and the ref name of the tracking branch.

  Raises:
    NoTrackingBranchException if the passed in branch is not tracking anything.
  """
  KEY_NOT_FOUND_ERROR_CODE = 1
  info = {}
  try:
    for key in ('remote', 'merge'):
      cmd = ['git', 'config', 'branch.%s.%s' % (branch, key)]
      info[key] = RunCommand(cmd, redirect_stdout=True, cwd=cwd).output.strip()
  except RunCommandError as e:
    if e.result.returncode == KEY_NOT_FOUND_ERROR_CODE:
      raise NoTrackingBranchException()
    else:
      raise e

  return info['remote'], info['merge']


def GetPushBranch(cwd):
  """Gets the appropriate push branch for the specified directory.

  This function works on both repo projects and regular git checkouts.

  Assumptions:
   1. For repo checkouts, we assume that the gerrit remote is already set up.
      For cbuildbot checkouts, the sync stage sets up the gerrit remote.
   2. For non-repo checkouts, we assume that you have checked out from
      origin/master.

  Args:
    cwd: Directory to look in.
  """
  try:
    # For valid repo projects, we push to the 'gerrit' remote which uses SSH.
    output = RunCommand(['repo', 'forall', '.', '-c', 'echo $REPO_PROJECT'],
                        print_cmd=False, redirect_stdout=True,
                        redirect_stderr=True, cwd=cwd).output
  except RunCommandError:
    # For other repositories, we push to origin/master.
    return 'origin', 'master'

  repo_root = FindRepoCheckoutRoot(cwd)
  remote_branch = GetProjectManifestBranch(repo_root, output.rstrip())[1]
  return GERRIT_SSH_REMOTE, StripLeadingRefsHeads(remote_branch)


def CreatePushBranch(branch, cwd, sync=True, remote_push_branch=None):
  """Create a local branch for pushing changes inside a repo repository.

    Args:
      branch: Local branch to create.
      cwd: Directory to create the branch in.
      sync: Update remote before creating push branch.
      remote_push_branch: A tuple of the (remote, branch) to push to. i.e.,
                          ('cros', 'master').  By default it tries to
                          automatically determine which tracking branch to use
                          (see GetPushBranch()).
  """
  if not remote_push_branch:
    remote, push_branch = GetPushBranch(cwd)
  else:
    remote, push_branch = remote_push_branch

  if sync:
    RunCommand(['git', 'remote', 'update', remote], cwd=cwd)
  RunCommand(['git', 'checkout', '-B', branch, '-t',
              '%s/%s' % (remote, push_branch)], cwd=cwd)


def SyncPushBranch(cwd, remote, push_branch):
  """Sync a push branch to the latest remote version.

    Args:
      cwd: Directory to rebase in.
      remote: The remote returned by GetPushBranch()
      push_branch: The branch name returned by GetPushBranch().
  """
  RunCommand(['git', 'remote', 'update', remote], cwd=cwd)
  try:
    RunCommand(['git', 'rebase', '%s/%s' % (remote, push_branch)], cwd=cwd)
  except RunCommandError:
    # Looks like our change conflicts with upstream. Cleanup our failed
    # rebase.
    RunCommand(['git', 'rebase', '--abort'], error_ok=True, cwd=cwd)
    raise


def GitPushWithRetry(branch, cwd, dryrun=False, retries=5):
  """General method to push local git changes.

    This method only works with branches created via the CreatePushBranch
    function.

    Args:
      branch: Local branch to push.  Branch should have already been created
        with a local change committed ready to push to the remote branch.  Must
        also already be checked out to that branch.
      cwd: Directory to push in.
      dryrun: Git push --dry-run if set to True.
      retries: The number of times to retry before giving up, default: 5

    Raises:
      GitPushFailed if push was unsuccessful after retries
  """
  remote, ref = GetTrackingBranch(branch, cwd)
  push_branch = StripLeadingRefsHeads(ref)

  for retry in range(1, retries + 1):
    SyncPushBranch(cwd, remote, push_branch)
    push_command = ['git', 'push', remote, '%s:%s' % (branch, push_branch)]
    if dryrun:
      push_command.append('--dry-run')
    try:
      RunCommand(push_command, cwd=cwd)
      break
    except RunCommandError:
      if retry < retries:
        print 'Error pushing changes trying again (%s/%s)' % (retry, retries)
        time.sleep(5 * retry)
      else:
        raise


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


def RunCommandWithRetries(max_retry, *args, **kwds):
  """Wrapper for RunCommand that will retry a command

  Arguments:
    max_retry: A positive integer representing how many times to retry
      the command before giving up.  Worst case, the command is invoked
      (max_retry + 1) times before failing.
    sleep: Multiplier for how long to sleep between retries; will delay
           (1*sleep) the first time, then (2*sleep), then ...
    args: Positional args passed to RunCommand; see RunCommand for specifics.
    kwds: Optional args passed to RunCommand; see RunCommand for specifics.
  Returns:
    A RunCommandResult object.
  Raises:
    Exception:  Raises RunCommandError on error with optional error_message.
  """
  sleep = kwds.pop('sleep', 0)
  try:
    return RunCommand(*args, **kwds)
  except TerminateRunCommandError:
    raise
  except RunCommandError:
    # pylint: disable=W0612
    for attempt in xrange(max_retry):
      time.sleep(sleep * (attempt + 1))
      try:
        return RunCommand(*args, **kwds)
      except TerminateRunCommandError:
        # Unfortunately, there is no right answer for this case- do we expose
        # the original error?  Or do we indicate we were told to die?
        # Right now we expose that we were sigtermed, this is open for debate.
        raise
      except RunCommandError:
        # We intentionally ignore any failures in later attempts since we'll
        # throw the original failure if all retries fail.
        pass
    raise


def RunCommandCaptureOutput(cmd, **kwds):
  """Wrapper for RunCommand that captures output.

  This wrapper calls RunCommand with redirect_stdout=True and
  redirect_stderr=True. This is for convenience.

  Arguments:
    cmd: The command to run.
    kwds: Optional args passed to RunCommand; see RunCommand for specifics.
  """
  return RunCommand(cmd, redirect_stdout=True, redirect_stderr=True, **kwds)


def GetInput(prompt):
  """Helper function to grab input from a user.   Makes testing easier."""
  return raw_input(prompt)


def YesNoPrompt(default, prompt="Do you want to continue", warning="",
                full=False):
  """Helper function for processing yes/no inputs from user.

  Args:
    default: Answer selected if the user hits "enter" without typing anything.
    prompt: The question to present to the user.
    warning: An optional warning to issue before the prompt.
    full: If True, user has to type "yes" or "no", otherwise "y" or "n" is OK.

  Returns:
    What the user entered, normalized to "yes" or "no".
  """
  if warning:
    Warning(warning)

  if full:
    if default == NO:
      # ('yes', 'No')
      yes, no = YES, NO[0].upper() + NO[1:]
    else:
      # ('Yes', 'no')
      yes, no = YES[0].upper() + YES[1:], NO
    expy = [YES]
    expn = [NO]
  else:
    if default == NO:
      # ('y', 'N')
      yes, no = YES[0].lower(), NO[0].upper()
    else:
      # ('Y', 'n')
      yes, no = YES[0].upper(), NO[0].lower()
    # expy = ['y', 'ye', 'yes'], expn = ['n', 'no']
    expy = [YES[0:i + 1] for i in xrange(len(YES))]
    expn = [NO[0:i + 1] for i in xrange(len(NO))]

  prompt = ('\n%s (%s/%s)? ' % (prompt, yes, no))
  while True:
    response = GetInput(prompt).lower()
    if not response:
      response = default
    if response in expy:
      return YES
    elif response in expn:
      return NO


def SafeMakedirs(path, mode=0775, sudo=False):
  """Make parent directories if needed.  Ignore if existing.

  Arguments:
    path: The path to create.  Intermediate directories will be created as
          needed.
    mode: The access permissions in the style of chmod
    sudo: If True, create it via sudo, thus root owned.
  Raises:
    EnvironmentError: if the makedir failed and it was non sudo.
    RunCommandError: If sudo mode, and the command failed for any reason.
  """
  if sudo:
    SudoRunCommand(['mkdir', '-p', '--mode', oct(mode), path],
                   print_cmd=False)
    return
  try:
    os.makedirs(path, mode)
  except EnvironmentError, e:
    if e.errno != errno.EEXIST:
      raise


# Suppress whacked complaints about abstract class being unused.
#pylint: disable=R0921
class MasterPidContextManager(object):

  """
  Class for context managers that need to run their exit
  strictly from within the same PID.
  """

  def __init__(self):
    self._invoking_pid = None

  def __enter__(self):
    self._invoking_pid = os.getpid()
    return self._enter()

  def __exit__(self, exc_type, exc, traceback):
    if self._invoking_pid == os.getpid():
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
    max_run_time: a positive integer.
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


# Support having this module test itself if run as __main__, by leveraging
# the corresponding unittest module.
# Also, the unittests serve as extra documentation.
if __name__ == '__main__':
  import cros_build_lib_unittest
  cros_build_lib_unittest.unittest.main(cros_build_lib_unittest)
