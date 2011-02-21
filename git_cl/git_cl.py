#!/usr/bin/python
# git-cl -- a git-command for integrating reviews on Rietveld
# Copyright (C) 2008 Evan Martin <martine@danga.com>

import errno
import logging
import optparse
import os
import re
import subprocess
import sys
import tempfile
import textwrap
import upload
import urlparse
import urllib2

try:
  import readline
except ImportError:
  pass

try:
  # Add the parent directory in case it's a depot_tools checkout.
  depot_tools_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
  sys.path.append(depot_tools_path)
  import breakpad
except ImportError:
  pass

DEFAULT_SERVER = 'http://codereview.appspot.com'
PREDCOMMIT_HOOK = '.git/hooks/pre-cl-dcommit'
POSTUPSTREAM_HOOK_PATTERN = '.git/hooks/post-cl-%s'
PREUPLOAD_HOOK = '.git/hooks/pre-cl-upload'
DESCRIPTION_BACKUP_FILE = '~/.git_cl_description_backup'

def DieWithError(message):
  print >>sys.stderr, message
  sys.exit(1)


def Popen(cmd, **kwargs):
  """Wrapper for subprocess.Popen() that logs and watch for cygwin issues"""
  logging.info('Popen: ' + ' '.join(cmd))
  try:
    return subprocess.Popen(cmd, **kwargs)
  except OSError, e:
    if e.errno == errno.EAGAIN and sys.platform == 'cygwin':
      DieWithError(
          'Visit '
          'http://code.google.com/p/chromium/wiki/CygwinDllRemappingFailure to '
          'learn how to fix this error; you need to rebase your cygwin dlls')
    raise


def RunCommand(cmd, error_ok=False, error_message=None,
               redirect_stdout=True, swallow_stderr=False):
  if redirect_stdout:
    stdout = subprocess.PIPE
  else:
    stdout = None
  if swallow_stderr:
    stderr = subprocess.PIPE
  else:
    stderr = None
  proc = Popen(cmd, stdout=stdout, stderr=stderr)
  output = proc.communicate()[0]
  if not error_ok and proc.returncode != 0:
    DieWithError('Command "%s" failed.\n' % (' '.join(cmd)) +
                 (error_message or output or ''))
  return output


def RunGit(args, **kwargs):
  cmd = ['git'] + args
  return RunCommand(cmd, **kwargs)


def RunGitWithCode(args):
  proc = Popen(['git'] + args, stdout=subprocess.PIPE)
  output = proc.communicate()[0]
  return proc.returncode, output


def usage(more):
  def hook(fn):
    fn.usage_more = more
    return fn
  return hook


def FixUrl(server):
  """Fix a server url to defaults protocol to http:// if none is specified."""
  if not server:
    return server
  if not re.match(r'[a-z]+\://.*', server):
    return 'http://' + server
  return server


class Settings(object):
  def __init__(self):
    self.default_server = None
    self.cc = None
    self.root = None
    self.is_git_svn = None
    self.svn_branch = None
    self.tree_status_url = None
    self.viewvc_url = None
    self.updated = False

  def LazyUpdateIfNeeded(self):
    """Updates the settings from a codereview.settings file, if available."""
    if not self.updated:
      cr_settings_file = FindCodereviewSettingsFile()
      if cr_settings_file:
        LoadCodereviewSettingsFromFile(cr_settings_file)
      self.updated = True

  def GetDefaultServerUrl(self, error_ok=False):
    if not self.default_server:
      self.LazyUpdateIfNeeded()
      self.default_server = FixUrl(self._GetConfig('rietveld.server',
                                                   error_ok=True))
      if error_ok:
        return self.default_server
      if not self.default_server:
        error_message = ('Could not find settings file. You must configure '
                         'your review setup by running "git cl config".')
        self.default_server = FixUrl(self._GetConfig(
            'rietveld.server', error_message=error_message))
    return self.default_server

  def GetCCList(self):
    """Return the users cc'd on this CL.

    Return is a string suitable for passing to gcl with the --cc flag.
    """
    if self.cc is None:
      self.cc = self._GetConfig('rietveld.cc', error_ok=True)
      more_cc = self._GetConfig('rietveld.extracc', error_ok=True)
      if more_cc is not None:
        self.cc += ',' + more_cc
    return self.cc

  def GetRoot(self):
    if not self.root:
      self.root = os.path.abspath(RunGit(['rev-parse', '--show-cdup']).strip())
    return self.root

  def GetIsGitSvn(self):
    """Return true if this repo looks like it's using git-svn."""
    if self.is_git_svn is None:
      # If you have any "svn-remote.*" config keys, we think you're using svn.
      self.is_git_svn = RunGitWithCode(
          ['config', '--get-regexp', r'^svn-remote\.'])[0] == 0
    return self.is_git_svn

  def GetSVNBranch(self):
    if self.svn_branch is None:
      if not self.GetIsGitSvn():
        DieWithError('Repo doesn\'t appear to be a git-svn repo.')

      # Try to figure out which remote branch we're based on.
      # Strategy:
      # 1) find all git-svn branches and note their svn URLs.
      # 2) iterate through our branch history and match up the URLs.

      # regexp matching the git-svn line that contains the URL.
      git_svn_re = re.compile(r'^\s*git-svn-id: (\S+)@', re.MULTILINE)

      # Get the refname and svn url for all refs/remotes/*.
      remotes = RunGit(['for-each-ref', '--format=%(refname)',
                        'refs/remotes']).splitlines()
      svn_refs = {}
      for ref in remotes:
        match = git_svn_re.search(RunGit(['cat-file', '-p', ref]))
        # Prefer origin/HEAD over all others.
        if match and (match.group(1) not in svn_refs or
                      ref == "refs/remotes/origin/HEAD"):
          svn_refs[match.group(1)] = ref

      if len(svn_refs) == 1:
        # Only one svn branch exists -- seems like a good candidate.
        self.svn_branch = svn_refs.values()[0]
      elif len(svn_refs) > 1:
        # We have more than one remote branch available.  We don't
        # want to go through all of history, so read a line from the
        # pipe at a time.
        # The -100 is an arbitrary limit so we don't search forever.
        cmd = ['git', 'log', '-100', '--pretty=medium']
        proc = Popen(cmd, stdout=subprocess.PIPE)
        for line in proc.stdout:
          match = git_svn_re.match(line)
          if match:
            url = match.group(1)
            if url in svn_refs:
              self.svn_branch = svn_refs[url]
              proc.stdout.close()  # Cut pipe.
              break

      if not self.svn_branch:
        DieWithError('Can\'t guess svn branch -- try specifying it on the '
            'command line')

    return self.svn_branch

  def GetTreeStatusUrl(self, error_ok=False):
    if not self.tree_status_url:
      error_message = ('You must configure your tree status URL by running '
                       '"git cl config".')
      self.tree_status_url = self._GetConfig('rietveld.tree-status-url',
                                             error_ok=error_ok,
                                             error_message=error_message)
    return self.tree_status_url

  def GetViewVCUrl(self):
    if not self.viewvc_url:
      self.viewvc_url = self._GetConfig('rietveld.viewvc-url', error_ok=True)
    return self.viewvc_url

  def _GetConfig(self, param, **kwargs):
    self.LazyUpdateIfNeeded()
    return RunGit(['config', param], **kwargs).strip()


settings = Settings()


did_migrate_check = False
def CheckForMigration():
  """Migrate from the old issue format, if found.

  We used to store the branch<->issue mapping in a file in .git, but it's
  better to store it in the .git/config, since deleting a branch deletes that
  branch's entry there.
  """

  # Don't run more than once.
  global did_migrate_check
  if did_migrate_check:
    return

  gitdir = RunGit(['rev-parse', '--git-dir']).strip()
  storepath = os.path.join(gitdir, 'cl-mapping')
  if os.path.exists(storepath):
    print "old-style git-cl mapping file (%s) found; migrating." % storepath
    store = open(storepath, 'r')
    for line in store:
      branch, issue = line.strip().split()
      RunGit(['config', 'branch.%s.rietveldissue' % ShortBranchName(branch),
              issue])
    store.close()
    os.remove(storepath)
  did_migrate_check = True


def ShortBranchName(branch):
  """Convert a name like 'refs/heads/foo' to just 'foo'."""
  return branch.replace('refs/heads/', '')


class Changelist(object):
  def __init__(self, branchref=None):
    # Poke settings so we get the "configure your server" message if necessary.
    settings.GetDefaultServerUrl()
    self.branchref = branchref
    if self.branchref:
      self.branch = ShortBranchName(self.branchref)
    else:
      self.branch = None
    self.rietveld_server = None
    self.upstream_branch = None
    self.has_issue = False
    self.issue = None
    self.has_description = False
    self.description = None
    self.has_patchset = False
    self.patchset = None

  def GetBranch(self):
    """Returns the short branch name, e.g. 'master'."""
    if not self.branch:
      self.branchref = RunGit(['symbolic-ref', 'HEAD']).strip()
      self.branch = ShortBranchName(self.branchref)
    return self.branch

  def GetBranchRef(self):
    """Returns the full branch name, e.g. 'refs/heads/master'."""
    self.GetBranch()  # Poke the lazy loader.
    return self.branchref

  def FetchUpstreamTuple(self):
    """Returns a tuple containg remote and remote ref,
       e.g. 'origin', 'refs/heads/master'
    """
    remote = '.'
    branch = self.GetBranch()
    upstream_branch = RunGit(['config', 'branch.%s.merge' % branch],
                             error_ok=True).strip()
    if upstream_branch:
      remote = RunGit(['config', 'branch.%s.remote' % branch]).strip()
    else:
      # Fall back on trying a git-svn upstream branch.
      if settings.GetIsGitSvn():
        upstream_branch = settings.GetSVNBranch()
      else:
        # Else, try to guess the origin remote.
        remote_branches = RunGit(['branch', '-r']).split()
        if 'origin/master' in remote_branches:
          # Fall back on origin/master if it exits.
          remote = 'origin'
          upstream_branch = 'refs/heads/master'
        elif 'origin/trunk' in remote_branches:
          # Fall back on origin/trunk if it exists. Generally a shared
          # git-svn clone
          remote = 'origin'
          upstream_branch = 'refs/heads/trunk'
        else:
          DieWithError("""Unable to determine default branch to diff against.
Either pass complete "git diff"-style arguments, like
  git cl upload origin/master
or verify this branch is set up to track another (via the --track argument to
"git checkout -b ...").""")

    return remote, upstream_branch

  def GetUpstreamBranch(self):
    if self.upstream_branch is None:
      remote, upstream_branch = self.FetchUpstreamTuple()
      if remote is not '.':
        upstream_branch = upstream_branch.replace('heads', 'remotes/' + remote)
      self.upstream_branch = upstream_branch
    return self.upstream_branch

  def GetRemoteUrl(self):
    """Return the configured remote URL, e.g. 'git://example.org/foo.git/'.

    Returns None if there is no remote.
    """
    remote = self.FetchUpstreamTuple()[0]
    if remote == '.':
      return None
    return RunGit(['config', 'remote.%s.url' % remote], error_ok=True).strip()

  def GetIssue(self):
    if not self.has_issue:
      CheckForMigration()
      issue = RunGit(['config', self._IssueSetting()], error_ok=True).strip()
      if issue:
        self.issue = issue
        self.rietveld_server = FixUrl(RunGit(
            ['config', self._RietveldServer()], error_ok=True).strip())
      else:
        self.issue = None
      if not self.rietveld_server:
        self.rietveld_server = settings.GetDefaultServerUrl()
      self.has_issue = True
    return self.issue

  def GetRietveldServer(self):
    self.GetIssue()
    return self.rietveld_server

  def GetIssueURL(self):
    """Get the URL for a particular issue."""
    return '%s/%s' % (self.GetRietveldServer(), self.GetIssue())

  def GetDescription(self, pretty=False):
    if not self.has_description:
      if self.GetIssue():
        path = '/' + self.GetIssue() + '/description'
        rpc_server = self._RpcServer()
        self.description = rpc_server.Send(path).strip()
      self.has_description = True
    if pretty:
      wrapper = textwrap.TextWrapper()
      wrapper.initial_indent = wrapper.subsequent_indent = '  '
      return wrapper.fill(self.description)
    return self.description

  def GetPatchset(self):
    if not self.has_patchset:
      patchset = RunGit(['config', self._PatchsetSetting()],
                        error_ok=True).strip()
      if patchset:
        self.patchset = patchset
      else:
        self.patchset = None
      self.has_patchset = True
    return self.patchset

  def SetPatchset(self, patchset):
    """Set this branch's patchset.  If patchset=0, clears the patchset."""
    if patchset:
      RunGit(['config', self._PatchsetSetting(), str(patchset)])
    else:
      RunGit(['config', '--unset', self._PatchsetSetting()],
             swallow_stderr=True, error_ok=True)
    self.has_patchset = False

  def SetIssue(self, issue):
    """Set this branch's issue.  If issue=0, clears the issue."""
    if issue:
      RunGit(['config', self._IssueSetting(), str(issue)])
      if self.rietveld_server:
        RunGit(['config', self._RietveldServer(), self.rietveld_server])
    else:
      RunGit(['config', '--unset', self._IssueSetting()])
      self.SetPatchset(0)
    self.has_issue = False

  def CloseIssue(self):
    rpc_server = self._RpcServer()
    # Newer versions of Rietveld require us to pass an XSRF token to POST, so
    # we fetch it from the server.  (The version used by Chromium has been
    # modified so the token isn't required when closing an issue.)
    xsrf_token = rpc_server.Send('/xsrf_token',
                                 extra_headers={'X-Requesting-XSRF-Token': '1'})

    # You cannot close an issue with a GET.
    # We pass an empty string for the data so it is a POST rather than a GET.
    data = [("description", self.description),
            ("xsrf_token", xsrf_token)]
    ctype, body = upload.EncodeMultipartFormData(data, [])
    rpc_server.Send('/' + self.GetIssue() + '/close', body, ctype)

  def _RpcServer(self):
    """Returns an upload.RpcServer() to access this review's rietveld instance.
    """
    server = self.GetRietveldServer()
    return upload.GetRpcServer(server, save_cookies=True)

  def _IssueSetting(self):
    """Return the git setting that stores this change's issue."""
    return 'branch.%s.rietveldissue' % self.GetBranch()

  def _PatchsetSetting(self):
    """Return the git setting that stores this change's most recent patchset."""
    return 'branch.%s.rietveldpatchset' % self.GetBranch()

  def _RietveldServer(self):
    """Returns the git setting that stores this change's rietveld server."""
    return 'branch.%s.rietveldserver' % self.GetBranch()


def GetCodereviewSettingsInteractively():
  """Prompt the user for settings."""
  server = settings.GetDefaultServerUrl(error_ok=True)
  prompt = 'Rietveld server (host[:port])'
  prompt += ' [%s]' % (server or DEFAULT_SERVER)
  newserver = raw_input(prompt + ': ')
  if not server and not newserver:
    newserver = DEFAULT_SERVER
  if newserver and newserver != server:
    RunGit(['config', 'rietveld.server', newserver])

  def SetProperty(initial, caption, name):
    prompt = caption
    if initial:
      prompt += ' ("x" to clear) [%s]' % initial
    new_val = raw_input(prompt + ': ')
    if new_val == 'x':
      RunGit(['config', '--unset-all', 'rietveld.' + name], error_ok=True)
    elif new_val and new_val != initial:
      RunGit(['config', 'rietveld.' + name, new_val])

  SetProperty(settings.GetCCList(), 'CC list', 'cc')
  SetProperty(settings.GetTreeStatusUrl(error_ok=True), 'Tree status URL',
              'tree-status-url')
  SetProperty(settings.GetViewVCUrl(), 'ViewVC URL', 'viewvc-url')

  # TODO: configure a default branch to diff against, rather than this
  # svn-based hackery.


def FindCodereviewSettingsFile(filename='codereview.settings'):
  """Finds the given file starting in the cwd and going up.

  Only looks up to the top of the repository unless an
  'inherit-review-settings-ok' file exists in the root of the repository.
  """
  inherit_ok_file = 'inherit-review-settings-ok'
  cwd = os.getcwd()
  root = os.path.abspath(RunGit(['rev-parse', '--show-cdup']).strip())
  if os.path.isfile(os.path.join(root, inherit_ok_file)):
    root = '/'
  while True:
    if filename in os.listdir(cwd):
      if os.path.isfile(os.path.join(cwd, filename)):
        return open(os.path.join(cwd, filename))
    if cwd == root:
      break
    cwd = os.path.dirname(cwd)


def LoadCodereviewSettingsFromFile(fileobj):
  """Parse a codereview.settings file and updates hooks."""
  def DownloadToFile(url, filename):
    filename = os.path.join(settings.GetRoot(), filename)
    contents = urllib2.urlopen(url).read()
    fileobj = open(filename, 'w')
    fileobj.write(contents)
    fileobj.close()
    os.chmod(filename, 0755)
    return 0

  keyvals = {}
  for line in fileobj.read().splitlines():
    if not line or line.startswith("#"):
      continue
    k, v = line.split(": ", 1)
    keyvals[k] = v

  def GetProperty(name):
    return keyvals.get(name)

  def SetProperty(name, setting, unset_error_ok=False):
    fullname = 'rietveld.' + name
    if setting in keyvals:
      RunGit(['config', fullname, keyvals[setting]])
    else:
      RunGit(['config', '--unset-all', fullname], error_ok=unset_error_ok)

  SetProperty('server', 'CODE_REVIEW_SERVER')
  # Only server setting is required. Other settings can be absent.
  # In that case, we ignore errors raised during option deletion attempt.
  SetProperty('cc', 'CC_LIST', unset_error_ok=True)
  SetProperty('tree-status-url', 'STATUS', unset_error_ok=True)
  SetProperty('viewvc-url', 'VIEW_VC', unset_error_ok=True)

  if 'PUSH_URL_CONFIG' in keyvals and 'ORIGIN_URL_CONFIG' in keyvals:
    #should be of the form
    #PUSH_URL_CONFIG: url.ssh://gitrw.chromium.org.pushinsteadof
    #ORIGIN_URL_CONFIG: http://src.chromium.org/git
    RunGit(['config', keyvals['PUSH_URL_CONFIG'],
            keyvals['ORIGIN_URL_CONFIG']])

  # Update the hooks if the local hook files aren't present already.
  if GetProperty('GITCL_PREUPLOAD') and not os.path.isfile(PREUPLOAD_HOOK):
    DownloadToFile(GetProperty('GITCL_PREUPLOAD'), PREUPLOAD_HOOK)
  if GetProperty('GITCL_PREDCOMMIT') and not os.path.isfile(PREDCOMMIT_HOOK):
    DownloadToFile(GetProperty('GITCL_PREDCOMMIT'), PREDCOMMIT_HOOK)
  return 0


@usage('[repo root containing codereview.settings]')
def CMDconfig(parser, args):
  """edit configuration for this tree"""

  (options, args) = parser.parse_args(args)
  if len(args) == 0:
    GetCodereviewSettingsInteractively()
    return 0

  url = args[0]
  if not url.endswith('codereview.settings'):
    url = os.path.join(url, 'codereview.settings')

  # Load code review settings and download hooks (if available).
  LoadCodereviewSettingsFromFile(urllib2.urlopen(url))
  return 0


def CMDstatus(parser, args):
  """show status of changelists"""
  parser.add_option('--field',
                    help='print only specific field (desc|id|patch|url)')
  (options, args) = parser.parse_args(args)

  # TODO: maybe make show_branches a flag if necessary.
  show_branches = not options.field

  if show_branches:
    branches = RunGit(['for-each-ref', '--format=%(refname)', 'refs/heads'])
    if branches:
      print 'Branches associated with reviews:'
      for branch in sorted(branches.splitlines()):
        cl = Changelist(branchref=branch)
        print "  %10s: %s" % (cl.GetBranch(), cl.GetIssue())

  cl = Changelist()
  if options.field:
    if options.field.startswith('desc'):
      print cl.GetDescription()
    elif options.field == 'id':
      issueid = cl.GetIssue()
      if issueid:
        print issueid
    elif options.field == 'patch':
      patchset = cl.GetPatchset()
      if patchset:
        print patchset
    elif options.field == 'url':
      url = cl.GetIssueURL()
      if url:
        print url
  else:
    print
    print 'Current branch:',
    if not cl.GetIssue():
      print 'no issue assigned.'
      return 0
    print cl.GetBranch()
    print 'Issue number:', cl.GetIssue(), '(%s)' % cl.GetIssueURL()
    print 'Issue description:'
    print cl.GetDescription(pretty=True)
  return 0


@usage('[issue_number]')
def CMDissue(parser, args):
  """Set or display the current code review issue number.

  Pass issue number 0 to clear the current issue.
"""
  (options, args) = parser.parse_args(args)

  cl = Changelist()
  if len(args) > 0:
    try:
      issue = int(args[0])
    except ValueError:
      DieWithError('Pass a number to set the issue or none to list it.\n'
          'Maybe you want to run git cl status?')
    cl.SetIssue(issue)
  print 'Issue number:', cl.GetIssue(), '(%s)' % cl.GetIssueURL()
  return 0


def CreateDescriptionFromLog(args):
  """Pulls out the commit log to use as a base for the CL description."""
  log_args = []
  if len(args) == 1 and not args[0].endswith('.'):
    log_args = [args[0] + '..']
  elif len(args) == 1 and args[0].endswith('...'):
    log_args = [args[0][:-1]]
  elif len(args) == 2:
    log_args = [args[0] + '..' + args[1]]
  else:
    log_args = args[:]  # Hope for the best!
  return RunGit(['log', '--pretty=format:%s\n\n%b'] + log_args)


def UserEditedLog(starting_text):
  """Given some starting text, let the user edit it and return the result."""
  editor = os.getenv('EDITOR', 'vi')

  (file_handle, filename) = tempfile.mkstemp()
  fileobj = os.fdopen(file_handle, 'w')
  fileobj.write(starting_text)
  fileobj.close()

  ret = subprocess.call(editor + ' ' + filename, shell=True)
  if ret != 0:
    os.remove(filename)
    return

  fileobj = open(filename)
  text = fileobj.read()
  fileobj.close()
  os.remove(filename)
  stripcomment_re = re.compile(r'^#.*$', re.MULTILINE)
  return stripcomment_re.sub('', text).strip()


def RunHook(hook, upstream_branch, error_ok=False):
  """Run a given hook if it exists. By default, we fail on errors."""
  hook = '%s/%s' % (settings.GetRoot(), hook)
  if not os.path.exists(hook):
    return
  return RunCommand([hook, upstream_branch], error_ok=error_ok,
                    redirect_stdout=False)


def CMDpresubmit(parser, args):
  """run presubmit tests on the current changelist"""
  parser.add_option('--upload', action='store_true',
                    help='Run upload hook instead of the push/dcommit hook')
  (options, args) = parser.parse_args(args)

  # Make sure index is up-to-date before running diff-index.
  RunGit(['update-index', '--refresh', '-q'], error_ok=True)
  if RunGit(['diff-index', 'HEAD']):
    # TODO(maruel): Is this really necessary?
    print 'Cannot presubmit with a dirty tree.  You must commit locally first.'
    return 1

  if args:
    base_branch = args[0]
  else:
    # Default to diffing against the "upstream" branch.
    base_branch = Changelist().GetUpstreamBranch()

  if options.upload:
    print '*** Presubmit checks for UPLOAD would report: ***'
    return not RunHook(PREUPLOAD_HOOK, upstream_branch=base_branch,
        error_ok=True)
  else:
    print '*** Presubmit checks for DCOMMIT would report: ***'
    return not RunHook(PREDCOMMIT_HOOK, upstream_branch=base_branch,
        error_ok=True)


@usage('[args to "git diff"]')
def CMDupload(parser, args):
  """upload the current changelist to codereview"""
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('-m', dest='message', help='message for patch')
  parser.add_option('-r', '--reviewers',
                    help='reviewer email addresses')
  parser.add_option('--send-mail', action='store_true',
                    help='send email to reviewer immediately')
  parser.add_option("--emulate_svn_auto_props", action="store_true",
                    dest="emulate_svn_auto_props",
                    help="Emulate Subversion's auto properties feature.")
  parser.add_option("--desc_from_logs", action="store_true",
                    dest="from_logs",
                    help="""Squashes git commit logs into change description and
                            uses message as subject""")
  (options, args) = parser.parse_args(args)

  # Make sure index is up-to-date before running diff-index.
  RunGit(['update-index', '--refresh', '-q'], error_ok=True)
  if RunGit(['diff-index', 'HEAD']):
    print 'Cannot upload with a dirty tree.  You must commit locally first.'
    return 1

  cl = Changelist()
  if args:
    base_branch = args[0]
  else:
    # Default to diffing against the "upstream" branch.
    base_branch = cl.GetUpstreamBranch()
    args = [base_branch + "..."]

  if not options.bypass_hooks:
    RunHook(PREUPLOAD_HOOK, upstream_branch=base_branch, error_ok=False)

  # --no-ext-diff is broken in some versions of Git, so try to work around
  # this by overriding the environment (but there is still a problem if the
  # git config key "diff.external" is used).
  env = os.environ.copy()
  if 'GIT_EXTERNAL_DIFF' in env:
    del env['GIT_EXTERNAL_DIFF']
  subprocess.call(['git', 'diff', '--no-ext-diff', '--stat', '-M'] + args,
                  env=env)

  upload_args = ['--assume_yes']  # Don't ask about untracked files.
  upload_args.extend(['--server', cl.GetRietveldServer()])
  if options.reviewers:
    upload_args.extend(['--reviewers', options.reviewers])
  if options.emulate_svn_auto_props:
    upload_args.append('--emulate_svn_auto_props')
  if options.send_mail:
    if not options.reviewers:
      DieWithError("Must specify reviewers to send email.")
    upload_args.append('--send_mail')
  if options.from_logs and not options.message:
    print 'Must set message for subject line if using desc_from_logs'
    return 1

  change_desc = None

  if cl.GetIssue():
    if options.message:
      upload_args.extend(['--message', options.message])
    upload_args.extend(['--issue', cl.GetIssue()])
    print ("This branch is associated with issue %s. "
           "Adding patch to that issue." % cl.GetIssue())
  else:
    log_desc = CreateDescriptionFromLog(args)
    if options.from_logs:
      # Uses logs as description and message as subject.
      subject = options.message
      change_desc = subject + '\n\n' + log_desc
    else:
      initial_text = """# Enter a description of the change.
# This will displayed on the codereview site.
# The first line will also be used as the subject of the review.
"""
      if 'BUG=' not in log_desc:
        log_desc += '\nBUG='
      if 'TEST=' not in log_desc:
        log_desc += '\nTEST='
      change_desc = UserEditedLog(initial_text + log_desc)
      subject = ''
      if change_desc:
        subject = change_desc.splitlines()[0]
    if not change_desc:
      print "Description is empty; aborting."
      return 1
    upload_args.extend(['--message', subject])
    upload_args.extend(['--description', change_desc])
    upload_args.extend(['--cc', settings.GetCCList()])

  # Include the upstream repo's URL in the change -- this is useful for
  # projects that have their source spread across multiple repos.
  remote_url = None
  if settings.GetIsGitSvn():
    data = RunGit(['svn', 'info'])
    if data:
      keys = dict(line.split(': ', 1) for line in data.splitlines()
                  if ': ' in line)
      remote_url = keys.get('URL', None)
  else:
    if cl.GetRemoteUrl() and '/' in cl.GetUpstreamBranch():
      remote_url = (cl.GetRemoteUrl() + '@'
                    + cl.GetUpstreamBranch().split('/')[-1])
  if remote_url:
    upload_args.extend(['--base_url', remote_url])

  try:
    issue, patchset = upload.RealMain(['upload'] + upload_args + args)
  except:
    # If we got an exception after the user typed a description for their
    # change, back up the description before re-raising.
    if change_desc:
      backup_path = os.path.expanduser(DESCRIPTION_BACKUP_FILE)
      print '\nGot exception while uploading -- saving description to %s\n' \
          % backup_path
      backup_file = open(backup_path, 'w')
      backup_file.write(change_desc)
      backup_file.close()
    raise

  if not cl.GetIssue():
    cl.SetIssue(issue)
  cl.SetPatchset(patchset)
  return 0


def SendUpstream(parser, args, cmd):
  """Common code for CmdPush and CmdDCommit

  Squashed commit into a single.
  Updates changelog with metadata (e.g. pointer to review).
  Pushes/dcommits the code upstream.
  Updates review and closes.
  """
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('-m', dest='message',
                    help="override review description")
  parser.add_option('-f', action='store_true', dest='force',
                    help="force yes to questions (don't prompt)")
  parser.add_option('-c', dest='contributor',
                    help="external contributor for patch (appended to " +
                         "description and used as author for git). Should be " +
                         "formatted as 'First Last <email@example.com>'")
  parser.add_option('--tbr', action='store_true', dest='tbr',
                    help="short for 'to be reviewed', commit branch " +
                         "even without uploading for review")
  (options, args) = parser.parse_args(args)
  cl = Changelist()

  if not args or cmd == 'push':
    # Default to merging against our best guess of the upstream branch.
    args = [cl.GetUpstreamBranch()]

  base_branch = args[0]

  # Make sure index is up-to-date before running diff-index.
  RunGit(['update-index', '--refresh', '-q'], error_ok=True)
  if RunGit(['diff-index', 'HEAD']):
    print 'Cannot %s with a dirty tree.  You must commit locally first.' % cmd
    return 1

  # This rev-list syntax means "show all commits not in my branch that
  # are in base_branch".
  upstream_commits = RunGit(['rev-list', '^' + cl.GetBranchRef(),
                             base_branch]).splitlines()
  if upstream_commits:
    print ('Base branch "%s" has %d commits '
           'not in this branch.' % (base_branch, len(upstream_commits)))
    print 'Run "git merge %s" before attempting to %s.' % (base_branch, cmd)
    return 1

  if cmd == 'dcommit':
    # This is the revision `svn dcommit` will commit on top of.
    svn_head = RunGit(['log', '--grep=^git-svn-id:', '-1',
                       '--pretty=format:%H'])
    extra_commits = RunGit(['rev-list', '^' + svn_head, base_branch])
    if extra_commits:
      print ('This branch has %d additional commits not upstreamed yet.'
             % len(extra_commits.splitlines()))
      print ('Upstream "%s" or rebase this branch on top of the upstream trunk '
             'before attempting to %s.' % (base_branch, cmd))
      return 1

  if not options.force and not options.bypass_hooks:
    RunHook(PREDCOMMIT_HOOK, upstream_branch=base_branch, error_ok=False)

    if cmd == 'dcommit':
      # Check the tree status if the tree status URL is set.
      status = GetTreeStatus()
      if 'closed' == status:
        print ('The tree is closed.  Please wait for it to reopen. Use '
               '"git cl dcommit -f" to commit on a closed tree.')
        return 1
      elif 'unknown' == status:
        print ('Unable to determine tree status.  Please verify manually and '
               'use "git cl dcommit -f" to commit on a closed tree.')

  description = options.message
  if not options.tbr:
    # It is important to have these checks early.  Not only for user
    # convenience, but also because the cl object then caches the correct values
    # of these fields even as we're juggling branches for setting up the commit.
    if not cl.GetIssue():
      print 'Current issue unknown -- has this branch been uploaded?'
      print 'Use --tbr to commit without review.'
      return 1

    if not description:
      description = cl.GetDescription()

    if not description:
      print 'No description set.'
      print 'Visit %s/edit to set it.' % (cl.GetIssueURL())
      return 1

    description += "\n\nReview URL: %s" % cl.GetIssueURL()
  else:
    if not description:
      # Submitting TBR.  See if there's already a description in Rietveld, else
      # create a template description. Eitherway, give the user a chance to edit
      # it to fill in the TBR= field.
      if cl.GetIssue():
        description = cl.GetDescription()

      if not description:
        description = """# Enter a description of the change.
# This will be used as the change log for the commit.

"""
        description += CreateDescriptionFromLog(args)

      description = UserEditedLog(description + '\nTBR=')

    if not description:
      print "Description empty; aborting."
      return 1

  if options.contributor:
    if not re.match('^.*\s<\S+@\S+>$', options.contributor):
      print "Please provide contibutor as 'First Last <email@example.com>'"
      return 1
    description += "\nPatch from %s." % options.contributor
  print 'Description:', repr(description)

  branches = [base_branch, cl.GetBranchRef()]
  if not options.force:
    subprocess.call(['git', 'diff', '--stat'] + branches)
    raw_input("About to commit; enter to confirm.")

  # We want to squash all this branch's commits into one commit with the
  # proper description.
  # We do this by doing a "merge --squash" into a new commit branch, then
  # dcommitting that.
  MERGE_BRANCH = 'git-cl-commit'
  # Delete the merge branch if it already exists.
  if RunGitWithCode(['show-ref', '--quiet', '--verify',
                     'refs/heads/' + MERGE_BRANCH])[0] == 0:
    RunGit(['branch', '-D', MERGE_BRANCH])

  # We might be in a directory that's present in this branch but not in the
  # trunk.  Move up to the top of the tree so that git commands that expect a
  # valid CWD won't fail after we check out the merge branch.
  rel_base_path = RunGit(['rev-parse', '--show-cdup']).strip()
  if rel_base_path:
    os.chdir(rel_base_path)

  # Stuff our change into the merge branch.
  # We wrap in a try...finally block so if anything goes wrong,
  # we clean up the branches.
  retcode = -1
  try:
    RunGit(['checkout', '-q', '-b', MERGE_BRANCH, base_branch])
    RunGit(['merge', '--squash', cl.GetBranchRef()])
    if options.contributor:
      RunGit(['commit', '--author', options.contributor, '-m', description])
    else:
      RunGit(['commit', '-m', description])
    if cmd == 'push':
      # push the merge branch.
      remote, branch = cl.FetchUpstreamTuple()
      retcode, output = RunGitWithCode(
          ['push', '--porcelain', remote, 'HEAD:%s' % branch])
      logging.debug(output)
    else:
      # dcommit the merge branch.
      retcode, output = RunGitWithCode(['svn', 'dcommit', '--no-rebase'])
  finally:
    # And then swap back to the original branch and clean up.
    RunGit(['checkout', '-q', cl.GetBranch()])
    RunGit(['branch', '-D', MERGE_BRANCH])

  if cl.GetIssue():
    if cmd == 'dcommit' and 'Committed r' in output:
      revision = re.match('.*?\nCommitted r(\\d+)', output, re.DOTALL).group(1)
    elif cmd == 'push' and retcode == 0:
      match = (re.match(r'.*?([a-f0-9]{7})\.\.([a-f0-9]{7})$', l)
               for l in output.splitlines(False))
      match = filter(None, match)
      if len(match) != 1:
        DieWithError("Couldn't parse ouput to extract the committed hash:\n%s" %
            output)
      revision = match[0].group(2)
    else:
      return 1
    viewvc_url = settings.GetViewVCUrl()
    if viewvc_url and revision:
      cl.description += ('\n\nCommitted: ' + viewvc_url + revision)
    print ('Closing issue '
           '(you may be prompted for your codereview password)...')
    cl.CloseIssue()
    cl.SetIssue(0)

  if retcode == 0:
    hook = POSTUPSTREAM_HOOK_PATTERN % cmd
    if os.path.isfile(hook):
      RunHook(hook, upstream_branch=base_branch, error_ok=True)

  return 0


@usage('[upstream branch to apply against]')
def CMDdcommit(parser, args):
  """commit the current changelist via git-svn"""
  if not settings.GetIsGitSvn():
    message = """This doesn't appear to be an SVN repository.
If your project has a git mirror with an upstream SVN master, you probably need
to run 'git svn init', see your project's git mirror documentation.
If your project has a true writeable upstream repository, you probably want
to run 'git cl push' instead.
Choose wisely, if you get this wrong, your commit might appear to succeed but
will instead be silently ignored."""
    print(message)
    raw_input('[Press enter to dcommit or ctrl-C to quit]')
  return SendUpstream(parser, args, 'dcommit')


@usage('[upstream branch to apply against]')
def CMDpush(parser, args):
  """commit the current changelist via git"""
  if settings.GetIsGitSvn():
    print('This appears to be an SVN repository.')
    print('Are you sure you didn\'t mean \'git cl dcommit\'?')
    raw_input('[Press enter to push or ctrl-C to quit]')
  return SendUpstream(parser, args, 'push')


@usage('<patch url or issue id>')
def CMDpatch(parser, args):
  """patch in a code review"""
  parser.add_option('-b', dest='newbranch',
                    help='create a new branch off trunk for the patch')
  parser.add_option('-f', action='store_true', dest='force',
                    help='with -b, clobber any existing branch')
  parser.add_option('--reject', action='store_true', dest='reject',
                    help='allow failed patches and spew .rej files')
  parser.add_option('-n', '--no-commit', action='store_true', dest='nocommit',
                    help="don't commit after patch applies")
  (options, args) = parser.parse_args(args)
  if len(args) != 1:
    parser.print_help()
    return 1
  input = args[0]

  if re.match(r'\d+', input):
    # Input is an issue id.  Figure out the URL.
    issue = input
    server = settings.GetDefaultServerUrl()
    fetch = urllib2.urlopen('%s/%s' % (server, issue)).read()
    m = re.search(r'/download/issue[0-9]+_[0-9]+.diff', fetch)
    if not m:
      DieWithError('Must pass an issue ID or full URL for '
          '\'Download raw patch set\'')
    url = '%s%s' % (server, m.group(0).strip())
  else:
    # Assume it's a URL to the patch. Default to http.
    input = FixUrl(input)
    match = re.match(r'.*?/issue(\d+)_\d+.diff', input)
    if match:
      issue = match.group(1)
      url = input
    else:
      DieWithError('Must pass an issue ID or full URL for '
          '\'Download raw patch set\'')

  if options.newbranch:
    if options.force:
      RunGit(['branch', '-D', options.newbranch],
         swallow_stderr=True, error_ok=True)
    RunGit(['checkout', '-b', options.newbranch,
            Changelist().GetUpstreamBranch()])

  # Switch up to the top-level directory, if necessary, in preparation for
  # applying the patch.
  top = RunGit(['rev-parse', '--show-cdup']).strip()
  if top:
    os.chdir(top)

  patch_data = urllib2.urlopen(url).read()
  # Git patches have a/ at the beginning of source paths.  We strip that out
  # with a sed script rather than the -p flag to patch so we can feed either
  # Git or svn-style patches into the same apply command.
  # re.sub() should be used but flags=re.MULTILINE is only in python 2.7.
  sed_proc = Popen(['sed', '-e', 's|^--- a/|--- |; s|^+++ b/|+++ |'],
      stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  patch_data = sed_proc.communicate(patch_data)[0]
  if sed_proc.returncode:
    DieWithError('Git patch mungling failed.')
  logging.info(patch_data)
  # We use "git apply" to apply the patch instead of "patch" so that we can
  # pick up file adds.
  # The --index flag means: also insert into the index (so we catch adds).
  cmd = ['git', 'apply', '--index', '-p0']
  if options.reject:
    cmd.append('--reject')
  patch_proc = Popen(cmd, stdin=subprocess.PIPE)
  patch_proc.communicate(patch_data)
  if patch_proc.returncode:
    DieWithError('Failed to apply the patch')

  # If we had an issue, commit the current state and register the issue.
  if not options.nocommit:
    RunGit(['commit', '-m', 'patch from issue %s' % issue])
    cl = Changelist()
    cl.SetIssue(issue)
    print "Committed patch."
  else:
    print "Patch applied to index."
  return 0


def CMDrebase(parser, args):
  """rebase current branch on top of svn repo"""
  # Provide a wrapper for git svn rebase to help avoid accidental
  # git svn dcommit.
  # It's the only command that doesn't use parser at all since we just defer
  # execution to git-svn.
  RunGit(['svn', 'rebase'] + args, redirect_stdout=False)
  return 0


def GetTreeStatus():
  """Fetches the tree status and returns either 'open', 'closed',
  'unknown' or 'unset'."""
  url = settings.GetTreeStatusUrl(error_ok=True)
  if url:
    status = urllib2.urlopen(url).read().lower()
    if status.find('closed') != -1 or status == '0':
      return 'closed'
    elif status.find('open') != -1 or status == '1':
      return 'open'
    return 'unknown'

  return 'unset'

def GetTreeStatusReason():
  """Fetches the tree status from a json url and returns the message
  with the reason for the tree to be opened or closed."""
  # Don't import it at file level since simplejson is not installed by default
  # on python 2.5 and it is only used for git-cl tree which isn't often used,
  # forcing everyone to install simplejson isn't efficient.
  try:
    import simplejson as json
  except ImportError:
    try:
      import json
      # Some versions of python2.5 have an incomplete json module. Check to make
      # sure loads exists.
      json.loads
    except (ImportError, AttributeError):
      print >> sys.stderr, 'Please install simplejson'
      sys.exit(1)

  url = settings.GetTreeStatusUrl()
  json_url = urlparse.urljoin(url, '/current?format=json')
  connection = urllib2.urlopen(json_url)
  status = json.loads(connection.read())
  connection.close()
  return status['message']

def CMDtree(parser, args):
  """show the status of the tree"""
  (options, args) = parser.parse_args(args)
  status = GetTreeStatus()
  if 'unset' == status:
    print 'You must configure your tree status URL by running "git cl config".'
    return 2

  print "The tree is %s" % status
  print
  print GetTreeStatusReason()
  if status != 'open':
    return 1
  return 0


def CMDupstream(parser, args):
  """print the name of the upstream branch, if any"""
  (options, args) = parser.parse_args(args)
  cl = Changelist()
  print cl.GetUpstreamBranch()
  return 0


def Command(name):
  return getattr(sys.modules[__name__], 'CMD' + name, None)


def CMDhelp(parser, args):
  """print list of commands or help for a specific command"""
  (options, args) = parser.parse_args(args)
  if len(args) == 1:
    return main(args + ['--help'])
  parser.print_help()
  return 0


def GenUsage(parser, command):
  """Modify an OptParse object with the function's documentation."""
  obj = Command(command)
  more = getattr(obj, 'usage_more', '')
  if command == 'help':
    command = '<command>'
  else:
    # OptParser.description prefer nicely non-formatted strings.
    parser.description = re.sub('[\r\n ]{2,}', ' ', obj.__doc__)
  parser.set_usage('usage: %%prog %s [options] %s' % (command, more))


def main(argv):
  """Doesn't parse the arguments here, just find the right subcommand to
  execute."""
  # Do it late so all commands are listed.
  CMDhelp.usage_more = ('\n\nCommands are:\n' + '\n'.join([
      '  %-10s %s' % (fn[3:], Command(fn[3:]).__doc__.split('\n')[0].strip())
      for fn in dir(sys.modules[__name__]) if fn.startswith('CMD')]))

  # Create the option parse and add --verbose support.
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', action='store_true')
  old_parser_args = parser.parse_args
  def Parse(args):
    options, args = old_parser_args(args)
    if options.verbose:
      logging.basicConfig(level=logging.DEBUG)
    else:
      logging.basicConfig(level=logging.WARNING)
    return options, args
  parser.parse_args = Parse

  if argv:
    command = Command(argv[0])
    if command:
      # "fix" the usage and the description now that we know the subcommand.
      GenUsage(parser, argv[0])
      try:
        return command(parser, argv[1:])
      except urllib2.HTTPError, e:
        if e.code != 500:
          raise
        DieWithError(
            ('AppEngine is misbehaving and returned HTTP %d, again. Keep faith '
             'and retry or visit go/isgaeup.\n%s') % (e.code, str(e)))

  # Not a known command. Default to help.
  GenUsage(parser, 'help')
  return CMDhelp(parser, argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
