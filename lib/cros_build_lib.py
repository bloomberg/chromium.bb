# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

import inspect
import os
import re
import signal
import subprocess
import sys
import time
from terminal import Color
import xml.sax


_STDOUT_IS_TTY = hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()

class GitPushFailed(Exception):
  """Raised when a git push failed after retry."""
  pass

class CommandResult(object):
  """An object to store various attributes of a child process."""

  def __init__(self):
    self.cmd = None
    self.error = None
    self.output = None
    self.returncode = None


class RunCommandError(Exception):
  """Error caught in RunCommand() method."""
  def __init__(self, msg, cmd, error_code):
    self.cmd = cmd
    self.error_code = error_code
    Exception.__init__(self, msg)
    self.args = (msg, cmd, error_code)

  def __eq__(self, other):
    return (type(self) == type(other) and
            str(self) == str(other) and
            self.error_code == other.error_code and
            self.cmd == other.cmd)

  def __ne__(self, other):
    return not self.__eq__(other)

def RunCommand(cmd, print_cmd=True, error_ok=False, error_message=None,
               exit_code=False, redirect_stdout=False, redirect_stderr=False,
               cwd=None, input=None, enter_chroot=False, shell=False,
               env=None, extra_env=None, ignore_sigint=False,
               combine_stdout_stderr=False, log_stdout_to_file=None):
  """Runs a command.

  Args:
    cmd: cmd to run.  Should be input to subprocess.Popen. If a string, shell
      must be true. Otherwise the command must be an array of arguments, and
      shell must be false.
    print_cmd: prints the command before running it.
    error_ok: does not raise an exception on error.
    error_message: prints out this message when an error occurrs.
    exit_code: returns the return code of the shell command.
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
    combine_stdout_stderr: Combines stdout and stdin streams into stdout.
    log_stdout_to_file: If set, redirects stdout to file specified by this path.
      If combine_stdout_stderr is set to True, then stderr will also be logged
      to the specified file.

  Returns:
    A CommandResult object.

  Raises:
    RunCommandError:  Raises exception on error with optional error_message.
  """
  # Set default for variables.
  stdout = None
  stderr = None
  stdin = None
  file_handle = None
  cmd_result = CommandResult()

  # Modify defaults based on parameters.
  if log_stdout_to_file:
    file_handle = open(log_stdout_to_file, 'w+')
    stdout = file_handle
    if combine_stdout_stderr:
      stderr = file_handle
    elif redirect_stderr:
      stderr = subprocess.PIPE
  else:
    if redirect_stdout: stdout = subprocess.PIPE
    if redirect_stderr: stderr = subprocess.PIPE
    if combine_stdout_stderr: stderr = subprocess.STDOUT
  # TODO(sosa): gpylint complains about redefining built-in 'input'.
  #   Can we rename this variable?
  if input: stdin = subprocess.PIPE

  if isinstance(cmd, basestring):
    if not shell:
      raise Exception('Cannot run a string command without a shell')
    cmd = ['/bin/sh', '-c', cmd]
    shell = False
  elif shell:
    raise Exception('Cannot run an array command with a shell')

  # If we are using enter_chroot we need to use enterchroot pass env through
  # to the final command.
  if enter_chroot:
    cmd = ['cros_sdk', '--'] + cmd

    if extra_env:
      for (key, value) in extra_env.items():
        cmd.insert(1, '%s=%s' % (key, value))

  elif extra_env:
    if env is not None:
      env = env.copy()
    else:
      env = os.environ.copy()

    env.update(extra_env)

  # Print out the command before running.
  if print_cmd:
    if cwd:
      Info('RunCommand: %r in %s' % (cmd, cwd))
    else:
      Info('RunCommand: %r' % cmd)
  cmd_result.cmd = cmd

  try:
    proc = subprocess.Popen(cmd, cwd=cwd, stdin=stdin, stdout=stdout,
                            stderr=stderr, shell=False, env=env,
                            close_fds=True)
    if ignore_sigint:
      old_sigint = signal.signal(signal.SIGINT, signal.SIG_IGN)
    try:
      (cmd_result.output, cmd_result.error) = proc.communicate(input)
    finally:
      if ignore_sigint:
        signal.signal(signal.SIGINT, old_sigint)

    if exit_code:
      cmd_result.returncode = proc.returncode

    if not error_ok and proc.returncode:
      msg = ('Command "%r" with extra env %r\n' % (cmd, extra_env) +
             (error_message or cmd_result.error or cmd_result.output or ''))
      raise RunCommandError(msg, cmd, proc.returncode)
  # TODO(sosa): is it possible not to use the catch-all Exception here?
  except OSError, e:
    if not error_ok:
      raise RunCommandError(str(e), cmd, None)
    else:
      Warning(str(e))
  except Exception, e:
    if not error_ok:
      raise
    else:
      Warning(str(e))
  finally:
    if file_handle:
      file_handle.close()

  return cmd_result


#TODO(sjg): Remove this in favor of operation.Die
def Die(message):
  """Emits a red error message and halts execution.

  Args:
    message: The message to be emitted before exiting.
  """
  print >> sys.stderr, (
      Color(_STDOUT_IS_TTY).Color(Color.RED, '\nERROR: ' + message))
  sys.exit(1)


#TODO(sjg): Remove this in favor of operation.Warning
# pylint: disable-msg=W0622
def Warning(message):
  """Emits a yellow warning message and continues execution.

  Args:
    message: The message to be emitted.
  """
  print >> sys.stderr, (
      Color(_STDOUT_IS_TTY).Color(Color.YELLOW, '\nWARNING: ' + message))


# This command is deprecated in favor of operation.Info()
# It is left here for the moment so people are aware what happened.
# The reason is that this is not aware of the terminal output restrictions such
# as verbose, quiet and subprocess output. You should not be calling this.
def Info(message):
  """Emits a blue informational message and continues execution.

  Args:
    message: The message to be emitted.
  """
  print >> sys.stderr, (
      Color(_STDOUT_IS_TTY).Color(Color.BLUE, '\nINFO: ' + message))


def ListFiles(base_dir):
  """Recurively list files in a directory.

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

  Assuming test script will always be run from descendent of src/scripts.

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


def GetProjectDir(cwd, project):
  """Returns the absolute path to a project.

  Args:
    cwd: a directory within a repo-managed checkout.
    project: the name of the project to get the path for.
  """
  build_root = FindRepoCheckoutRoot(cwd)
  manifest_path = os.path.join(build_root, '.repo', 'manifests/full.xml')
  handler = ManifestHandler.ParseManifest(manifest_path)
  return os.path.join(build_root, handler.projects[project]['path'])


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
    current_branch = RunCommand(['git', 'symbolic-ref', 'HEAD'], cwd=cwd,
                                redirect_stdout=True).output.strip()
    current_branch = current_branch.replace('refs/heads/', '')
  except RunCommandError:
    return None
  return current_branch


def GetShortBranchName(remote, ref):
  """Return branch name in the form 'cros/master' given a remote and a ref.

  Args:
    remote: The git remote name - i.e., 'cros'
    ref: The ref that exists on the remote - i.e., 'refs/heads/master'

  Returns:
    Concatenated name of the ref - i.e., 'cros/master'
  """
  assert(ref.startswith('refs/heads/'))
  return os.path.join(remote, ref.replace('refs/heads/', ''))


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
    pass

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


def GetProjectManifestBranch(buildroot, project):
  """Return the branch specified in the manifest for a project.

  Args:
    buildroot: The root directory of the repo-managed checkout.
    project: The name of the project.

  Returns:
    A tuple of the remote and ref name specified in the manifest - i.e.,
    ('cros', 'refs/heads/master').
  """
  # We can't use .repo/manifest.xml since it may be overwritten by sync stage
  manifest_path = os.path.join(buildroot, '.repo', 'manifests/full.xml')
  handler = ManifestHandler.ParseManifest(manifest_path)

  project_branch = {}
  for key in ['remote', 'revision']:
    if key in handler.projects[project]:
      project_branch[key] = handler.projects[project][key]
    else:
      project_branch[key] = handler.default[key]

  return project_branch['remote'], project_branch['revision']


def GetManifestDefaultBranch(cwd):
  """Gets the manifest checkout branch from the manifest."""
  manifest = RunCommand(['repo', 'manifest', '-o', '-'], redirect_stdout=True,
                        cwd=cwd).output
  m = re.search(r'<default[^>]*revision="(refs/heads/[^"]*)"', manifest)
  assert m, "Can't find default revision in manifest"
  ref = m.group(1)
  assert ref.startswith('refs/heads/')
  return ref.replace('refs/heads/', '')


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
    if e.error_code == KEY_NOT_FOUND_ERROR_CODE:
      raise NoTrackingBranchException()
    else:
      raise e

  return info['remote'], info['merge']


def GetPushBranch(branch, cwd):
  """Gets the appropriate push branch for the specified branch / directory.

  If branch has a valid tracking branch, we should push to that branch. If
  the tracking branch is a revision, we can't push to that, so we should look
  at the default branch from the manifest.

  Args:
    branch: Branch to examine for tracking branch.
    cwd: Directory to look in.
  """
  (remote, merge) = GetTrackingBranch(branch, cwd)
  if not merge.startswith('refs/heads/'):
    # If tracking branch is a revision, use the default manifest branch.
    # This won't work for projects like kernel that override the default
    # manifest branch.  But we are not pushing to them, so things are
    # good for now.
    merge = 'refs/heads/' + GetManifestDefaultBranch(cwd)

  return remote, merge.replace('refs/heads/', '')


def GitPushWithRetry(branch, cwd, dryrun=False, retries=5):
  """General method to push local git changes.

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
  remote, push_branch = GetPushBranch(branch, cwd)
  for retry in range(1, retries + 1):
    try:
      RunCommand(['git', 'remote', 'update'], cwd=cwd)
      try:
        RunCommand(['git', 'rebase', '%s/%s' % (remote, push_branch)], cwd=cwd)
      except RunCommandError:
        # Looks like our change conflicts with upstream. Cleanup our failed
        # rebase.
        RunCommand(['git', 'rebase', '--abort'], error_ok=True, cwd=cwd)
        raise
      push_command = ['git', 'push', remote, '%s:%s' % (branch, push_branch)]
      if dryrun:
        push_command.append('--dry-run')

      RunCommand(push_command, cwd=cwd)
      break
    except RunCommandError:
      if retry < retries:
        print 'Error pushing changes trying again (%s/%s)' % (retry, retries)
        time.sleep(5 * retry)
  else:
    raise GitPushFailed('Failed to push change after %s retries' % retries)


def GetCallerName():
  """Returns the name of the calling module with __main__."""
  top_frame = inspect.stack()[-1][0]
  return os.path.basename(top_frame.f_code.co_filename)


class RunCommandException(Exception):
  """Raised when there is an error in OldRunCommand."""
  def __init__(self, msg, cmd):
    self.cmd = cmd
    Exception.__init__(self, msg)
    self.args = (msg, cmd)

  def __eq__(self, other):
    return (type(self) == type(other) and
            str(self) == str(other) and
            self.cmd == other.cmd)

  def __ne__(self, other):
    return not self.__eq__(other)


def OldRunCommand(cmd, print_cmd=True, error_ok=False, error_message=None,
                  exit_code=False, redirect_stdout=False, redirect_stderr=False,
                  cwd=None, input=None, enter_chroot=False, num_retries=0):
  """Legacy run shell command.

  Arguments:
    cmd: cmd to run.  Should be input to subprocess.POpen.  If a string,
      converted to an array using split().
    print_cmd: prints the command before running it.
    error_ok: does not raise an exception on error.
    error_message: prints out this message when an error occurrs.
    exit_code: returns the return code of the shell command.
    redirect_stdout: returns the stdout.
    redirect_stderr: holds stderr output until input is communicated.
    cwd: the working directory to run this cmd.
    input: input to pipe into this command through stdin.
    enter_chroot: this command should be run from within the chroot.  If set,
      cwd must point to the scripts directory.
    num_retries: the number of retries to perform before dying

  Returns:
    If exit_code is True, returns the return code of the shell command.
    Else returns the output of the shell command.

  Raises:
    Exception:  Raises RunCommandException on error with optional error_message.
  """
  # Set default for variables.
  stdout = None
  stderr = None
  stdin = None
  output = ''

  # Modify defaults based on parameters.
  if redirect_stdout:  stdout = subprocess.PIPE
  if redirect_stderr:  stderr = subprocess.PIPE
  if input:  stdin = subprocess.PIPE
  if enter_chroot:
    cmd = ['cros_sdk', '--'] + cmd

  # Print out the command before running.
  if print_cmd:
    Info('PROGRAM(%s) -> RunCommand: %r in dir %s' %
         (GetCallerName(), cmd, cwd))

  for retry_count in range(num_retries + 1):
    try:
      proc = subprocess.Popen(cmd, cwd=cwd, stdin=stdin,
                              stdout=stdout, stderr=stderr,
                              close_fds=True)
      (output, error) = proc.communicate(input)
      if exit_code and retry_count == num_retries:
        return proc.returncode

      if proc.returncode == 0:
        break

      raise RunCommandException('Command "%r" failed.\n' % (cmd) +
                                (error_message or error or output or ''),
                                cmd)
    except RunCommandException as e:
      if not error_ok and retry_count == num_retries:
        raise e
      else:
        Warning(str(e))
        if print_cmd:
          Info('PROGRAM(%s) -> RunCommand: retrying %r in dir %s' %
               (GetCallerName(), cmd, cwd))

  return output
