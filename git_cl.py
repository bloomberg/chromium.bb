#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copyright (C) 2008 Evan Martin <martine@danga.com>

"""A git-command for integrating reviews on Rietveld and Gerrit."""

from __future__ import print_function

from distutils.version import LooseVersion
from multiprocessing.pool import ThreadPool
import base64
import collections
import httplib
import json
import logging
import multiprocessing
import optparse
import os
import re
import stat
import sys
import textwrap
import time
import traceback
import urllib
import urllib2
import urlparse
import uuid
import webbrowser
import zlib

try:
  import readline  # pylint: disable=F0401,W0611
except ImportError:
  pass

from third_party import colorama
from third_party import httplib2
from third_party import upload
import auth
from luci_hacks import trigger_luci_job as luci_trigger
import clang_format
import commit_queue
import dart_format
import setup_color
import fix_encoding
import gclient_utils
import gerrit_util
import git_cache
import git_common
import git_footers
import owners
import owners_finder
import presubmit_support
import rietveld
import scm
import subcommand
import subprocess2
import watchlists

__version__ = '2.0'

COMMIT_BOT_EMAIL = 'commit-bot@chromium.org'
DEFAULT_SERVER = 'https://codereview.appspot.com'
POSTUPSTREAM_HOOK_PATTERN = '.git/hooks/post-cl-%s'
DESCRIPTION_BACKUP_FILE = '~/.git_cl_description_backup'
GIT_INSTRUCTIONS_URL = 'http://code.google.com/p/chromium/wiki/UsingGit'
REFS_THAT_ALIAS_TO_OTHER_REFS = {
    'refs/remotes/origin/lkgr': 'refs/remotes/origin/master',
    'refs/remotes/origin/lkcr': 'refs/remotes/origin/master',
}

# Valid extensions for files we want to lint.
DEFAULT_LINT_REGEX = r"(.*\.cpp|.*\.cc|.*\.h)"
DEFAULT_LINT_IGNORE_REGEX = r"$^"

# Shortcut since it quickly becomes redundant.
Fore = colorama.Fore

# Initialized in main()
settings = None


def DieWithError(message):
  print(message, file=sys.stderr)
  sys.exit(1)


def GetNoGitPagerEnv():
  env = os.environ.copy()
  # 'cat' is a magical git string that disables pagers on all platforms.
  env['GIT_PAGER'] = 'cat'
  return env


def RunCommand(args, error_ok=False, error_message=None, shell=False, **kwargs):
  try:
    return subprocess2.check_output(args, shell=shell, **kwargs)
  except subprocess2.CalledProcessError as e:
    logging.debug('Failed running %s', args)
    if not error_ok:
      DieWithError(
          'Command "%s" failed.\n%s' % (
            ' '.join(args), error_message or e.stdout or ''))
    return e.stdout


def RunGit(args, **kwargs):
  """Returns stdout."""
  return RunCommand(['git'] + args, **kwargs)


def RunGitWithCode(args, suppress_stderr=False):
  """Returns return code and stdout."""
  try:
    if suppress_stderr:
      stderr = subprocess2.VOID
    else:
      stderr = sys.stderr
    out, code = subprocess2.communicate(['git'] + args,
                                        env=GetNoGitPagerEnv(),
                                        stdout=subprocess2.PIPE,
                                        stderr=stderr)
    return code, out[0]
  except ValueError:
    # When the subprocess fails, it returns None.  That triggers a ValueError
    # when trying to unpack the return value into (out, code).
    return 1, ''


def RunGitSilent(args):
  """Returns stdout, suppresses stderr and ignores the return code."""
  return RunGitWithCode(args, suppress_stderr=True)[1]


def IsGitVersionAtLeast(min_version):
  prefix = 'git version '
  version = RunGit(['--version']).strip()
  return (version.startswith(prefix) and
      LooseVersion(version[len(prefix):]) >= LooseVersion(min_version))


def BranchExists(branch):
  """Return True if specified branch exists."""
  code, _ = RunGitWithCode(['rev-parse', '--verify', branch],
                           suppress_stderr=True)
  return not code


def ask_for_data(prompt):
  try:
    return raw_input(prompt)
  except KeyboardInterrupt:
    # Hide the exception.
    sys.exit(1)


def git_set_branch_value(key, value):
  branch = GetCurrentBranch()
  if not branch:
    return

  cmd = ['config']
  if isinstance(value, int):
    cmd.append('--int')
  git_key = 'branch.%s.%s' % (branch, key)
  RunGit(cmd + [git_key, str(value)])


def git_get_branch_default(key, default):
  branch = GetCurrentBranch()
  if branch:
    git_key = 'branch.%s.%s' % (branch, key)
    (_, stdout) = RunGitWithCode(['config', '--int', '--get', git_key])
    try:
      return int(stdout.strip())
    except ValueError:
      pass
  return default


def add_git_similarity(parser):
  parser.add_option(
      '--similarity', metavar='SIM', type='int', action='store',
      help='Sets the percentage that a pair of files need to match in order to'
           ' be considered copies (default 50)')
  parser.add_option(
      '--find-copies', action='store_true',
      help='Allows git to look for copies.')
  parser.add_option(
      '--no-find-copies', action='store_false', dest='find_copies',
      help='Disallows git from looking for copies.')

  old_parser_args = parser.parse_args
  def Parse(args):
    options, args = old_parser_args(args)

    if options.similarity is None:
      options.similarity = git_get_branch_default('git-cl-similarity', 50)
    else:
      print('Note: Saving similarity of %d%% in git config.'
            % options.similarity)
      git_set_branch_value('git-cl-similarity', options.similarity)

    options.similarity = max(0, min(options.similarity, 100))

    if options.find_copies is None:
      options.find_copies = bool(
          git_get_branch_default('git-find-copies', True))
    else:
      git_set_branch_value('git-find-copies', int(options.find_copies))

    print('Using %d%% similarity for rename/copy detection. '
          'Override with --similarity.' % options.similarity)

    return options, args
  parser.parse_args = Parse


def _get_properties_from_options(options):
  properties = dict(x.split('=', 1) for x in options.properties)
  for key, val in properties.iteritems():
    try:
      properties[key] = json.loads(val)
    except ValueError:
      pass  # If a value couldn't be evaluated, treat it as a string.
  return properties


def _prefix_master(master):
  """Convert user-specified master name to full master name.

  Buildbucket uses full master name(master.tryserver.chromium.linux) as bucket
  name, while the developers always use shortened master name
  (tryserver.chromium.linux) by stripping off the prefix 'master.'. This
  function does the conversion for buildbucket migration.
  """
  prefix = 'master.'
  if master.startswith(prefix):
    return master
  return '%s%s' % (prefix, master)


def _buildbucket_retry(operation_name, http, *args, **kwargs):
  """Retries requests to buildbucket service and returns parsed json content."""
  try_count = 0
  while True:
    response, content = http.request(*args, **kwargs)
    try:
      content_json = json.loads(content)
    except ValueError:
      content_json = None

    # Buildbucket could return an error even if status==200.
    if content_json and content_json.get('error'):
      error = content_json.get('error')
      if error.get('code') == 403:
        raise BuildbucketResponseException(
            'Access denied: %s' % error.get('message', ''))
      msg = 'Error in response. Reason: %s. Message: %s.' % (
          error.get('reason', ''), error.get('message', ''))
      raise BuildbucketResponseException(msg)

    if response.status == 200:
      if not content_json:
        raise BuildbucketResponseException(
            'Buildbucket returns invalid json content: %s.\n'
            'Please file bugs at http://crbug.com, label "Infra-BuildBucket".' %
            content)
      return content_json
    if response.status < 500 or try_count >= 2:
      raise httplib2.HttpLib2Error(content)

    # status >= 500 means transient failures.
    logging.debug('Transient errors when %s. Will retry.', operation_name)
    time.sleep(0.5 + 1.5*try_count)
    try_count += 1
  assert False, 'unreachable'


def trigger_luci_job(changelist, masters, options):
  """Send a job to run on LUCI."""
  issue_props = changelist.GetIssueProperties()
  issue = changelist.GetIssue()
  patchset = changelist.GetMostRecentPatchset()
  for builders_and_tests in sorted(masters.itervalues()):
    # TODO(hinoka et al): add support for other properties.
    # Currently, this completely ignores testfilter and other properties.
    for builder in sorted(builders_and_tests):
      luci_trigger.trigger(
          builder, 'HEAD', issue, patchset, issue_props['project'])


def trigger_try_jobs(auth_config, changelist, options, masters, category):
  rietveld_url = settings.GetDefaultServerUrl()
  rietveld_host = urlparse.urlparse(rietveld_url).hostname
  authenticator = auth.get_authenticator_for_host(rietveld_host, auth_config)
  http = authenticator.authorize(httplib2.Http())
  http.force_exception_to_status_code = True
  issue_props = changelist.GetIssueProperties()
  issue = changelist.GetIssue()
  patchset = changelist.GetMostRecentPatchset()
  properties = _get_properties_from_options(options)

  buildbucket_put_url = (
      'https://{hostname}/_ah/api/buildbucket/v1/builds/batch'.format(
          hostname=options.buildbucket_host))
  buildset = 'patch/rietveld/{hostname}/{issue}/{patch}'.format(
      hostname=rietveld_host,
      issue=issue,
      patch=patchset)

  batch_req_body = {'builds': []}
  print_text = []
  print_text.append('Tried jobs on:')
  for master, builders_and_tests in sorted(masters.iteritems()):
    print_text.append('Master: %s' % master)
    bucket = _prefix_master(master)
    for builder, tests in sorted(builders_and_tests.iteritems()):
      print_text.append('  %s: %s' % (builder, tests))
      parameters = {
          'builder_name': builder,
          'changes': [{
              'author': {'email': issue_props['owner_email']},
              'revision': options.revision,
          }],
          'properties': {
              'category': category,
              'issue': issue,
              'master': master,
              'patch_project': issue_props['project'],
              'patch_storage': 'rietveld',
              'patchset': patchset,
              'reason': options.name,
              'rietveld': rietveld_url,
          },
      }
      if 'presubmit' in builder.lower():
        parameters['properties']['dry_run'] = 'true'
      if tests:
        parameters['properties']['testfilter'] = tests
      if properties:
        parameters['properties'].update(properties)
      if options.clobber:
        parameters['properties']['clobber'] = True
      batch_req_body['builds'].append(
          {
              'bucket': bucket,
              'parameters_json': json.dumps(parameters),
              'client_operation_id': str(uuid.uuid4()),
              'tags': ['builder:%s' % builder,
                       'buildset:%s' % buildset,
                       'master:%s' % master,
                       'user_agent:git_cl_try']
          }
      )

  _buildbucket_retry(
      'triggering tryjobs',
      http,
      buildbucket_put_url,
      'PUT',
      body=json.dumps(batch_req_body),
      headers={'Content-Type': 'application/json'}
  )
  print_text.append('To see results here, run:        git cl try-results')
  print_text.append('To see results in browser, run:  git cl web')
  print('\n'.join(print_text))


def fetch_try_jobs(auth_config, changelist, options):
  """Fetches tryjobs from buildbucket.

  Returns a map from build id to build info as json dictionary.
  """
  rietveld_url = settings.GetDefaultServerUrl()
  rietveld_host = urlparse.urlparse(rietveld_url).hostname
  authenticator = auth.get_authenticator_for_host(rietveld_host, auth_config)
  if authenticator.has_cached_credentials():
    http = authenticator.authorize(httplib2.Http())
  else:
    print('Warning: Some results might be missing because %s' %
          # Get the message on how to login.
          (auth.LoginRequiredError(rietveld_host).message,))
    http = httplib2.Http()

  http.force_exception_to_status_code = True

  buildset = 'patch/rietveld/{hostname}/{issue}/{patch}'.format(
      hostname=rietveld_host,
      issue=changelist.GetIssue(),
      patch=options.patchset)
  params = {'tag': 'buildset:%s' % buildset}

  builds = {}
  while True:
    url = 'https://{hostname}/_ah/api/buildbucket/v1/search?{params}'.format(
        hostname=options.buildbucket_host,
        params=urllib.urlencode(params))
    content = _buildbucket_retry('fetching tryjobs', http, url, 'GET')
    for build in content.get('builds', []):
      builds[build['id']] = build
    if 'next_cursor' in content:
      params['start_cursor'] = content['next_cursor']
    else:
      break
  return builds


def print_tryjobs(options, builds):
  """Prints nicely result of fetch_try_jobs."""
  if not builds:
    print('No tryjobs scheduled')
    return

  # Make a copy, because we'll be modifying builds dictionary.
  builds = builds.copy()
  builder_names_cache = {}

  def get_builder(b):
    try:
      return builder_names_cache[b['id']]
    except KeyError:
      try:
        parameters = json.loads(b['parameters_json'])
        name = parameters['builder_name']
      except (ValueError, KeyError) as error:
        print('WARNING: failed to get builder name for build %s: %s' % (
              b['id'], error))
        name = None
      builder_names_cache[b['id']] = name
      return name

  def get_bucket(b):
    bucket = b['bucket']
    if bucket.startswith('master.'):
      return bucket[len('master.'):]
    return bucket

  if options.print_master:
    name_fmt = '%%-%ds %%-%ds' % (
        max(len(str(get_bucket(b))) for b in builds.itervalues()),
        max(len(str(get_builder(b))) for b in builds.itervalues()))
    def get_name(b):
      return name_fmt % (get_bucket(b), get_builder(b))
  else:
    name_fmt = '%%-%ds' % (
        max(len(str(get_builder(b))) for b in builds.itervalues()))
    def get_name(b):
      return name_fmt % get_builder(b)

  def sort_key(b):
    return b['status'], b.get('result'), get_name(b), b.get('url')

  def pop(title, f, color=None, **kwargs):
    """Pop matching builds from `builds` dict and print them."""

    if not options.color or color is None:
      colorize = str
    else:
      colorize = lambda x: '%s%s%s' % (color, x, Fore.RESET)

    result = []
    for b in builds.values():
      if all(b.get(k) == v for k, v in kwargs.iteritems()):
        builds.pop(b['id'])
        result.append(b)
    if result:
      print(colorize(title))
      for b in sorted(result, key=sort_key):
        print(' ', colorize('\t'.join(map(str, f(b)))))

  total = len(builds)
  pop(status='COMPLETED', result='SUCCESS',
      title='Successes:', color=Fore.GREEN,
      f=lambda b: (get_name(b), b.get('url')))
  pop(status='COMPLETED', result='FAILURE', failure_reason='INFRA_FAILURE',
      title='Infra Failures:', color=Fore.MAGENTA,
      f=lambda b: (get_name(b), b.get('url')))
  pop(status='COMPLETED', result='FAILURE', failure_reason='BUILD_FAILURE',
      title='Failures:', color=Fore.RED,
      f=lambda b: (get_name(b), b.get('url')))
  pop(status='COMPLETED', result='CANCELED',
      title='Canceled:', color=Fore.MAGENTA,
      f=lambda b: (get_name(b),))
  pop(status='COMPLETED', result='FAILURE',
      failure_reason='INVALID_BUILD_DEFINITION',
      title='Wrong master/builder name:', color=Fore.MAGENTA,
      f=lambda b: (get_name(b),))
  pop(status='COMPLETED', result='FAILURE',
      title='Other failures:',
      f=lambda b: (get_name(b), b.get('failure_reason'), b.get('url')))
  pop(status='COMPLETED',
      title='Other finished:',
      f=lambda b: (get_name(b), b.get('result'), b.get('url')))
  pop(status='STARTED',
      title='Started:', color=Fore.YELLOW,
      f=lambda b: (get_name(b), b.get('url')))
  pop(status='SCHEDULED',
      title='Scheduled:',
      f=lambda b: (get_name(b), 'id=%s' % b['id']))
  # The last section is just in case buildbucket API changes OR there is a bug.
  pop(title='Other:',
      f=lambda b: (get_name(b), 'id=%s' % b['id']))
  assert len(builds) == 0
  print('Total: %d tryjobs' % total)


def MatchSvnGlob(url, base_url, glob_spec, allow_wildcards):
  """Return the corresponding git ref if |base_url| together with |glob_spec|
  matches the full |url|.

  If |allow_wildcards| is true, |glob_spec| can contain wildcards (see below).
  """
  fetch_suburl, as_ref = glob_spec.split(':')
  if allow_wildcards:
    glob_match = re.match('(.+/)?(\*|{[^/]*})(/.+)?', fetch_suburl)
    if glob_match:
      # Parse specs like "branches/*/src:refs/remotes/svn/*" or
      # "branches/{472,597,648}/src:refs/remotes/svn/*".
      branch_re = re.escape(base_url)
      if glob_match.group(1):
        branch_re += '/' + re.escape(glob_match.group(1))
      wildcard = glob_match.group(2)
      if wildcard == '*':
        branch_re += '([^/]*)'
      else:
        # Escape and replace surrounding braces with parentheses and commas
        # with pipe symbols.
        wildcard = re.escape(wildcard)
        wildcard = re.sub('^\\\\{', '(', wildcard)
        wildcard = re.sub('\\\\,', '|', wildcard)
        wildcard = re.sub('\\\\}$', ')', wildcard)
        branch_re += wildcard
      if glob_match.group(3):
        branch_re += re.escape(glob_match.group(3))
      match = re.match(branch_re, url)
      if match:
        return re.sub('\*$', match.group(1), as_ref)

  # Parse specs like "trunk/src:refs/remotes/origin/trunk".
  if fetch_suburl:
    full_url = base_url + '/' + fetch_suburl
  else:
    full_url = base_url
  if full_url == url:
    return as_ref
  return None


def print_stats(similarity, find_copies, args):
  """Prints statistics about the change to the user."""
  # --no-ext-diff is broken in some versions of Git, so try to work around
  # this by overriding the environment (but there is still a problem if the
  # git config key "diff.external" is used).
  env = GetNoGitPagerEnv()
  if 'GIT_EXTERNAL_DIFF' in env:
    del env['GIT_EXTERNAL_DIFF']

  if find_copies:
    similarity_options = ['--find-copies-harder', '-l100000',
                          '-C%s' % similarity]
  else:
    similarity_options = ['-M%s' % similarity]

  try:
    stdout = sys.stdout.fileno()
  except AttributeError:
    stdout = None
  return subprocess2.call(
      ['git',
       'diff', '--no-ext-diff', '--stat'] + similarity_options + args,
      stdout=stdout, env=env)


class BuildbucketResponseException(Exception):
  pass


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
    self.is_gerrit = None
    self.squash_gerrit_uploads = None
    self.gerrit_skip_ensure_authenticated = None
    self.git_editor = None
    self.project = None
    self.force_https_commit_url = None
    self.pending_ref_prefix = None

  def LazyUpdateIfNeeded(self):
    """Updates the settings from a codereview.settings file, if available."""
    if not self.updated:
      # The only value that actually changes the behavior is
      # autoupdate = "false". Everything else means "true".
      autoupdate = RunGit(['config', 'rietveld.autoupdate'],
                          error_ok=True
                          ).strip().lower()

      cr_settings_file = FindCodereviewSettingsFile()
      if autoupdate != 'false' and cr_settings_file:
        LoadCodereviewSettingsFromFile(cr_settings_file)
      self.updated = True

  def GetDefaultServerUrl(self, error_ok=False):
    if not self.default_server:
      self.LazyUpdateIfNeeded()
      self.default_server = gclient_utils.UpgradeToHttps(
          self._GetRietveldConfig('server', error_ok=True))
      if error_ok:
        return self.default_server
      if not self.default_server:
        error_message = ('Could not find settings file. You must configure '
                         'your review setup by running "git cl config".')
        self.default_server = gclient_utils.UpgradeToHttps(
            self._GetRietveldConfig('server', error_message=error_message))
    return self.default_server

  @staticmethod
  def GetRelativeRoot():
    return RunGit(['rev-parse', '--show-cdup']).strip()

  def GetRoot(self):
    if self.root is None:
      self.root = os.path.abspath(self.GetRelativeRoot())
    return self.root

  def GetGitMirror(self, remote='origin'):
    """If this checkout is from a local git mirror, return a Mirror object."""
    local_url = RunGit(['config', '--get', 'remote.%s.url' % remote]).strip()
    if not os.path.isdir(local_url):
      return None
    git_cache.Mirror.SetCachePath(os.path.dirname(local_url))
    remote_url = git_cache.Mirror.CacheDirToUrl(local_url)
    # Use the /dev/null print_func to avoid terminal spew in WaitForRealCommit.
    mirror = git_cache.Mirror(remote_url, print_func = lambda *args: None)
    if mirror.exists():
      return mirror
    return None

  def GetIsGitSvn(self):
    """Return true if this repo looks like it's using git-svn."""
    if self.is_git_svn is None:
      if self.GetPendingRefPrefix():
        # If PENDING_REF_PREFIX is set then it's a pure git repo no matter what.
        self.is_git_svn = False
      else:
        # If you have any "svn-remote.*" config keys, we think you're using svn.
        self.is_git_svn = RunGitWithCode(
            ['config', '--local', '--get-regexp', r'^svn-remote\.'])[0] == 0
    return self.is_git_svn

  def GetSVNBranch(self):
    if self.svn_branch is None:
      if not self.GetIsGitSvn():
        DieWithError('Repo doesn\'t appear to be a git-svn repo.')

      # Try to figure out which remote branch we're based on.
      # Strategy:
      # 1) iterate through our branch history and find the svn URL.
      # 2) find the svn-remote that fetches from the URL.

      # regexp matching the git-svn line that contains the URL.
      git_svn_re = re.compile(r'^\s*git-svn-id: (\S+)@', re.MULTILINE)

      # We don't want to go through all of history, so read a line from the
      # pipe at a time.
      # The -100 is an arbitrary limit so we don't search forever.
      cmd = ['git', 'log', '-100', '--pretty=medium']
      proc = subprocess2.Popen(cmd, stdout=subprocess2.PIPE,
                               env=GetNoGitPagerEnv())
      url = None
      for line in proc.stdout:
        match = git_svn_re.match(line)
        if match:
          url = match.group(1)
          proc.stdout.close()  # Cut pipe.
          break

      if url:
        svn_remote_re = re.compile(r'^svn-remote\.([^.]+)\.url (.*)$')
        remotes = RunGit(['config', '--get-regexp',
                          r'^svn-remote\..*\.url']).splitlines()
        for remote in remotes:
          match = svn_remote_re.match(remote)
          if match:
            remote = match.group(1)
            base_url = match.group(2)
            rewrite_root = RunGit(
                ['config', 'svn-remote.%s.rewriteRoot' % remote],
                error_ok=True).strip()
            if rewrite_root:
              base_url = rewrite_root
            fetch_spec = RunGit(
                ['config', 'svn-remote.%s.fetch' % remote],
                error_ok=True).strip()
            if fetch_spec:
              self.svn_branch = MatchSvnGlob(url, base_url, fetch_spec, False)
              if self.svn_branch:
                break
            branch_spec = RunGit(
                ['config', 'svn-remote.%s.branches' % remote],
                error_ok=True).strip()
            if branch_spec:
              self.svn_branch = MatchSvnGlob(url, base_url, branch_spec, True)
              if self.svn_branch:
                break
            tag_spec = RunGit(
                ['config', 'svn-remote.%s.tags' % remote],
                error_ok=True).strip()
            if tag_spec:
              self.svn_branch = MatchSvnGlob(url, base_url, tag_spec, True)
              if self.svn_branch:
                break

      if not self.svn_branch:
        DieWithError('Can\'t guess svn branch -- try specifying it on the '
            'command line')

    return self.svn_branch

  def GetTreeStatusUrl(self, error_ok=False):
    if not self.tree_status_url:
      error_message = ('You must configure your tree status URL by running '
                       '"git cl config".')
      self.tree_status_url = self._GetRietveldConfig(
          'tree-status-url', error_ok=error_ok, error_message=error_message)
    return self.tree_status_url

  def GetViewVCUrl(self):
    if not self.viewvc_url:
      self.viewvc_url = self._GetRietveldConfig('viewvc-url', error_ok=True)
    return self.viewvc_url

  def GetBugPrefix(self):
    return self._GetRietveldConfig('bug-prefix', error_ok=True)

  def GetIsSkipDependencyUpload(self, branch_name):
    """Returns true if specified branch should skip dep uploads."""
    return self._GetBranchConfig(branch_name, 'skip-deps-uploads',
                                 error_ok=True)

  def GetRunPostUploadHook(self):
    run_post_upload_hook = self._GetRietveldConfig(
        'run-post-upload-hook', error_ok=True)
    return run_post_upload_hook == "True"

  def GetDefaultCCList(self):
    return self._GetRietveldConfig('cc', error_ok=True)

  def GetDefaultPrivateFlag(self):
    return self._GetRietveldConfig('private', error_ok=True)

  def GetIsGerrit(self):
    """Return true if this repo is assosiated with gerrit code review system."""
    if self.is_gerrit is None:
      self.is_gerrit = self._GetConfig('gerrit.host', error_ok=True)
    return self.is_gerrit

  def GetSquashGerritUploads(self):
    """Return true if uploads to Gerrit should be squashed by default."""
    if self.squash_gerrit_uploads is None:
      self.squash_gerrit_uploads = self.GetSquashGerritUploadsOverride()
      if self.squash_gerrit_uploads is None:
        # Default is squash now (http://crbug.com/611892#c23).
        self.squash_gerrit_uploads = not (
            RunGit(['config', '--bool', 'gerrit.squash-uploads'],
                   error_ok=True).strip() == 'false')
    return self.squash_gerrit_uploads

  def GetSquashGerritUploadsOverride(self):
    """Return True or False if codereview.settings should be overridden.

    Returns None if no override has been defined.
    """
    # See also http://crbug.com/611892#c23
    result = RunGit(['config', '--bool', 'gerrit.override-squash-uploads'],
                    error_ok=True).strip()
    if result == 'true':
      return True
    if result == 'false':
      return False
    return None

  def GetGerritSkipEnsureAuthenticated(self):
    """Return True if EnsureAuthenticated should not be done for Gerrit
    uploads."""
    if self.gerrit_skip_ensure_authenticated is None:
      self.gerrit_skip_ensure_authenticated = (
          RunGit(['config', '--bool', 'gerrit.skip-ensure-authenticated'],
                 error_ok=True).strip() == 'true')
    return self.gerrit_skip_ensure_authenticated

  def GetGitEditor(self):
    """Return the editor specified in the git config, or None if none is."""
    if self.git_editor is None:
      self.git_editor = self._GetConfig('core.editor', error_ok=True)
    return self.git_editor or None

  def GetLintRegex(self):
    return (self._GetRietveldConfig('cpplint-regex', error_ok=True) or
            DEFAULT_LINT_REGEX)

  def GetLintIgnoreRegex(self):
    return (self._GetRietveldConfig('cpplint-ignore-regex', error_ok=True) or
            DEFAULT_LINT_IGNORE_REGEX)

  def GetProject(self):
    if not self.project:
      self.project = self._GetRietveldConfig('project', error_ok=True)
    return self.project

  def GetForceHttpsCommitUrl(self):
    if not self.force_https_commit_url:
      self.force_https_commit_url = self._GetRietveldConfig(
          'force-https-commit-url', error_ok=True)
    return self.force_https_commit_url

  def GetPendingRefPrefix(self):
    if not self.pending_ref_prefix:
      self.pending_ref_prefix = self._GetRietveldConfig(
          'pending-ref-prefix', error_ok=True)
    return self.pending_ref_prefix

  def _GetRietveldConfig(self, param, **kwargs):
    return self._GetConfig('rietveld.' + param, **kwargs)

  def _GetBranchConfig(self, branch_name, param, **kwargs):
    return self._GetConfig('branch.' + branch_name + '.' + param, **kwargs)

  def _GetConfig(self, param, **kwargs):
    self.LazyUpdateIfNeeded()
    return RunGit(['config', param], **kwargs).strip()


def ShortBranchName(branch):
  """Convert a name like 'refs/heads/foo' to just 'foo'."""
  return branch.replace('refs/heads/', '', 1)


def GetCurrentBranchRef():
  """Returns branch ref (e.g., refs/heads/master) or None."""
  return RunGit(['symbolic-ref', 'HEAD'],
                stderr=subprocess2.VOID, error_ok=True).strip() or None


def GetCurrentBranch():
  """Returns current branch or None.

  For refs/heads/* branches, returns just last part. For others, full ref.
  """
  branchref = GetCurrentBranchRef()
  if branchref:
    return ShortBranchName(branchref)
  return None


class _CQState(object):
  """Enum for states of CL with respect to Commit Queue."""
  NONE = 'none'
  DRY_RUN = 'dry_run'
  COMMIT = 'commit'

  ALL_STATES = [NONE, DRY_RUN, COMMIT]


class _ParsedIssueNumberArgument(object):
  def __init__(self, issue=None, patchset=None, hostname=None):
    self.issue = issue
    self.patchset = patchset
    self.hostname = hostname

  @property
  def valid(self):
    return self.issue is not None


class _RietveldParsedIssueNumberArgument(_ParsedIssueNumberArgument):
  def __init__(self, *args, **kwargs):
    self.patch_url = kwargs.pop('patch_url', None)
    super(_RietveldParsedIssueNumberArgument, self).__init__(*args, **kwargs)


def ParseIssueNumberArgument(arg):
  """Parses the issue argument and returns _ParsedIssueNumberArgument."""
  fail_result = _ParsedIssueNumberArgument()

  if arg.isdigit():
    return _ParsedIssueNumberArgument(issue=int(arg))
  if not arg.startswith('http'):
    return fail_result
  url = gclient_utils.UpgradeToHttps(arg)
  try:
    parsed_url = urlparse.urlparse(url)
  except ValueError:
    return fail_result
  for cls in _CODEREVIEW_IMPLEMENTATIONS.itervalues():
    tmp = cls.ParseIssueURL(parsed_url)
    if tmp is not None:
      return tmp
  return fail_result


class Changelist(object):
  """Changelist works with one changelist in local branch.

  Supports two codereview backends: Rietveld or Gerrit, selected at object
  creation.

  Notes:
    * Not safe for concurrent multi-{thread,process} use.
    * Caches values from current branch. Therefore, re-use after branch change
      with care.
  """

  def __init__(self, branchref=None, issue=None, codereview=None, **kwargs):
    """Create a new ChangeList instance.

    If issue is given, the codereview must be given too.

    If `codereview` is given, it must be 'rietveld' or 'gerrit'.
    Otherwise, it's decided based on current configuration of the local branch,
    with default being 'rietveld' for backwards compatibility.
    See _load_codereview_impl for more details.

    **kwargs will be passed directly to codereview implementation.
    """
    # Poke settings so we get the "configure your server" message if necessary.
    global settings
    if not settings:
      # Happens when git_cl.py is used as a utility library.
      settings = Settings()

    if issue:
      assert codereview, 'codereview must be known, if issue is known'

    self.branchref = branchref
    if self.branchref:
      assert branchref.startswith('refs/heads/')
      self.branch = ShortBranchName(self.branchref)
    else:
      self.branch = None
    self.upstream_branch = None
    self.lookedup_issue = False
    self.issue = issue or None
    self.has_description = False
    self.description = None
    self.lookedup_patchset = False
    self.patchset = None
    self.cc = None
    self.watchers = ()
    self._remote = None

    self._codereview_impl = None
    self._codereview = None
    self._load_codereview_impl(codereview, **kwargs)
    assert self._codereview_impl
    assert self._codereview in _CODEREVIEW_IMPLEMENTATIONS

  def _load_codereview_impl(self, codereview=None, **kwargs):
    if codereview:
      assert codereview in _CODEREVIEW_IMPLEMENTATIONS
      cls = _CODEREVIEW_IMPLEMENTATIONS[codereview]
      self._codereview = codereview
      self._codereview_impl = cls(self, **kwargs)
      return

    # Automatic selection based on issue number set for a current branch.
    # Rietveld takes precedence over Gerrit.
    assert not self.issue
    # Whether we find issue or not, we are doing the lookup.
    self.lookedup_issue = True
    for codereview, cls in _CODEREVIEW_IMPLEMENTATIONS.iteritems():
      setting = cls.IssueSetting(self.GetBranch())
      issue = RunGit(['config', setting], error_ok=True).strip()
      if issue:
        self._codereview = codereview
        self._codereview_impl = cls(self, **kwargs)
        self.issue = int(issue)
        return

    # No issue is set for this branch, so decide based on repo-wide settings.
    return self._load_codereview_impl(
        codereview='gerrit' if settings.GetIsGerrit() else 'rietveld',
        **kwargs)

  def IsGerrit(self):
    return self._codereview == 'gerrit'

  def GetCCList(self):
    """Return the users cc'd on this CL.

    Return is a string suitable for passing to gcl with the --cc flag.
    """
    if self.cc is None:
      base_cc = settings.GetDefaultCCList()
      more_cc = ','.join(self.watchers)
      self.cc = ','.join(filter(None, (base_cc, more_cc))) or ''
    return self.cc

  def GetCCListWithoutDefault(self):
    """Return the users cc'd on this CL excluding default ones."""
    if self.cc is None:
      self.cc = ','.join(self.watchers)
    return self.cc

  def SetWatchers(self, watchers):
    """Set the list of email addresses that should be cc'd based on the changed
       files in this CL.
    """
    self.watchers = watchers

  def GetBranch(self):
    """Returns the short branch name, e.g. 'master'."""
    if not self.branch:
      branchref = GetCurrentBranchRef()
      if not branchref:
        return None
      self.branchref = branchref
      self.branch = ShortBranchName(self.branchref)
    return self.branch

  def GetBranchRef(self):
    """Returns the full branch name, e.g. 'refs/heads/master'."""
    self.GetBranch()  # Poke the lazy loader.
    return self.branchref

  def ClearBranch(self):
    """Clears cached branch data of this object."""
    self.branch = self.branchref = None

  @staticmethod
  def FetchUpstreamTuple(branch):
    """Returns a tuple containing remote and remote ref,
       e.g. 'origin', 'refs/heads/master'
    """
    remote = '.'
    upstream_branch = RunGit(['config', 'branch.%s.merge' % branch],
                             error_ok=True).strip()
    if upstream_branch:
      remote = RunGit(['config', 'branch.%s.remote' % branch]).strip()
    else:
      upstream_branch = RunGit(['config', 'rietveld.upstream-branch'],
                               error_ok=True).strip()
      if upstream_branch:
        remote = RunGit(['config', 'rietveld.upstream-remote']).strip()
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
            DieWithError(
               'Unable to determine default branch to diff against.\n'
               'Either pass complete "git diff"-style arguments, like\n'
               '  git cl upload origin/master\n'
               'or verify this branch is set up to track another \n'
               '(via the --track argument to "git checkout -b ...").')

    return remote, upstream_branch

  def GetCommonAncestorWithUpstream(self):
    upstream_branch = self.GetUpstreamBranch()
    if not BranchExists(upstream_branch):
      DieWithError('The upstream for the current branch (%s) does not exist '
                   'anymore.\nPlease fix it and try again.' % self.GetBranch())
    return git_common.get_or_create_merge_base(self.GetBranch(),
                                               upstream_branch)

  def GetUpstreamBranch(self):
    if self.upstream_branch is None:
      remote, upstream_branch = self.FetchUpstreamTuple(self.GetBranch())
      if remote is not '.':
        upstream_branch = upstream_branch.replace('refs/heads/',
                                                  'refs/remotes/%s/' % remote)
        upstream_branch = upstream_branch.replace('refs/branch-heads/',
                                                  'refs/remotes/branch-heads/')
      self.upstream_branch = upstream_branch
    return self.upstream_branch

  def GetRemoteBranch(self):
    if not self._remote:
      remote, branch = None, self.GetBranch()
      seen_branches = set()
      while branch not in seen_branches:
        seen_branches.add(branch)
        remote, branch = self.FetchUpstreamTuple(branch)
        branch = ShortBranchName(branch)
        if remote != '.' or branch.startswith('refs/remotes'):
          break
      else:
        remotes = RunGit(['remote'], error_ok=True).split()
        if len(remotes) == 1:
          remote, = remotes
        elif 'origin' in remotes:
          remote = 'origin'
          logging.warning('Could not determine which remote this change is '
                          'associated with, so defaulting to "%s".  This may '
                          'not be what you want.  You may prevent this message '
                          'by running "git svn info" as documented here:  %s',
                          self._remote,
                          GIT_INSTRUCTIONS_URL)
        else:
          logging.warn('Could not determine which remote this change is '
                       'associated with.  You may prevent this message by '
                       'running "git svn info" as documented here:  %s',
                       GIT_INSTRUCTIONS_URL)
        branch = 'HEAD'
      if branch.startswith('refs/remotes'):
        self._remote = (remote, branch)
      elif branch.startswith('refs/branch-heads/'):
        self._remote = (remote, branch.replace('refs/', 'refs/remotes/'))
      else:
        self._remote = (remote, 'refs/remotes/%s/%s' % (remote, branch))
    return self._remote

  def GitSanityChecks(self, upstream_git_obj):
    """Checks git repo status and ensures diff is from local commits."""

    if upstream_git_obj is None:
      if self.GetBranch() is None:
        print('ERROR: unable to determine current branch (detached HEAD?)',
              file=sys.stderr)
      else:
        print('ERROR: no upstream branch', file=sys.stderr)
      return False

    # Verify the commit we're diffing against is in our current branch.
    upstream_sha = RunGit(['rev-parse', '--verify', upstream_git_obj]).strip()
    common_ancestor = RunGit(['merge-base', upstream_sha, 'HEAD']).strip()
    if upstream_sha != common_ancestor:
      print('ERROR: %s is not in the current branch.  You may need to rebase '
            'your tracking branch' % upstream_sha, file=sys.stderr)
      return False

    # List the commits inside the diff, and verify they are all local.
    commits_in_diff = RunGit(
        ['rev-list', '^%s' % upstream_sha, 'HEAD']).splitlines()
    code, remote_branch = RunGitWithCode(['config', 'gitcl.remotebranch'])
    remote_branch = remote_branch.strip()
    if code != 0:
      _, remote_branch = self.GetRemoteBranch()

    commits_in_remote = RunGit(
        ['rev-list', '^%s' % upstream_sha, remote_branch]).splitlines()

    common_commits = set(commits_in_diff) & set(commits_in_remote)
    if common_commits:
      print('ERROR: Your diff contains %d commits already in %s.\n'
            'Run "git log --oneline %s..HEAD" to get a list of commits in '
            'the diff.  If you are using a custom git flow, you can override'
            ' the reference used for this check with "git config '
            'gitcl.remotebranch <git-ref>".' % (
                len(common_commits), remote_branch, upstream_git_obj),
            file=sys.stderr)
      return False
    return True

  def GetGitBaseUrlFromConfig(self):
    """Return the configured base URL from branch.<branchname>.baseurl.

    Returns None if it is not set.
    """
    return RunGit(['config', 'branch.%s.base-url' % self.GetBranch()],
                  error_ok=True).strip()

  def GetGitSvnRemoteUrl(self):
    """Return the configured git-svn remote URL parsed from git svn info.

    Returns None if it is not set.
    """
    # URL is dependent on the current directory.
    data = RunGit(['svn', 'info'], cwd=settings.GetRoot())
    if data:
      keys = dict(line.split(': ', 1) for line in data.splitlines()
                  if ': ' in line)
      return keys.get('URL', None)
    return None

  def GetRemoteUrl(self):
    """Return the configured remote URL, e.g. 'git://example.org/foo.git/'.

    Returns None if there is no remote.
    """
    remote, _ = self.GetRemoteBranch()
    url = RunGit(['config', 'remote.%s.url' % remote], error_ok=True).strip()

    # If URL is pointing to a local directory, it is probably a git cache.
    if os.path.isdir(url):
      url = RunGit(['config', 'remote.%s.url' % remote],
                   error_ok=True,
                   cwd=url).strip()
    return url

  def GetIssue(self):
    """Returns the issue number as a int or None if not set."""
    if self.issue is None and not self.lookedup_issue:
      issue = RunGit(['config',
                      self._codereview_impl.IssueSetting(self.GetBranch())],
                     error_ok=True).strip()
      self.issue = int(issue) or None if issue else None
      self.lookedup_issue = True
    return self.issue

  def GetIssueURL(self):
    """Get the URL for a particular issue."""
    issue = self.GetIssue()
    if not issue:
      return None
    return '%s/%s' % (self._codereview_impl.GetCodereviewServer(), issue)

  def GetDescription(self, pretty=False):
    if not self.has_description:
      if self.GetIssue():
        self.description = self._codereview_impl.FetchDescription()
      self.has_description = True
    if pretty:
      wrapper = textwrap.TextWrapper()
      wrapper.initial_indent = wrapper.subsequent_indent = '  '
      return wrapper.fill(self.description)
    return self.description

  def GetPatchset(self):
    """Returns the patchset number as a int or None if not set."""
    if self.patchset is None and not self.lookedup_patchset:
      patchset = RunGit(['config', self._codereview_impl.PatchsetSetting()],
                        error_ok=True).strip()
      self.patchset = int(patchset) or None if patchset else None
      self.lookedup_patchset = True
    return self.patchset

  def SetPatchset(self, patchset):
    """Set this branch's patchset.  If patchset=0, clears the patchset."""
    patchset_setting = self._codereview_impl.PatchsetSetting()
    if patchset:
      RunGit(['config', patchset_setting, str(patchset)])
      self.patchset = patchset
    else:
      RunGit(['config', '--unset', patchset_setting],
             stderr=subprocess2.PIPE, error_ok=True)
      self.patchset = None

  def SetIssue(self, issue=None):
    """Set this branch's issue.  If issue isn't given, clears the issue."""
    issue_setting = self._codereview_impl.IssueSetting(self.GetBranch())
    codereview_setting = self._codereview_impl.GetCodereviewServerSetting()
    if issue:
      self.issue = issue
      RunGit(['config', issue_setting, str(issue)])
      codereview_server = self._codereview_impl.GetCodereviewServer()
      if codereview_server:
        RunGit(['config', codereview_setting, codereview_server])
    else:
      # Reset it regardless. It doesn't hurt.
      config_settings = [issue_setting, self._codereview_impl.PatchsetSetting()]
      for prop in (['last-upload-hash'] +
                    self._codereview_impl._PostUnsetIssueProperties()):
        config_settings.append('branch.%s.%s' % (self.GetBranch(), prop))
      for setting in config_settings:
        RunGit(['config', '--unset', setting], error_ok=True)
      self.issue = None
      self.patchset = None

  def GetChange(self, upstream_branch, author):
    if not self.GitSanityChecks(upstream_branch):
      DieWithError('\nGit sanity check failure')

    root = settings.GetRelativeRoot()
    if not root:
      root = '.'
    absroot = os.path.abspath(root)

    # We use the sha1 of HEAD as a name of this change.
    name = RunGitWithCode(['rev-parse', 'HEAD'])[1].strip()
    # Need to pass a relative path for msysgit.
    try:
      files = scm.GIT.CaptureStatus([root], '.', upstream_branch)
    except subprocess2.CalledProcessError:
      DieWithError(
          ('\nFailed to diff against upstream branch %s\n\n'
           'This branch probably doesn\'t exist anymore. To reset the\n'
           'tracking branch, please run\n'
           '    git branch --set-upstream %s trunk\n'
           'replacing trunk with origin/master or the relevant branch') %
          (upstream_branch, self.GetBranch()))

    issue = self.GetIssue()
    patchset = self.GetPatchset()
    if issue:
      description = self.GetDescription()
    else:
      # If the change was never uploaded, use the log messages of all commits
      # up to the branch point, as git cl upload will prefill the description
      # with these log messages.
      args = ['log', '--pretty=format:%s%n%n%b', '%s...' % (upstream_branch)]
      description = RunGitWithCode(args)[1].strip()

    if not author:
      author = RunGit(['config', 'user.email']).strip() or None
    return presubmit_support.GitChange(
        name,
        description,
        absroot,
        files,
        issue,
        patchset,
        author,
        upstream=upstream_branch)

  def UpdateDescription(self, description):
    self.description = description
    return self._codereview_impl.UpdateDescriptionRemote(description)

  def RunHook(self, committing, may_prompt, verbose, change):
    """Calls sys.exit() if the hook fails; returns a HookResults otherwise."""
    try:
      return presubmit_support.DoPresubmitChecks(change, committing,
          verbose=verbose, output_stream=sys.stdout, input_stream=sys.stdin,
          default_presubmit=None, may_prompt=may_prompt,
          rietveld_obj=self._codereview_impl.GetRieveldObjForPresubmit(),
          gerrit_obj=self._codereview_impl.GetGerritObjForPresubmit())
    except presubmit_support.PresubmitFailure as e:
      DieWithError(
          ('%s\nMaybe your depot_tools is out of date?\n'
           'If all fails, contact maruel@') % e)

  def CMDPatchIssue(self, issue_arg, reject, nocommit, directory):
    """Fetches and applies the issue patch from codereview to local branch."""
    if isinstance(issue_arg, (int, long)) or issue_arg.isdigit():
      parsed_issue_arg = _ParsedIssueNumberArgument(int(issue_arg))
    else:
      # Assume url.
      parsed_issue_arg = self._codereview_impl.ParseIssueURL(
          urlparse.urlparse(issue_arg))
    if not parsed_issue_arg or not parsed_issue_arg.valid:
      DieWithError('Failed to parse issue argument "%s". '
                   'Must be an issue number or a valid URL.' % issue_arg)
    return self._codereview_impl.CMDPatchWithParsedIssue(
        parsed_issue_arg, reject, nocommit, directory)

  def CMDUpload(self, options, git_diff_args, orig_args):
    """Uploads a change to codereview."""
    if git_diff_args:
      # TODO(ukai): is it ok for gerrit case?
      base_branch = git_diff_args[0]
    else:
      if self.GetBranch() is None:
        DieWithError('Can\'t upload from detached HEAD state. Get on a branch!')

      # Default to diffing against common ancestor of upstream branch
      base_branch = self.GetCommonAncestorWithUpstream()
      git_diff_args = [base_branch, 'HEAD']

    # Make sure authenticated to codereview before running potentially expensive
    # hooks.  It is a fast, best efforts check. Codereview still can reject the
    # authentication during the actual upload.
    self._codereview_impl.EnsureAuthenticated(force=options.force)

    # Apply watchlists on upload.
    change = self.GetChange(base_branch, None)
    watchlist = watchlists.Watchlists(change.RepositoryRoot())
    files = [f.LocalPath() for f in change.AffectedFiles()]
    if not options.bypass_watchlists:
      self.SetWatchers(watchlist.GetWatchersForPaths(files))

    if not options.bypass_hooks:
      if options.reviewers or options.tbr_owners:
        # Set the reviewer list now so that presubmit checks can access it.
        change_description = ChangeDescription(change.FullDescriptionText())
        change_description.update_reviewers(options.reviewers,
                                            options.tbr_owners,
                                            change)
        change.SetDescriptionText(change_description.description)
      hook_results = self.RunHook(committing=False,
                                may_prompt=not options.force,
                                verbose=options.verbose,
                                change=change)
      if not hook_results.should_continue():
        return 1
      if not options.reviewers and hook_results.reviewers:
        options.reviewers = hook_results.reviewers.split(',')

    if self.GetIssue():
      latest_patchset = self.GetMostRecentPatchset()
      local_patchset = self.GetPatchset()
      if (latest_patchset and local_patchset and
          local_patchset != latest_patchset):
        print('The last upload made from this repository was patchset #%d but '
              'the most recent patchset on the server is #%d.'
              % (local_patchset, latest_patchset))
        print('Uploading will still work, but if you\'ve uploaded to this '
              'issue from another machine or branch the patch you\'re '
              'uploading now might not include those changes.')
        ask_for_data('About to upload; enter to confirm.')

    print_stats(options.similarity, options.find_copies, git_diff_args)
    ret = self.CMDUploadChange(options, git_diff_args, change)
    if not ret:
      if options.use_commit_queue:
        self.SetCQState(_CQState.COMMIT)
      elif options.cq_dry_run:
        self.SetCQState(_CQState.DRY_RUN)

      git_set_branch_value('last-upload-hash',
                           RunGit(['rev-parse', 'HEAD']).strip())
      # Run post upload hooks, if specified.
      if settings.GetRunPostUploadHook():
        presubmit_support.DoPostUploadExecuter(
            change,
            self,
            settings.GetRoot(),
            options.verbose,
            sys.stdout)

      # Upload all dependencies if specified.
      if options.dependencies:
        print()
        print('--dependencies has been specified.')
        print('All dependent local branches will be re-uploaded.')
        print()
        # Remove the dependencies flag from args so that we do not end up in a
        # loop.
        orig_args.remove('--dependencies')
        ret = upload_branch_deps(self, orig_args)
    return ret

  def SetCQState(self, new_state):
    """Update the CQ state for latest patchset.

    Issue must have been already uploaded and known.
    """
    assert new_state in _CQState.ALL_STATES
    assert self.GetIssue()
    return self._codereview_impl.SetCQState(new_state)

  # Forward methods to codereview specific implementation.

  def CloseIssue(self):
    return self._codereview_impl.CloseIssue()

  def GetStatus(self):
    return self._codereview_impl.GetStatus()

  def GetCodereviewServer(self):
    return self._codereview_impl.GetCodereviewServer()

  def GetApprovingReviewers(self):
    return self._codereview_impl.GetApprovingReviewers()

  def GetMostRecentPatchset(self):
    return self._codereview_impl.GetMostRecentPatchset()

  def __getattr__(self, attr):
    # This is because lots of untested code accesses Rietveld-specific stuff
    # directly, and it's hard to fix for sure. So, just let it work, and fix
    # on a case by case basis.
    return getattr(self._codereview_impl, attr)


class _ChangelistCodereviewBase(object):
  """Abstract base class encapsulating codereview specifics of a changelist."""
  def __init__(self, changelist):
    self._changelist = changelist  # instance of Changelist

  def __getattr__(self, attr):
    # Forward methods to changelist.
    # TODO(tandrii): maybe clean up _GerritChangelistImpl and
    # _RietveldChangelistImpl to avoid this hack?
    return getattr(self._changelist, attr)

  def GetStatus(self):
    """Apply a rough heuristic to give a simple summary of an issue's review
    or CQ status, assuming adherence to a common workflow.

    Returns None if no issue for this branch, or specific string keywords.
    """
    raise NotImplementedError()

  def GetCodereviewServer(self):
    """Returns server URL without end slash, like "https://codereview.com"."""
    raise NotImplementedError()

  def FetchDescription(self):
    """Fetches and returns description from the codereview server."""
    raise NotImplementedError()

  def GetCodereviewServerSetting(self):
    """Returns git config setting for the codereview server."""
    raise NotImplementedError()

  @classmethod
  def IssueSetting(cls, branch):
    return 'branch.%s.%s' % (branch, cls.IssueSettingSuffix())

  @classmethod
  def IssueSettingSuffix(cls):
    """Returns name of git config setting which stores issue number for a given
    branch."""
    raise NotImplementedError()

  def PatchsetSetting(self):
    """Returns name of git config setting which stores issue number."""
    raise NotImplementedError()

  def _PostUnsetIssueProperties(self):
    """Which branch-specific properties to erase when unsettin issue."""
    raise NotImplementedError()

  def GetRieveldObjForPresubmit(self):
    # This is an unfortunate Rietveld-embeddedness in presubmit.
    # For non-Rietveld codereviews, this probably should return a dummy object.
    raise NotImplementedError()

  def GetGerritObjForPresubmit(self):
    # None is valid return value, otherwise presubmit_support.GerritAccessor.
    return None

  def UpdateDescriptionRemote(self, description):
    """Update the description on codereview site."""
    raise NotImplementedError()

  def CloseIssue(self):
    """Closes the issue."""
    raise NotImplementedError()

  def GetApprovingReviewers(self):
    """Returns a list of reviewers approving the change.

    Note: not necessarily committers.
    """
    raise NotImplementedError()

  def GetMostRecentPatchset(self):
    """Returns the most recent patchset number from the codereview site."""
    raise NotImplementedError()

  def CMDPatchWithParsedIssue(self, parsed_issue_arg, reject, nocommit,
                              directory):
    """Fetches and applies the issue.

    Arguments:
      parsed_issue_arg: instance of _ParsedIssueNumberArgument.
      reject: if True, reject the failed patch instead of switching to 3-way
        merge. Rietveld only.
      nocommit: do not commit the patch, thus leave the tree dirty. Rietveld
        only.
      directory: switch to directory before applying the patch. Rietveld only.
    """
    raise NotImplementedError()

  @staticmethod
  def ParseIssueURL(parsed_url):
    """Parses url and returns instance of _ParsedIssueNumberArgument or None if
    failed."""
    raise NotImplementedError()

  def EnsureAuthenticated(self, force):
    """Best effort check that user is authenticated with codereview server.

    Arguments:
      force: whether to skip confirmation questions.
    """
    raise NotImplementedError()

  def CMDUploadChange(self, options, args, change):
    """Uploads a change to codereview."""
    raise NotImplementedError()

  def SetCQState(self, new_state):
    """Update the CQ state for latest patchset.

    Issue must have been already uploaded and known.
    """
    raise NotImplementedError()


class _RietveldChangelistImpl(_ChangelistCodereviewBase):
  def __init__(self, changelist, auth_config=None, rietveld_server=None):
    super(_RietveldChangelistImpl, self).__init__(changelist)
    assert settings, 'must be initialized in _ChangelistCodereviewBase'
    if not rietveld_server:
      settings.GetDefaultServerUrl()

    self._rietveld_server = rietveld_server
    self._auth_config = auth_config
    self._props = None
    self._rpc_server = None

  def GetCodereviewServer(self):
    if not self._rietveld_server:
      # If we're on a branch then get the server potentially associated
      # with that branch.
      if self.GetIssue():
        rietveld_server_setting = self.GetCodereviewServerSetting()
        if rietveld_server_setting:
          self._rietveld_server = gclient_utils.UpgradeToHttps(RunGit(
              ['config', rietveld_server_setting], error_ok=True).strip())
      if not self._rietveld_server:
        self._rietveld_server = settings.GetDefaultServerUrl()
    return self._rietveld_server

  def EnsureAuthenticated(self, force):
    """Best effort check that user is authenticated with Rietveld server."""
    if self._auth_config.use_oauth2:
      authenticator = auth.get_authenticator_for_host(
          self.GetCodereviewServer(), self._auth_config)
      if not authenticator.has_cached_credentials():
        raise auth.LoginRequiredError(self.GetCodereviewServer())

  def FetchDescription(self):
    issue = self.GetIssue()
    assert issue
    try:
      return self.RpcServer().get_description(issue).strip()
    except urllib2.HTTPError as e:
      if e.code == 404:
        DieWithError(
            ('\nWhile fetching the description for issue %d, received a '
             '404 (not found)\n'
             'error. It is likely that you deleted this '
             'issue on the server. If this is the\n'
             'case, please run\n\n'
             '    git cl issue 0\n\n'
             'to clear the association with the deleted issue. Then run '
             'this command again.') % issue)
      else:
        DieWithError(
            '\nFailed to fetch issue description. HTTP error %d' % e.code)
    except urllib2.URLError as e:
      print('Warning: Failed to retrieve CL description due to network '
            'failure.', file=sys.stderr)
      return ''

  def GetMostRecentPatchset(self):
    return self.GetIssueProperties()['patchsets'][-1]

  def GetPatchSetDiff(self, issue, patchset):
    return self.RpcServer().get(
        '/download/issue%s_%s.diff' % (issue, patchset))

  def GetIssueProperties(self):
    if self._props is None:
      issue = self.GetIssue()
      if not issue:
        self._props = {}
      else:
        self._props = self.RpcServer().get_issue_properties(issue, True)
    return self._props

  def GetApprovingReviewers(self):
    return get_approving_reviewers(self.GetIssueProperties())

  def AddComment(self, message):
    return self.RpcServer().add_comment(self.GetIssue(), message)

  def GetStatus(self):
    """Apply a rough heuristic to give a simple summary of an issue's review
    or CQ status, assuming adherence to a common workflow.

    Returns None if no issue for this branch, or one of the following keywords:
      * 'error'   - error from review tool (including deleted issues)
      * 'unsent'  - not sent for review
      * 'waiting' - waiting for review
      * 'reply'   - waiting for owner to reply to review
      * 'lgtm'    - LGTM from at least one approved reviewer
      * 'commit'  - in the commit queue
      * 'closed'  - closed
    """
    if not self.GetIssue():
      return None

    try:
      props = self.GetIssueProperties()
    except urllib2.HTTPError:
      return 'error'

    if props.get('closed'):
      # Issue is closed.
      return 'closed'
    if props.get('commit') and not props.get('cq_dry_run', False):
      # Issue is in the commit queue.
      return 'commit'

    try:
      reviewers = self.GetApprovingReviewers()
    except urllib2.HTTPError:
      return 'error'

    if reviewers:
      # Was LGTM'ed.
      return 'lgtm'

    messages = props.get('messages') or []

    # Skip CQ messages that don't require owner's action.
    while messages and messages[-1]['sender'] == COMMIT_BOT_EMAIL:
      if 'Dry run:' in messages[-1]['text']:
        messages.pop()
      elif 'The CQ bit was unchecked' in messages[-1]['text']:
        # This message always follows prior messages from CQ,
        # so skip this too.
        messages.pop()
      else:
        # This is probably a CQ messages warranting user attention.
        break

    if not messages:
      # No message was sent.
      return 'unsent'
    if messages[-1]['sender'] != props.get('owner_email'):
      # Non-LGTM reply from non-owner and not CQ bot.
      return 'reply'
    return 'waiting'

  def UpdateDescriptionRemote(self, description):
    return self.RpcServer().update_description(
        self.GetIssue(), self.description)

  def CloseIssue(self):
    return self.RpcServer().close_issue(self.GetIssue())

  def SetFlag(self, flag, value):
    return self.SetFlags({flag: value})

  def SetFlags(self, flags):
    """Sets flags on this CL/patchset in Rietveld.

    The latest patchset in Rietveld must be the same as latest known locally.
    """
    if not self.GetPatchset():
      DieWithError('The patchset needs to match. Send another patchset.')
    try:
      return self.RpcServer().set_flags(
          self.GetIssue(), self.GetPatchset(), flags)
    except urllib2.HTTPError as e:
      if e.code == 404:
        DieWithError('The issue %s doesn\'t exist.' % self.GetIssue())
      if e.code == 403:
        DieWithError(
            ('Access denied to issue %s. Maybe the patchset %s doesn\'t '
             'match?') % (self.GetIssue(), self.GetPatchset()))
      raise

  def RpcServer(self):
    """Returns an upload.RpcServer() to access this review's rietveld instance.
    """
    if not self._rpc_server:
      self._rpc_server = rietveld.CachingRietveld(
          self.GetCodereviewServer(),
          self._auth_config or auth.make_auth_config())
    return self._rpc_server

  @classmethod
  def IssueSettingSuffix(cls):
    return 'rietveldissue'

  def PatchsetSetting(self):
    """Return the git setting that stores this change's most recent patchset."""
    return 'branch.%s.rietveldpatchset' % self.GetBranch()

  def GetCodereviewServerSetting(self):
    """Returns the git setting that stores this change's rietveld server."""
    branch = self.GetBranch()
    if branch:
      return 'branch.%s.rietveldserver' % branch
    return None

  def _PostUnsetIssueProperties(self):
    """Which branch-specific properties to erase when unsetting issue."""
    return ['rietveldserver']

  def GetRieveldObjForPresubmit(self):
    return self.RpcServer()

  def SetCQState(self, new_state):
    props = self.GetIssueProperties()
    if props.get('private'):
      DieWithError('Cannot set-commit on private issue')

    if new_state == _CQState.COMMIT:
      self.SetFlags({'commit': '1', 'cq_dry_run': '0'})
    elif new_state == _CQState.NONE:
      self.SetFlags({'commit': '0', 'cq_dry_run': '0'})
    else:
      assert new_state == _CQState.DRY_RUN
      self.SetFlags({'commit': '1', 'cq_dry_run': '1'})


  def CMDPatchWithParsedIssue(self, parsed_issue_arg, reject, nocommit,
                              directory):
    # TODO(maruel): Use apply_issue.py

    # PatchIssue should never be called with a dirty tree.  It is up to the
    # caller to check this, but just in case we assert here since the
    # consequences of the caller not checking this could be dire.
    assert(not git_common.is_dirty_git_tree('apply'))
    assert(parsed_issue_arg.valid)
    self._changelist.issue = parsed_issue_arg.issue
    if parsed_issue_arg.hostname:
      self._rietveld_server = 'https://%s' % parsed_issue_arg.hostname

    if (isinstance(parsed_issue_arg, _RietveldParsedIssueNumberArgument) and
        parsed_issue_arg.patch_url):
      assert parsed_issue_arg.patchset
      patchset = parsed_issue_arg.patchset
      patch_data = urllib2.urlopen(parsed_issue_arg.patch_url).read()
    else:
      patchset = parsed_issue_arg.patchset or self.GetMostRecentPatchset()
      patch_data = self.GetPatchSetDiff(self.GetIssue(), patchset)

    # Switch up to the top-level directory, if necessary, in preparation for
    # applying the patch.
    top = settings.GetRelativeRoot()
    if top:
      os.chdir(top)

    # Git patches have a/ at the beginning of source paths.  We strip that out
    # with a sed script rather than the -p flag to patch so we can feed either
    # Git or svn-style patches into the same apply command.
    # re.sub() should be used but flags=re.MULTILINE is only in python 2.7.
    try:
      patch_data = subprocess2.check_output(
          ['sed', '-e', 's|^--- a/|--- |; s|^+++ b/|+++ |'], stdin=patch_data)
    except subprocess2.CalledProcessError:
      DieWithError('Git patch mungling failed.')
    logging.info(patch_data)

    # We use "git apply" to apply the patch instead of "patch" so that we can
    # pick up file adds.
    # The --index flag means: also insert into the index (so we catch adds).
    cmd = ['git', 'apply', '--index', '-p0']
    if directory:
      cmd.extend(('--directory', directory))
    if reject:
      cmd.append('--reject')
    elif IsGitVersionAtLeast('1.7.12'):
      cmd.append('--3way')
    try:
      subprocess2.check_call(cmd, env=GetNoGitPagerEnv(),
                             stdin=patch_data, stdout=subprocess2.VOID)
    except subprocess2.CalledProcessError:
      print('Failed to apply the patch')
      return 1

    # If we had an issue, commit the current state and register the issue.
    if not nocommit:
      RunGit(['commit', '-m', (self.GetDescription() + '\n\n' +
                               'patch from issue %(i)s at patchset '
                               '%(p)s (http://crrev.com/%(i)s#ps%(p)s)'
                               % {'i': self.GetIssue(), 'p': patchset})])
      self.SetIssue(self.GetIssue())
      self.SetPatchset(patchset)
      print('Committed patch locally.')
    else:
      print('Patch applied to index.')
    return 0

  @staticmethod
  def ParseIssueURL(parsed_url):
    if not parsed_url.scheme or not parsed_url.scheme.startswith('http'):
      return None
    # Typical url: https://domain/<issue_number>[/[other]]
    match = re.match('/(\d+)(/.*)?$', parsed_url.path)
    if match:
      return _RietveldParsedIssueNumberArgument(
          issue=int(match.group(1)),
          hostname=parsed_url.netloc)
    # Rietveld patch: https://domain/download/issue<number>_<patchset>.diff
    match = re.match(r'/download/issue(\d+)_(\d+).diff$', parsed_url.path)
    if match:
      return _RietveldParsedIssueNumberArgument(
          issue=int(match.group(1)),
          patchset=int(match.group(2)),
          hostname=parsed_url.netloc,
          patch_url=gclient_utils.UpgradeToHttps(parsed_url.geturl()))
    return None

  def CMDUploadChange(self, options, args, change):
    """Upload the patch to Rietveld."""
    upload_args = ['--assume_yes']  # Don't ask about untracked files.
    upload_args.extend(['--server', self.GetCodereviewServer()])
    upload_args.extend(auth.auth_config_to_command_options(self._auth_config))
    if options.emulate_svn_auto_props:
      upload_args.append('--emulate_svn_auto_props')

    change_desc = None

    if options.email is not None:
      upload_args.extend(['--email', options.email])

    if self.GetIssue():
      if options.title is not None:
        upload_args.extend(['--title', options.title])
      if options.message:
        upload_args.extend(['--message', options.message])
      upload_args.extend(['--issue', str(self.GetIssue())])
      print('This branch is associated with issue %s. '
            'Adding patch to that issue.' % self.GetIssue())
    else:
      if options.title is not None:
        upload_args.extend(['--title', options.title])
      message = (options.title or options.message or
                 CreateDescriptionFromLog(args))
      change_desc = ChangeDescription(message)
      if options.reviewers or options.tbr_owners:
        change_desc.update_reviewers(options.reviewers,
                                     options.tbr_owners,
                                     change)
      if not options.force:
        change_desc.prompt(bug=options.bug)

      if not change_desc.description:
        print('Description is empty; aborting.')
        return 1

      upload_args.extend(['--message', change_desc.description])
      if change_desc.get_reviewers():
        upload_args.append('--reviewers=%s' % ','.join(
            change_desc.get_reviewers()))
      if options.send_mail:
        if not change_desc.get_reviewers():
          DieWithError("Must specify reviewers to send email.")
        upload_args.append('--send_mail')

      # We check this before applying rietveld.private assuming that in
      # rietveld.cc only addresses which we can send private CLs to are listed
      # if rietveld.private is set, and so we should ignore rietveld.cc only
      # when --private is specified explicitly on the command line.
      if options.private:
        logging.warn('rietveld.cc is ignored since private flag is specified.  '
                     'You need to review and add them manually if necessary.')
        cc = self.GetCCListWithoutDefault()
      else:
        cc = self.GetCCList()
      cc = ','.join(filter(None, (cc, ','.join(options.cc))))
      if cc:
        upload_args.extend(['--cc', cc])

    if options.private or settings.GetDefaultPrivateFlag() == "True":
      upload_args.append('--private')

    upload_args.extend(['--git_similarity', str(options.similarity)])
    if not options.find_copies:
      upload_args.extend(['--git_no_find_copies'])

    # Include the upstream repo's URL in the change -- this is useful for
    # projects that have their source spread across multiple repos.
    remote_url = self.GetGitBaseUrlFromConfig()
    if not remote_url:
      if settings.GetIsGitSvn():
        remote_url = self.GetGitSvnRemoteUrl()
      else:
        if self.GetRemoteUrl() and '/' in self.GetUpstreamBranch():
          remote_url = '%s@%s' % (self.GetRemoteUrl(),
                                  self.GetUpstreamBranch().split('/')[-1])
    if remote_url:
      upload_args.extend(['--base_url', remote_url])
      remote, remote_branch = self.GetRemoteBranch()
      target_ref = GetTargetRef(remote, remote_branch, options.target_branch,
                                settings.GetPendingRefPrefix())
      if target_ref:
        upload_args.extend(['--target_ref', target_ref])

      # Look for dependent patchsets. See crbug.com/480453 for more details.
      remote, upstream_branch = self.FetchUpstreamTuple(self.GetBranch())
      upstream_branch = ShortBranchName(upstream_branch)
      if remote is '.':
        # A local branch is being tracked.
        local_branch = upstream_branch
        if settings.GetIsSkipDependencyUpload(local_branch):
          print()
          print('Skipping dependency patchset upload because git config '
                'branch.%s.skip-deps-uploads is set to True.' % local_branch)
          print()
        else:
          auth_config = auth.extract_auth_config_from_options(options)
          branch_cl = Changelist(branchref='refs/heads/'+local_branch,
                                 auth_config=auth_config)
          branch_cl_issue_url = branch_cl.GetIssueURL()
          branch_cl_issue = branch_cl.GetIssue()
          branch_cl_patchset = branch_cl.GetPatchset()
          if branch_cl_issue_url and branch_cl_issue and branch_cl_patchset:
            upload_args.extend(
                ['--depends_on_patchset', '%s:%s' % (
                     branch_cl_issue, branch_cl_patchset)])
            print(
                '\n'
                'The current branch (%s) is tracking a local branch (%s) with '
                'an associated CL.\n'
                'Adding %s/#ps%s as a dependency patchset.\n'
                '\n' % (self.GetBranch(), local_branch, branch_cl_issue_url,
                        branch_cl_patchset))

    project = settings.GetProject()
    if project:
      upload_args.extend(['--project', project])

    try:
      upload_args = ['upload'] + upload_args + args
      logging.info('upload.RealMain(%s)', upload_args)
      issue, patchset = upload.RealMain(upload_args)
      issue = int(issue)
      patchset = int(patchset)
    except KeyboardInterrupt:
      sys.exit(1)
    except:
      # If we got an exception after the user typed a description for their
      # change, back up the description before re-raising.
      if change_desc:
        backup_path = os.path.expanduser(DESCRIPTION_BACKUP_FILE)
        print('\nGot exception while uploading -- saving description to %s\n' %
              backup_path)
        backup_file = open(backup_path, 'w')
        backup_file.write(change_desc.description)
        backup_file.close()
      raise

    if not self.GetIssue():
      self.SetIssue(issue)
    self.SetPatchset(patchset)
    return 0


class _GerritChangelistImpl(_ChangelistCodereviewBase):
  def __init__(self, changelist, auth_config=None):
    # auth_config is Rietveld thing, kept here to preserve interface only.
    super(_GerritChangelistImpl, self).__init__(changelist)
    self._change_id = None
    # Lazily cached values.
    self._gerrit_server = None  # e.g. https://chromium-review.googlesource.com
    self._gerrit_host = None    # e.g. chromium-review.googlesource.com

  def _GetGerritHost(self):
    # Lazy load of configs.
    self.GetCodereviewServer()
    if self._gerrit_host and '.' not in self._gerrit_host:
      # Abbreviated domain like "chromium" instead of chromium.googlesource.com.
      # This happens for internal stuff http://crbug.com/614312.
      parsed = urlparse.urlparse(self.GetRemoteUrl())
      if parsed.scheme == 'sso':
        print('WARNING: using non https URLs for remote is likely broken\n'
              '  Your current remote is: %s'  % self.GetRemoteUrl())
        self._gerrit_host = '%s.googlesource.com' % self._gerrit_host
        self._gerrit_server = 'https://%s' % self._gerrit_host
    return self._gerrit_host

  def _GetGitHost(self):
    """Returns git host to be used when uploading change to Gerrit."""
    return urlparse.urlparse(self.GetRemoteUrl()).netloc

  def GetCodereviewServer(self):
    if not self._gerrit_server:
      # If we're on a branch then get the server potentially associated
      # with that branch.
      if self.GetIssue():
        gerrit_server_setting = self.GetCodereviewServerSetting()
        if gerrit_server_setting:
          self._gerrit_server = RunGit(['config', gerrit_server_setting],
                                       error_ok=True).strip()
          if self._gerrit_server:
            self._gerrit_host = urlparse.urlparse(self._gerrit_server).netloc
      if not self._gerrit_server:
        # We assume repo to be hosted on Gerrit, and hence Gerrit server
        # has "-review" suffix for lowest level subdomain.
        parts = self._GetGitHost().split('.')
        parts[0] = parts[0] + '-review'
        self._gerrit_host = '.'.join(parts)
        self._gerrit_server = 'https://%s' % self._gerrit_host
    return self._gerrit_server

  @classmethod
  def IssueSettingSuffix(cls):
    return 'gerritissue'

  def EnsureAuthenticated(self, force):
    """Best effort check that user is authenticated with Gerrit server."""
    if settings.GetGerritSkipEnsureAuthenticated():
      # For projects with unusual authentication schemes.
      # See http://crbug.com/603378.
      return
    # Lazy-loader to identify Gerrit and Git hosts.
    if gerrit_util.GceAuthenticator.is_gce():
      return
    self.GetCodereviewServer()
    git_host = self._GetGitHost()
    assert self._gerrit_server and self._gerrit_host
    cookie_auth = gerrit_util.CookiesAuthenticator()

    gerrit_auth = cookie_auth.get_auth_header(self._gerrit_host)
    git_auth = cookie_auth.get_auth_header(git_host)
    if gerrit_auth and git_auth:
      if gerrit_auth == git_auth:
        return
      print((
          'WARNING: you have different credentials for Gerrit and git hosts.\n'
          '         Check your %s or %s file for credentials of hosts:\n'
          '           %s\n'
          '           %s\n'
          '         %s') %
          (cookie_auth.get_gitcookies_path(), cookie_auth.get_netrc_path(),
           git_host, self._gerrit_host,
           cookie_auth.get_new_password_message(git_host)))
      if not force:
        ask_for_data('If you know what you are doing, press Enter to continue, '
                     'Ctrl+C to abort.')
      return
    else:
      missing = (
          [] if gerrit_auth else [self._gerrit_host] +
          [] if git_auth else [git_host])
      DieWithError('Credentials for the following hosts are required:\n'
                   '  %s\n'
                   'These are read from %s (or legacy %s)\n'
                   '%s' % (
                     '\n  '.join(missing),
                     cookie_auth.get_gitcookies_path(),
                     cookie_auth.get_netrc_path(),
                     cookie_auth.get_new_password_message(git_host)))


  def PatchsetSetting(self):
    """Return the git setting that stores this change's most recent patchset."""
    return 'branch.%s.gerritpatchset' % self.GetBranch()

  def GetCodereviewServerSetting(self):
    """Returns the git setting that stores this change's Gerrit server."""
    branch = self.GetBranch()
    if branch:
      return 'branch.%s.gerritserver' % branch
    return None

  def _PostUnsetIssueProperties(self):
    """Which branch-specific properties to erase when unsetting issue."""
    return [
        'gerritserver',
        'gerritsquashhash',
    ]

  def GetRieveldObjForPresubmit(self):
    class ThisIsNotRietveldIssue(object):
      def __nonzero__(self):
        # This is a hack to make presubmit_support think that rietveld is not
        # defined, yet still ensure that calls directly result in a decent
        # exception message below.
        return False

      def __getattr__(self, attr):
        print(
            'You aren\'t using Rietveld at the moment, but Gerrit.\n'
            'Using Rietveld in your PRESUBMIT scripts won\'t work.\n'
            'Please, either change your PRESUBIT to not use rietveld_obj.%s,\n'
            'or use Rietveld for codereview.\n'
            'See also http://crbug.com/579160.' % attr)
        raise NotImplementedError()
    return ThisIsNotRietveldIssue()

  def GetGerritObjForPresubmit(self):
    return presubmit_support.GerritAccessor(self._GetGerritHost())

  def GetStatus(self):
    """Apply a rough heuristic to give a simple summary of an issue's review
    or CQ status, assuming adherence to a common workflow.

    Returns None if no issue for this branch, or one of the following keywords:
      * 'error'    - error from review tool (including deleted issues)
      * 'unsent'   - no reviewers added
      * 'waiting'  - waiting for review
      * 'reply'    - waiting for owner to reply to review
      * 'not lgtm' - Code-Review -2 from at least one approved reviewer
      * 'lgtm'     - Code-Review +2 from at least one approved reviewer
      * 'commit'   - in the commit queue
      * 'closed'   - abandoned
    """
    if not self.GetIssue():
      return None

    try:
      data = self._GetChangeDetail(['DETAILED_LABELS', 'CURRENT_REVISION'])
    except httplib.HTTPException:
      return 'error'

    if data['status'] in ('ABANDONED', 'MERGED'):
      return 'closed'

    cq_label = data['labels'].get('Commit-Queue', {})
    if cq_label:
      # Vote value is a stringified integer, which we expect from 0 to 2.
      vote_value = cq_label.get('value', '0')
      vote_text = cq_label.get('values', {}).get(vote_value, '')
      if vote_text.lower() == 'commit':
        return 'commit'

    lgtm_label = data['labels'].get('Code-Review', {})
    if lgtm_label:
      if 'rejected' in lgtm_label:
        return 'not lgtm'
      if 'approved' in lgtm_label:
        return 'lgtm'

    if not data.get('reviewers', {}).get('REVIEWER', []):
      return 'unsent'

    messages = data.get('messages', [])
    if messages:
      owner = data['owner'].get('_account_id')
      last_message_author = messages[-1].get('author', {}).get('_account_id')
      if owner != last_message_author:
        # Some reply from non-owner.
        return 'reply'

    return 'waiting'

  def GetMostRecentPatchset(self):
    data = self._GetChangeDetail(['CURRENT_REVISION'])
    return data['revisions'][data['current_revision']]['_number']

  def FetchDescription(self):
    data = self._GetChangeDetail(['CURRENT_REVISION'])
    current_rev = data['current_revision']
    url = data['revisions'][current_rev]['fetch']['http']['url']
    return gerrit_util.GetChangeDescriptionFromGitiles(url, current_rev)

  def UpdateDescriptionRemote(self, description):
    gerrit_util.SetCommitMessage(self._GetGerritHost(), self.GetIssue(),
                                 description)

  def CloseIssue(self):
    gerrit_util.AbandonChange(self._GetGerritHost(), self.GetIssue(), msg='')

  def GetApprovingReviewers(self):
    """Returns a list of reviewers approving the change.

    Note: not necessarily committers.
    """
    raise NotImplementedError()

  def SubmitIssue(self, wait_for_merge=True):
    gerrit_util.SubmitChange(self._GetGerritHost(), self.GetIssue(),
                             wait_for_merge=wait_for_merge)

  def _GetChangeDetail(self, options=None, issue=None):
    options = options or []
    issue = issue or self.GetIssue()
    assert issue, 'issue required to query Gerrit'
    return gerrit_util.GetChangeDetail(self._GetGerritHost(), str(issue),
                                       options)

  def CMDLand(self, force, bypass_hooks, verbose):
    if git_common.is_dirty_git_tree('land'):
      return 1
    detail = self._GetChangeDetail(['CURRENT_REVISION', 'LABELS'])
    if u'Commit-Queue' in detail.get('labels', {}):
      if not force:
        ask_for_data('\nIt seems this repository has a Commit Queue, '
                     'which can test and land changes for you. '
                     'Are you sure you wish to bypass it?\n'
                     'Press Enter to continue, Ctrl+C to abort.')

    differs = True
    last_upload = RunGit(['config',
                          'branch.%s.gerritsquashhash' % self.GetBranch()],
                         error_ok=True).strip()
    # Note: git diff outputs nothing if there is no diff.
    if not last_upload or RunGit(['diff', last_upload]).strip():
      print('WARNING: some changes from local branch haven\'t been uploaded')
    else:
      if detail['current_revision'] == last_upload:
        differs = False
      else:
        print('WARNING: local branch contents differ from latest uploaded '
              'patchset')
    if differs:
      if not force:
        ask_for_data(
            'Do you want to submit latest Gerrit patchset and bypass hooks?')
      print('WARNING: bypassing hooks and submitting latest uploaded patchset')
    elif not bypass_hooks:
      hook_results = self.RunHook(
          committing=True,
          may_prompt=not force,
          verbose=verbose,
          change=self.GetChange(self.GetCommonAncestorWithUpstream(), None))
      if not hook_results.should_continue():
        return 1

    self.SubmitIssue(wait_for_merge=True)
    print('Issue %s has been submitted.' % self.GetIssueURL())
    return 0

  def CMDPatchWithParsedIssue(self, parsed_issue_arg, reject, nocommit,
                              directory):
    assert not reject
    assert not nocommit
    assert not directory
    assert parsed_issue_arg.valid

    self._changelist.issue = parsed_issue_arg.issue

    if parsed_issue_arg.hostname:
      self._gerrit_host = parsed_issue_arg.hostname
      self._gerrit_server = 'https://%s' % self._gerrit_host

    detail = self._GetChangeDetail(['ALL_REVISIONS'])

    if not parsed_issue_arg.patchset:
      # Use current revision by default.
      revision_info = detail['revisions'][detail['current_revision']]
      patchset = int(revision_info['_number'])
    else:
      patchset = parsed_issue_arg.patchset
      for revision_info in detail['revisions'].itervalues():
        if int(revision_info['_number']) == parsed_issue_arg.patchset:
          break
      else:
        DieWithError('Couldn\'t find patchset %i in issue %i' %
                     (parsed_issue_arg.patchset, self.GetIssue()))

    fetch_info = revision_info['fetch']['http']
    RunGit(['fetch', fetch_info['url'], fetch_info['ref']])
    RunGit(['cherry-pick', 'FETCH_HEAD'])
    self.SetIssue(self.GetIssue())
    self.SetPatchset(patchset)
    print('Committed patch for issue %i pathset %i locally' %
          (self.GetIssue(), self.GetPatchset()))
    return 0

  @staticmethod
  def ParseIssueURL(parsed_url):
    if not parsed_url.scheme or not parsed_url.scheme.startswith('http'):
      return None
    # Gerrit's new UI is https://domain/c/<issue_number>[/[patchset]]
    # But current GWT UI is https://domain/#/c/<issue_number>[/[patchset]]
    # Short urls like https://domain/<issue_number> can be used, but don't allow
    # specifying the patchset (you'd 404), but we allow that here.
    if parsed_url.path == '/':
      part = parsed_url.fragment
    else:
      part = parsed_url.path
    match = re.match('(/c)?/(\d+)(/(\d+)?/?)?$', part)
    if match:
      return _ParsedIssueNumberArgument(
          issue=int(match.group(2)),
          patchset=int(match.group(4)) if match.group(4) else None,
          hostname=parsed_url.netloc)
    return None

  def _GerritCommitMsgHookCheck(self, offer_removal):
    hook = os.path.join(settings.GetRoot(), '.git', 'hooks', 'commit-msg')
    if not os.path.exists(hook):
      return
    # Crude attempt to distinguish Gerrit Codereview hook from potentially
    # custom developer made one.
    data = gclient_utils.FileRead(hook)
    if not('From Gerrit Code Review' in data and 'add_ChangeId()' in data):
      return
    print('Warning: you have Gerrit commit-msg hook installed.\n'
          'It is not neccessary for uploading with git cl in squash mode, '
          'and may interfere with it in subtle ways.\n'
          'We recommend you remove the commit-msg hook.')
    if offer_removal:
      reply = ask_for_data('Do you want to remove it now? [Yes/No]')
      if reply.lower().startswith('y'):
        gclient_utils.rm_file_or_tree(hook)
        print('Gerrit commit-msg hook removed.')
      else:
        print('OK, will keep Gerrit commit-msg hook in place.')

  def CMDUploadChange(self, options, args, change):
    """Upload the current branch to Gerrit."""
    if options.squash and options.no_squash:
      DieWithError('Can only use one of --squash or --no-squash')

    if not options.squash and not options.no_squash:
      # Load default for user, repo, squash=true, in this order.
      options.squash = settings.GetSquashGerritUploads()
    elif options.no_squash:
      options.squash = False

    # We assume the remote called "origin" is the one we want.
    # It is probably not worthwhile to support different workflows.
    gerrit_remote = 'origin'

    remote, remote_branch = self.GetRemoteBranch()
    branch = GetTargetRef(remote, remote_branch, options.target_branch,
                          pending_prefix='')

    if options.squash:
      self._GerritCommitMsgHookCheck(offer_removal=not options.force)
      if self.GetIssue():
        # Try to get the message from a previous upload.
        message = self.GetDescription()
        if not message:
          DieWithError(
              'failed to fetch description from current Gerrit issue %d\n'
              '%s' % (self.GetIssue(), self.GetIssueURL()))
        change_id = self._GetChangeDetail()['change_id']
        while True:
          footer_change_ids = git_footers.get_footer_change_id(message)
          if footer_change_ids == [change_id]:
            break
          if not footer_change_ids:
            message = git_footers.add_footer_change_id(message, change_id)
            print('WARNING: appended missing Change-Id to issue description')
            continue
          # There is already a valid footer but with different or several ids.
          # Doing this automatically is non-trivial as we don't want to lose
          # existing other footers, yet we want to append just 1 desired
          # Change-Id. Thus, just create a new footer, but let user verify the
          # new description.
          message = '%s\n\nChange-Id: %s' % (message, change_id)
          print(
              'WARNING: issue %s has Change-Id footer(s):\n'
              '  %s\n'
              'but issue has Change-Id %s, according to Gerrit.\n'
              'Please, check the proposed correction to the description, '
              'and edit it if necessary but keep the "Change-Id: %s" footer\n'
              % (self.GetIssue(), '\n  '.join(footer_change_ids), change_id,
                 change_id))
          ask_for_data('Press enter to edit now, Ctrl+C to abort')
          if not options.force:
            change_desc = ChangeDescription(message)
            change_desc.prompt(bug=options.bug)
            message = change_desc.description
            if not message:
              DieWithError("Description is empty. Aborting...")
          # Continue the while loop.
        # Sanity check of this code - we should end up with proper message
        # footer.
        assert [change_id] == git_footers.get_footer_change_id(message)
        change_desc = ChangeDescription(message)
      else:
        change_desc = ChangeDescription(
            options.message or CreateDescriptionFromLog(args))
        if not options.force:
          change_desc.prompt(bug=options.bug)
        if not change_desc.description:
          DieWithError("Description is empty. Aborting...")
        message = change_desc.description
        change_ids = git_footers.get_footer_change_id(message)
        if len(change_ids) > 1:
          DieWithError('too many Change-Id footers, at most 1 allowed.')
        if not change_ids:
          # Generate the Change-Id automatically.
          message = git_footers.add_footer_change_id(
              message, GenerateGerritChangeId(message))
          change_desc.set_description(message)
          change_ids = git_footers.get_footer_change_id(message)
          assert len(change_ids) == 1
        change_id = change_ids[0]

      remote, upstream_branch = self.FetchUpstreamTuple(self.GetBranch())
      if remote is '.':
        # If our upstream branch is local, we base our squashed commit on its
        # squashed version.
        upstream_branch_name = scm.GIT.ShortBranchName(upstream_branch)
        # Check the squashed hash of the parent.
        parent = RunGit(['config',
                         'branch.%s.gerritsquashhash' % upstream_branch_name],
                        error_ok=True).strip()
        # Verify that the upstream branch has been uploaded too, otherwise
        # Gerrit will create additional CLs when uploading.
        if not parent or (RunGitSilent(['rev-parse', upstream_branch + ':']) !=
                          RunGitSilent(['rev-parse', parent + ':'])):
          DieWithError(
              'Upload upstream branch %s first.\n'
              'Note: maybe you\'ve uploaded it with --no-squash. '
              'If so, then re-upload it with:\n'
              '    git cl upload --squash\n' % upstream_branch_name)
      else:
        parent = self.GetCommonAncestorWithUpstream()

      tree = RunGit(['rev-parse', 'HEAD:']).strip()
      ref_to_push = RunGit(['commit-tree', tree, '-p', parent,
                            '-m', message]).strip()
    else:
      change_desc = ChangeDescription(
          options.message or CreateDescriptionFromLog(args))
      if not change_desc.description:
        DieWithError("Description is empty. Aborting...")

      if not git_footers.get_footer_change_id(change_desc.description):
        DownloadGerritHook(False)
        change_desc.set_description(self._AddChangeIdToCommitMessage(options,
                                                                     args))
      ref_to_push = 'HEAD'
      parent = '%s/%s' % (gerrit_remote, branch)
      change_id = git_footers.get_footer_change_id(change_desc.description)[0]

    assert change_desc
    commits = RunGitSilent(['rev-list', '%s..%s' % (parent,
                                                    ref_to_push)]).splitlines()
    if len(commits) > 1:
      print('WARNING: This will upload %d commits. Run the following command '
            'to see which commits will be uploaded: ' % len(commits))
      print('git log %s..%s' % (parent, ref_to_push))
      print('You can also use `git squash-branch` to squash these into a '
            'single commit.')
      ask_for_data('About to upload; enter to confirm.')

    if options.reviewers or options.tbr_owners:
      change_desc.update_reviewers(options.reviewers, options.tbr_owners,
                                   change)

    # Extra options that can be specified at push time. Doc:
    # https://gerrit-review.googlesource.com/Documentation/user-upload.html
    refspec_opts = []
    if options.title:
      # Per doc, spaces must be converted to underscores, and Gerrit will do the
      # reverse on its side.
      if '_' in options.title:
        print('WARNING: underscores in title will be converted to spaces.')
      refspec_opts.append('m=' + options.title.replace(' ', '_'))

    if options.send_mail:
      if not change_desc.get_reviewers():
        DieWithError('Must specify reviewers to send email.')
      refspec_opts.append('notify=ALL')
    else:
      refspec_opts.append('notify=NONE')

    cc = self.GetCCList().split(',')
    if options.cc:
      cc.extend(options.cc)
    cc = filter(None, cc)
    if cc:
      refspec_opts.extend('cc=' + email.strip() for email in cc)

    if change_desc.get_reviewers():
      refspec_opts.extend('r=' + email.strip()
                          for email in change_desc.get_reviewers())

    refspec_suffix = ''
    if refspec_opts:
      refspec_suffix = '%' + ','.join(refspec_opts)
      assert ' ' not in refspec_suffix, (
          'spaces not allowed in refspec: "%s"' % refspec_suffix)
    refspec = '%s:refs/for/%s%s' % (ref_to_push, branch, refspec_suffix)

    push_stdout = gclient_utils.CheckCallAndFilter(
        ['git', 'push', gerrit_remote, refspec],
        print_stdout=True,
        # Flush after every line: useful for seeing progress when running as
        # recipe.
        filter_fn=lambda _: sys.stdout.flush())

    if options.squash:
      regex = re.compile(r'remote:\s+https?://[\w\-\.\/]*/(\d+)\s.*')
      change_numbers = [m.group(1)
                        for m in map(regex.match, push_stdout.splitlines())
                        if m]
      if len(change_numbers) != 1:
        DieWithError(
          ('Created|Updated %d issues on Gerrit, but only 1 expected.\n'
           'Change-Id: %s') % (len(change_numbers), change_id))
      self.SetIssue(change_numbers[0])
      RunGit(['config', 'branch.%s.gerritsquashhash' % self.GetBranch(),
              ref_to_push])
    return 0

  def _AddChangeIdToCommitMessage(self, options, args):
    """Re-commits using the current message, assumes the commit hook is in
    place.
    """
    log_desc = options.message or CreateDescriptionFromLog(args)
    git_command = ['commit', '--amend', '-m', log_desc]
    RunGit(git_command)
    new_log_desc = CreateDescriptionFromLog(args)
    if git_footers.get_footer_change_id(new_log_desc):
      print('git-cl: Added Change-Id to commit message.')
      return new_log_desc
    else:
      DieWithError('ERROR: Gerrit commit-msg hook not installed.')

  def SetCQState(self, new_state):
    """Sets the Commit-Queue label assuming canonical CQ config for Gerrit."""
    vote_map = {
        _CQState.NONE:    0,
        _CQState.DRY_RUN: 1,
        _CQState.COMMIT : 2,
    }
    gerrit_util.SetReview(self._GetGerritHost(), self.GetIssue(),
                          labels={'Commit-Queue': vote_map[new_state]})


_CODEREVIEW_IMPLEMENTATIONS = {
  'rietveld': _RietveldChangelistImpl,
  'gerrit': _GerritChangelistImpl,
}


def _add_codereview_select_options(parser):
  """Appends --gerrit and --rietveld options to force specific codereview."""
  parser.codereview_group = optparse.OptionGroup(
      parser, 'EXPERIMENTAL! Codereview override options')
  parser.add_option_group(parser.codereview_group)
  parser.codereview_group.add_option(
      '--gerrit', action='store_true',
      help='Force the use of Gerrit for codereview')
  parser.codereview_group.add_option(
      '--rietveld', action='store_true',
      help='Force the use of Rietveld for codereview')


def _process_codereview_select_options(parser, options):
  if options.gerrit and options.rietveld:
    parser.error('Options --gerrit and --rietveld are mutually exclusive')
  options.forced_codereview = None
  if options.gerrit:
    options.forced_codereview = 'gerrit'
  elif options.rietveld:
    options.forced_codereview = 'rietveld'


def _get_bug_line_values(default_project, bugs):
  """Given default_project and comma separated list of bugs, yields bug line
  values.

  Each bug can be either:
    * a number, which is combined with default_project
    * string, which is left as is.

  This function may produce more than one line, because bugdroid expects one
  project per line.

  >>> list(_get_bug_line_values('v8', '123,chromium:789'))
      ['v8:123', 'chromium:789']
  """
  default_bugs = []
  others = []
  for bug in bugs.split(','):
    bug = bug.strip()
    if bug:
      try:
        default_bugs.append(int(bug))
      except ValueError:
        others.append(bug)

  if default_bugs:
    default_bugs = ','.join(map(str, default_bugs))
    if default_project:
      yield '%s:%s' % (default_project, default_bugs)
    else:
      yield default_bugs
  for other in sorted(others):
    # Don't bother finding common prefixes, CLs with >2 bugs are very very rare.
    yield other


class ChangeDescription(object):
  """Contains a parsed form of the change description."""
  R_LINE = r'^[ \t]*(TBR|R)[ \t]*=[ \t]*(.*?)[ \t]*$'
  BUG_LINE = r'^[ \t]*(BUG)[ \t]*=[ \t]*(.*?)[ \t]*$'

  def __init__(self, description):
    self._description_lines = (description or '').strip().splitlines()

  @property               # www.logilab.org/ticket/89786
  def description(self):  # pylint: disable=E0202
    return '\n'.join(self._description_lines)

  def set_description(self, desc):
    if isinstance(desc, basestring):
      lines = desc.splitlines()
    else:
      lines = [line.rstrip() for line in desc]
    while lines and not lines[0]:
      lines.pop(0)
    while lines and not lines[-1]:
      lines.pop(-1)
    self._description_lines = lines

  def update_reviewers(self, reviewers, add_owners_tbr=False, change=None):
    """Rewrites the R=/TBR= line(s) as a single line each."""
    assert isinstance(reviewers, list), reviewers
    if not reviewers and not add_owners_tbr:
      return
    reviewers = reviewers[:]

    # Get the set of R= and TBR= lines and remove them from the desciption.
    regexp = re.compile(self.R_LINE)
    matches = [regexp.match(line) for line in self._description_lines]
    new_desc = [l for i, l in enumerate(self._description_lines)
                if not matches[i]]
    self.set_description(new_desc)

    # Construct new unified R= and TBR= lines.
    r_names = []
    tbr_names = []
    for match in matches:
      if not match:
        continue
      people = cleanup_list([match.group(2).strip()])
      if match.group(1) == 'TBR':
        tbr_names.extend(people)
      else:
        r_names.extend(people)
    for name in r_names:
      if name not in reviewers:
        reviewers.append(name)
    if add_owners_tbr:
      owners_db = owners.Database(change.RepositoryRoot(),
        fopen=file, os_path=os.path)
      all_reviewers = set(tbr_names + reviewers)
      missing_files = owners_db.files_not_covered_by(change.LocalPaths(),
                                                     all_reviewers)
      tbr_names.extend(owners_db.reviewers_for(missing_files,
                                               change.author_email))
    new_r_line = 'R=' + ', '.join(reviewers) if reviewers else None
    new_tbr_line = 'TBR=' + ', '.join(tbr_names) if tbr_names else None

    # Put the new lines in the description where the old first R= line was.
    line_loc = next((i for i, match in enumerate(matches) if match), -1)
    if 0 <= line_loc < len(self._description_lines):
      if new_tbr_line:
        self._description_lines.insert(line_loc, new_tbr_line)
      if new_r_line:
        self._description_lines.insert(line_loc, new_r_line)
    else:
      if new_r_line:
        self.append_footer(new_r_line)
      if new_tbr_line:
        self.append_footer(new_tbr_line)

  def prompt(self, bug=None):
    """Asks the user to update the description."""
    self.set_description([
      '# Enter a description of the change.',
      '# This will be displayed on the codereview site.',
      '# The first line will also be used as the subject of the review.',
      '#--------------------This line is 72 characters long'
      '--------------------',
    ] + self._description_lines)

    regexp = re.compile(self.BUG_LINE)
    if not any((regexp.match(line) for line in self._description_lines)):
      prefix = settings.GetBugPrefix()
      values = list(_get_bug_line_values(prefix, bug or '')) or [prefix]
      for value in values:
        # TODO(tandrii): change this to 'Bug: xxx' to be a proper Gerrit footer.
        self.append_footer('BUG=%s' % value)

    content = gclient_utils.RunEditor(self.description, True,
                                      git_editor=settings.GetGitEditor())
    if not content:
      DieWithError('Running editor failed')
    lines = content.splitlines()

    # Strip off comments.
    clean_lines = [line.rstrip() for line in lines if not line.startswith('#')]
    if not clean_lines:
      DieWithError('No CL description, aborting')
    self.set_description(clean_lines)

  def append_footer(self, line):
    """Adds a footer line to the description.

    Differentiates legacy "KEY=xxx" footers (used to be called tags) and
    Gerrit's footers in the form of "Footer-Key: footer any value" and ensures
    that Gerrit footers are always at the end.
    """
    parsed_footer_line = git_footers.parse_footer(line)
    if parsed_footer_line:
      # Line is a gerrit footer in the form: Footer-Key: any value.
      # Thus, must be appended observing Gerrit footer rules.
      self.set_description(
          git_footers.add_footer(self.description,
                                 key=parsed_footer_line[0],
                                 value=parsed_footer_line[1]))
      return

    if not self._description_lines:
      self._description_lines.append(line)
      return

    top_lines, gerrit_footers, _ = git_footers.split_footers(self.description)
    if gerrit_footers:
      # git_footers.split_footers ensures that there is an empty line before
      # actual (gerrit) footers, if any. We have to keep it that way.
      assert top_lines and top_lines[-1] == ''
      top_lines, separator = top_lines[:-1], top_lines[-1:]
    else:
      separator = []  # No need for separator if there are no gerrit_footers.

    prev_line = top_lines[-1] if top_lines else ''
    if (not presubmit_support.Change.TAG_LINE_RE.match(prev_line) or
        not presubmit_support.Change.TAG_LINE_RE.match(line)):
      top_lines.append('')
    top_lines.append(line)
    self._description_lines = top_lines + separator + gerrit_footers

  def get_reviewers(self):
    """Retrieves the list of reviewers."""
    matches = [re.match(self.R_LINE, line) for line in self._description_lines]
    reviewers = [match.group(2).strip() for match in matches if match]
    return cleanup_list(reviewers)


def get_approving_reviewers(props):
  """Retrieves the reviewers that approved a CL from the issue properties with
  messages.

  Note that the list may contain reviewers that are not committer, thus are not
  considered by the CQ.
  """
  return sorted(
      set(
        message['sender']
        for message in props['messages']
        if message['approval'] and message['sender'] in props['reviewers']
      )
  )


def FindCodereviewSettingsFile(filename='codereview.settings'):
  """Finds the given file starting in the cwd and going up.

  Only looks up to the top of the repository unless an
  'inherit-review-settings-ok' file exists in the root of the repository.
  """
  inherit_ok_file = 'inherit-review-settings-ok'
  cwd = os.getcwd()
  root = settings.GetRoot()
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
  keyvals = gclient_utils.ParseCodereviewSettingsContent(fileobj.read())

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
  SetProperty('private', 'PRIVATE', unset_error_ok=True)
  SetProperty('tree-status-url', 'STATUS', unset_error_ok=True)
  SetProperty('viewvc-url', 'VIEW_VC', unset_error_ok=True)
  SetProperty('bug-prefix', 'BUG_PREFIX', unset_error_ok=True)
  SetProperty('cpplint-regex', 'LINT_REGEX', unset_error_ok=True)
  SetProperty('force-https-commit-url', 'FORCE_HTTPS_COMMIT_URL',
              unset_error_ok=True)
  SetProperty('cpplint-ignore-regex', 'LINT_IGNORE_REGEX', unset_error_ok=True)
  SetProperty('project', 'PROJECT', unset_error_ok=True)
  SetProperty('pending-ref-prefix', 'PENDING_REF_PREFIX', unset_error_ok=True)
  SetProperty('run-post-upload-hook', 'RUN_POST_UPLOAD_HOOK',
              unset_error_ok=True)

  if 'GERRIT_HOST' in keyvals:
    RunGit(['config', 'gerrit.host', keyvals['GERRIT_HOST']])

  if 'GERRIT_SQUASH_UPLOADS' in keyvals:
    RunGit(['config', 'gerrit.squash-uploads',
            keyvals['GERRIT_SQUASH_UPLOADS']])

  if 'GERRIT_SKIP_ENSURE_AUTHENTICATED' in keyvals:
    RunGit(['config', 'gerrit.skip-ensure-authenticated',
            keyvals['GERRIT_SKIP_ENSURE_AUTHENTICATED']])

  if 'PUSH_URL_CONFIG' in keyvals and 'ORIGIN_URL_CONFIG' in keyvals:
    #should be of the form
    #PUSH_URL_CONFIG: url.ssh://gitrw.chromium.org.pushinsteadof
    #ORIGIN_URL_CONFIG: http://src.chromium.org/git
    RunGit(['config', keyvals['PUSH_URL_CONFIG'],
            keyvals['ORIGIN_URL_CONFIG']])


def urlretrieve(source, destination):
  """urllib is broken for SSL connections via a proxy therefore we
  can't use urllib.urlretrieve()."""
  with open(destination, 'w') as f:
    f.write(urllib2.urlopen(source).read())


def hasSheBang(fname):
  """Checks fname is a #! script."""
  with open(fname) as f:
    return f.read(2).startswith('#!')


# TODO(bpastene) Remove once a cleaner fix to crbug.com/600473 presents itself.
def DownloadHooks(*args, **kwargs):
  pass


def DownloadGerritHook(force):
  """Download and install Gerrit commit-msg hook.

  Args:
    force: True to update hooks. False to install hooks if not present.
  """
  if not settings.GetIsGerrit():
    return
  src = 'https://gerrit-review.googlesource.com/tools/hooks/commit-msg'
  dst = os.path.join(settings.GetRoot(), '.git', 'hooks', 'commit-msg')
  if not os.access(dst, os.X_OK):
    if os.path.exists(dst):
      if not force:
        return
    try:
      urlretrieve(src, dst)
      if not hasSheBang(dst):
        DieWithError('Not a script: %s\n'
                     'You need to download from\n%s\n'
                     'into .git/hooks/commit-msg and '
                     'chmod +x .git/hooks/commit-msg' % (dst, src))
      os.chmod(dst, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
    except Exception:
      if os.path.exists(dst):
        os.remove(dst)
      DieWithError('\nFailed to download hooks.\n'
                   'You need to download from\n%s\n'
                   'into .git/hooks/commit-msg and '
                   'chmod +x .git/hooks/commit-msg' % src)



def GetRietveldCodereviewSettingsInteractively():
  """Prompt the user for settings."""
  server = settings.GetDefaultServerUrl(error_ok=True)
  prompt = 'Rietveld server (host[:port])'
  prompt += ' [%s]' % (server or DEFAULT_SERVER)
  newserver = ask_for_data(prompt + ':')
  if not server and not newserver:
    newserver = DEFAULT_SERVER
  if newserver:
    newserver = gclient_utils.UpgradeToHttps(newserver)
    if newserver != server:
      RunGit(['config', 'rietveld.server', newserver])

  def SetProperty(initial, caption, name, is_url):
    prompt = caption
    if initial:
      prompt += ' ("x" to clear) [%s]' % initial
    new_val = ask_for_data(prompt + ':')
    if new_val == 'x':
      RunGit(['config', '--unset-all', 'rietveld.' + name], error_ok=True)
    elif new_val:
      if is_url:
        new_val = gclient_utils.UpgradeToHttps(new_val)
      if new_val != initial:
        RunGit(['config', 'rietveld.' + name, new_val])

  SetProperty(settings.GetDefaultCCList(), 'CC list', 'cc', False)
  SetProperty(settings.GetDefaultPrivateFlag(),
              'Private flag (rietveld only)', 'private', False)
  SetProperty(settings.GetTreeStatusUrl(error_ok=True), 'Tree status URL',
              'tree-status-url', False)
  SetProperty(settings.GetViewVCUrl(), 'ViewVC URL', 'viewvc-url', True)
  SetProperty(settings.GetBugPrefix(), 'Bug Prefix', 'bug-prefix', False)
  SetProperty(settings.GetRunPostUploadHook(), 'Run Post Upload Hook',
              'run-post-upload-hook', False)

@subcommand.usage('[repo root containing codereview.settings]')
def CMDconfig(parser, args):
  """Edits configuration for this tree."""

  print('WARNING: git cl config works for Rietveld only.\n'
        'For Gerrit, see http://crbug.com/603116.')
  # TODO(tandrii): add Gerrit support as part of http://crbug.com/603116.
  parser.add_option('--activate-update', action='store_true',
                    help='activate auto-updating [rietveld] section in '
                         '.git/config')
  parser.add_option('--deactivate-update', action='store_true',
                    help='deactivate auto-updating [rietveld] section in '
                         '.git/config')
  options, args = parser.parse_args(args)

  if options.deactivate_update:
    RunGit(['config', 'rietveld.autoupdate', 'false'])
    return

  if options.activate_update:
    RunGit(['config', '--unset', 'rietveld.autoupdate'])
    return

  if len(args) == 0:
    GetRietveldCodereviewSettingsInteractively()
    return 0

  url = args[0]
  if not url.endswith('codereview.settings'):
    url = os.path.join(url, 'codereview.settings')

  # Load code review settings and download hooks (if available).
  LoadCodereviewSettingsFromFile(urllib2.urlopen(url))
  return 0


def CMDbaseurl(parser, args):
  """Gets or sets base-url for this branch."""
  branchref = RunGit(['symbolic-ref', 'HEAD']).strip()
  branch = ShortBranchName(branchref)
  _, args = parser.parse_args(args)
  if not args:
    print('Current base-url:')
    return RunGit(['config', 'branch.%s.base-url' % branch],
                  error_ok=False).strip()
  else:
    print('Setting base-url to %s' % args[0])
    return RunGit(['config', 'branch.%s.base-url' % branch, args[0]],
                  error_ok=False).strip()


def color_for_status(status):
  """Maps a Changelist status to color, for CMDstatus and other tools."""
  return {
    'unsent': Fore.RED,
    'waiting': Fore.BLUE,
    'reply': Fore.YELLOW,
    'lgtm': Fore.GREEN,
    'commit': Fore.MAGENTA,
    'closed': Fore.CYAN,
    'error': Fore.WHITE,
  }.get(status, Fore.WHITE)


def get_cl_statuses(changes, fine_grained, max_processes=None):
  """Returns a blocking iterable of (cl, status) for given branches.

  If fine_grained is true, this will fetch CL statuses from the server.
  Otherwise, simply indicate if there's a matching url for the given branches.

  If max_processes is specified, it is used as the maximum number of processes
  to spawn to fetch CL status from the server. Otherwise 1 process per branch is
  spawned.

  See GetStatus() for a list of possible statuses.
  """
  # Silence upload.py otherwise it becomes unwieldly.
  upload.verbosity = 0

  if fine_grained:
    # Process one branch synchronously to work through authentication, then
    # spawn processes to process all the other branches in parallel.
    if changes:
      fetch = lambda cl: (cl, cl.GetStatus())
      yield fetch(changes[0])

      if not changes:
        # Exit early if there was only one branch to fetch.
        return

      changes_to_fetch = changes[1:]
      pool = ThreadPool(
          min(max_processes, len(changes_to_fetch))
              if max_processes is not None
              else len(changes_to_fetch))

      fetched_cls = set()
      it = pool.imap_unordered(fetch, changes_to_fetch).__iter__()
      while True:
        try:
          row = it.next(timeout=5)
        except multiprocessing.TimeoutError:
          break

        fetched_cls.add(row[0])
        yield row

      # Add any branches that failed to fetch.
      for cl in set(changes_to_fetch) - fetched_cls:
        yield (cl, 'error')

  else:
    # Do not use GetApprovingReviewers(), since it requires an HTTP request.
    for cl in changes:
      yield (cl, 'waiting' if cl.GetIssueURL() else 'error')


def upload_branch_deps(cl, args):
  """Uploads CLs of local branches that are dependents of the current branch.

  If the local branch dependency tree looks like:
  test1 -> test2.1 -> test3.1
                   -> test3.2
        -> test2.2 -> test3.3

  and you run "git cl upload --dependencies" from test1 then "git cl upload" is
  run on the dependent branches in this order:
  test2.1, test3.1, test3.2, test2.2, test3.3

  Note: This function does not rebase your local dependent branches. Use it when
        you make a change to the parent branch that will not conflict with its
        dependent branches, and you would like their dependencies updated in
        Rietveld.
  """
  if git_common.is_dirty_git_tree('upload-branch-deps'):
    return 1

  root_branch = cl.GetBranch()
  if root_branch is None:
    DieWithError('Can\'t find dependent branches from detached HEAD state. '
                 'Get on a branch!')
  if not cl.GetIssue() or not cl.GetPatchset():
    DieWithError('Current branch does not have an uploaded CL. We cannot set '
                 'patchset dependencies without an uploaded CL.')

  branches = RunGit(['for-each-ref',
                     '--format=%(refname:short) %(upstream:short)',
                     'refs/heads'])
  if not branches:
    print('No local branches found.')
    return 0

  # Create a dictionary of all local branches to the branches that are dependent
  # on it.
  tracked_to_dependents = collections.defaultdict(list)
  for b in branches.splitlines():
    tokens = b.split()
    if len(tokens) == 2:
      branch_name, tracked = tokens
      tracked_to_dependents[tracked].append(branch_name)

  print()
  print('The dependent local branches of %s are:' % root_branch)
  dependents = []
  def traverse_dependents_preorder(branch, padding=''):
    dependents_to_process = tracked_to_dependents.get(branch, [])
    padding += '  '
    for dependent in dependents_to_process:
      print('%s%s' % (padding, dependent))
      dependents.append(dependent)
      traverse_dependents_preorder(dependent, padding)
  traverse_dependents_preorder(root_branch)
  print()

  if not dependents:
    print('There are no dependent local branches for %s' % root_branch)
    return 0

  print('This command will checkout all dependent branches and run '
        '"git cl upload".')
  ask_for_data('[Press enter to continue or ctrl-C to quit]')

  # Add a default patchset title to all upload calls in Rietveld.
  if not cl.IsGerrit():
    args.extend(['-t', 'Updated patchset dependency'])

  # Record all dependents that failed to upload.
  failures = {}
  # Go through all dependents, checkout the branch and upload.
  try:
    for dependent_branch in dependents:
      print()
      print('--------------------------------------')
      print('Running "git cl upload" from %s:' % dependent_branch)
      RunGit(['checkout', '-q', dependent_branch])
      print()
      try:
        if CMDupload(OptionParser(), args) != 0:
          print('Upload failed for %s!' % dependent_branch)
          failures[dependent_branch] = 1
      except:  # pylint: disable=W0702
        failures[dependent_branch] = 1
      print()
  finally:
    # Swap back to the original root branch.
    RunGit(['checkout', '-q', root_branch])

  print()
  print('Upload complete for dependent branches!')
  for dependent_branch in dependents:
    upload_status = 'failed' if failures.get(dependent_branch) else 'succeeded'
    print('  %s : %s' % (dependent_branch, upload_status))
  print()

  return 0


def CMDarchive(parser, args):
  """Archives and deletes branches associated with closed changelists."""
  parser.add_option(
      '-j', '--maxjobs', action='store', type=int,
      help='The maximum number of jobs to use when retrieving review status')
  parser.add_option(
      '-f', '--force', action='store_true',
      help='Bypasses the confirmation prompt.')

  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported args: %s' % ' '.join(args))
  auth_config = auth.extract_auth_config_from_options(options)

  branches = RunGit(['for-each-ref', '--format=%(refname)', 'refs/heads'])
  if not branches:
    return 0

  print('Finding all branches associated with closed issues...')
  changes = [Changelist(branchref=b, auth_config=auth_config)
              for b in branches.splitlines()]
  alignment = max(5, max(len(c.GetBranch()) for c in changes))
  statuses = get_cl_statuses(changes,
                             fine_grained=True,
                             max_processes=options.maxjobs)
  proposal = [(cl.GetBranch(),
               'git-cl-archived-%s-%s' % (cl.GetIssue(), cl.GetBranch()))
              for cl, status in statuses
              if status == 'closed']
  proposal.sort()

  if not proposal:
    print('No branches with closed codereview issues found.')
    return 0

  current_branch = GetCurrentBranch()

  print('\nBranches with closed issues that will be archived:\n')
  print('%*s | %s' % (alignment, 'Branch name', 'Archival tag name'))
  for next_item in proposal:
    print('%*s   %s' % (alignment, next_item[0], next_item[1]))

  if any(branch == current_branch for branch, _ in proposal):
    print('You are currently on a branch \'%s\' which is associated with a '
          'closed codereview issue, so archive cannot proceed. Please '
          'checkout another branch and run this command again.' %
          current_branch)
    return 1

  if not options.force:
    answer = ask_for_data('\nProceed with deletion (Y/n)? ').lower()
    if answer not in ('y', ''):
      print('Aborted.')
      return 1

  for branch, tagname in proposal:
    RunGit(['tag', tagname, branch])
    RunGit(['branch', '-D', branch])
  print('\nJob\'s done!')

  return 0


def CMDstatus(parser, args):
  """Show status of changelists.

  Colors are used to tell the state of the CL unless --fast is used:
    - Red      not sent for review or broken
    - Blue     waiting for review
    - Yellow   waiting for you to reply to review
    - Green    LGTM'ed
    - Magenta  in the commit queue
    - Cyan     was committed, branch can be deleted

  Also see 'git cl comments'.
  """
  parser.add_option('--field',
                    help='print only specific field (desc|id|patch|url)')
  parser.add_option('-f', '--fast', action='store_true',
                    help='Do not retrieve review status')
  parser.add_option(
      '-j', '--maxjobs', action='store', type=int,
      help='The maximum number of jobs to use when retrieving review status')

  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported args: %s' % args)
  auth_config = auth.extract_auth_config_from_options(options)

  if options.field:
    cl = Changelist(auth_config=auth_config)
    if options.field.startswith('desc'):
      print(cl.GetDescription())
    elif options.field == 'id':
      issueid = cl.GetIssue()
      if issueid:
        print(issueid)
    elif options.field == 'patch':
      patchset = cl.GetPatchset()
      if patchset:
        print(patchset)
    elif options.field == 'url':
      url = cl.GetIssueURL()
      if url:
        print(url)
    return 0

  branches = RunGit(['for-each-ref', '--format=%(refname)', 'refs/heads'])
  if not branches:
    print('No local branch found.')
    return 0

  changes = [
      Changelist(branchref=b, auth_config=auth_config)
      for b in branches.splitlines()]
  print('Branches associated with reviews:')
  output = get_cl_statuses(changes,
                           fine_grained=not options.fast,
                           max_processes=options.maxjobs)

  branch_statuses = {}
  alignment = max(5, max(len(ShortBranchName(c.GetBranch())) for c in changes))
  for cl in sorted(changes, key=lambda c: c.GetBranch()):
    branch = cl.GetBranch()
    while branch not in branch_statuses:
      c, status = output.next()
      branch_statuses[c.GetBranch()] = status
    status = branch_statuses.pop(branch)
    url = cl.GetIssueURL()
    if url and (not status or status == 'error'):
      # The issue probably doesn't exist anymore.
      url += ' (broken)'

    color = color_for_status(status)
    reset = Fore.RESET
    if not setup_color.IS_TTY:
      color = ''
      reset = ''
    status_str = '(%s)' % status if status else ''
    print('  %*s : %s%s %s%s' % (
          alignment, ShortBranchName(branch), color, url,
          status_str, reset))

  cl = Changelist(auth_config=auth_config)
  print()
  print('Current branch:',)
  print(cl.GetBranch())
  if not cl.GetIssue():
    print('No issue assigned.')
    return 0
  print('Issue number: %s (%s)' % (cl.GetIssue(), cl.GetIssueURL()))
  if not options.fast:
    print('Issue description:')
    print(cl.GetDescription(pretty=True))
  return 0


def colorize_CMDstatus_doc():
  """To be called once in main() to add colors to git cl status help."""
  colors = [i for i in dir(Fore) if i[0].isupper()]

  def colorize_line(line):
    for color in colors:
      if color in line.upper():
        # Extract whitespaces first and the leading '-'.
        indent = len(line) - len(line.lstrip(' ')) + 1
        return line[:indent] + getattr(Fore, color) + line[indent:] + Fore.RESET
    return line

  lines = CMDstatus.__doc__.splitlines()
  CMDstatus.__doc__ = '\n'.join(colorize_line(l) for l in lines)


@subcommand.usage('[issue_number]')
def CMDissue(parser, args):
  """Sets or displays the current code review issue number.

  Pass issue number 0 to clear the current issue.
  """
  parser.add_option('-r', '--reverse', action='store_true',
                    help='Lookup the branch(es) for the specified issues. If '
                         'no issues are specified, all branches with mapped '
                         'issues will be listed.')
  _add_codereview_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  if options.reverse:
    branches = RunGit(['for-each-ref', 'refs/heads',
                       '--format=%(refname:short)']).splitlines()

    # Reverse issue lookup.
    issue_branch_map = {}
    for branch in branches:
      cl = Changelist(branchref=branch)
      issue_branch_map.setdefault(cl.GetIssue(), []).append(branch)
    if not args:
      args = sorted(issue_branch_map.iterkeys())
    for issue in args:
      if not issue:
        continue
      print('Branch for issue number %s: %s' % (
          issue, ', '.join(issue_branch_map.get(int(issue)) or ('None',))))
  else:
    cl = Changelist(codereview=options.forced_codereview)
    if len(args) > 0:
      try:
        issue = int(args[0])
      except ValueError:
        DieWithError('Pass a number to set the issue or none to list it.\n'
                     'Maybe you want to run git cl status?')
      cl.SetIssue(issue)
    print('Issue number: %s (%s)' % (cl.GetIssue(), cl.GetIssueURL()))
  return 0


def CMDcomments(parser, args):
  """Shows or posts review comments for any changelist."""
  parser.add_option('-a', '--add-comment', dest='comment',
                    help='comment to add to an issue')
  parser.add_option('-i', dest='issue',
                    help="review issue id (defaults to current issue)")
  parser.add_option('-j', '--json-file',
                    help='File to write JSON summary to')
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)

  issue = None
  if options.issue:
    try:
      issue = int(options.issue)
    except ValueError:
      DieWithError('A review issue id is expected to be a number')

  cl = Changelist(issue=issue, codereview='rietveld', auth_config=auth_config)

  if options.comment:
    cl.AddComment(options.comment)
    return 0

  data = cl.GetIssueProperties()
  summary = []
  for message in sorted(data.get('messages', []), key=lambda x: x['date']):
    summary.append({
        'date': message['date'],
        'lgtm': False,
        'message': message['text'],
        'not_lgtm': False,
        'sender': message['sender'],
    })
    if message['disapproval']:
      color = Fore.RED
      summary[-1]['not lgtm'] = True
    elif message['approval']:
      color = Fore.GREEN
      summary[-1]['lgtm'] = True
    elif message['sender'] == data['owner_email']:
      color = Fore.MAGENTA
    else:
      color = Fore.BLUE
    print('\n%s%s  %s%s' % (
        color, message['date'].split('.', 1)[0], message['sender'],
        Fore.RESET))
    if message['text'].strip():
      print('\n'.join('  ' + l for l in message['text'].splitlines()))
  if options.json_file:
    with open(options.json_file, 'wb') as f:
      json.dump(summary, f)
  return 0


@subcommand.usage('[codereview url or issue id]')
def CMDdescription(parser, args):
  """Brings up the editor for the current CL's description."""
  parser.add_option('-d', '--display', action='store_true',
                    help='Display the description instead of opening an editor')
  parser.add_option('-n', '--new-description',
                    help='New description to set for this issue (- for stdin)')

  _add_codereview_select_options(parser)
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  target_issue = None
  if len(args) > 0:
    target_issue = ParseIssueNumberArgument(args[0])
    if not target_issue.valid:
      parser.print_help()
      return 1

  auth_config = auth.extract_auth_config_from_options(options)

  kwargs = {
      'auth_config': auth_config,
      'codereview': options.forced_codereview,
  }
  if target_issue:
    kwargs['issue'] = target_issue.issue
    if options.forced_codereview == 'rietveld':
      kwargs['rietveld_server'] = target_issue.hostname

  cl = Changelist(**kwargs)

  if not cl.GetIssue():
    DieWithError('This branch has no associated changelist.')
  description = ChangeDescription(cl.GetDescription())

  if options.display:
    print(description.description)
    return 0

  if options.new_description:
    text = options.new_description
    if text == '-':
      text = '\n'.join(l.rstrip() for l in sys.stdin)

    description.set_description(text)
  else:
    description.prompt()

  if cl.GetDescription() != description.description:
    cl.UpdateDescription(description.description)
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


def CMDlint(parser, args):
  """Runs cpplint on the current changelist."""
  parser.add_option('--filter', action='append', metavar='-x,+y',
                    help='Comma-separated list of cpplint\'s category-filters')
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)

  # Access to a protected member _XX of a client class
  # pylint: disable=W0212
  try:
    import cpplint
    import cpplint_chromium
  except ImportError:
    print('Your depot_tools is missing cpplint.py and/or cpplint_chromium.py.')
    return 1

  # Change the current working directory before calling lint so that it
  # shows the correct base.
  previous_cwd = os.getcwd()
  os.chdir(settings.GetRoot())
  try:
    cl = Changelist(auth_config=auth_config)
    change = cl.GetChange(cl.GetCommonAncestorWithUpstream(), None)
    files = [f.LocalPath() for f in change.AffectedFiles()]
    if not files:
      print('Cannot lint an empty CL')
      return 1

    # Process cpplints arguments if any.
    command = args + files
    if options.filter:
      command = ['--filter=' + ','.join(options.filter)] + command
    filenames = cpplint.ParseArguments(command)

    white_regex = re.compile(settings.GetLintRegex())
    black_regex = re.compile(settings.GetLintIgnoreRegex())
    extra_check_functions = [cpplint_chromium.CheckPointerDeclarationWhitespace]
    for filename in filenames:
      if white_regex.match(filename):
        if black_regex.match(filename):
          print('Ignoring file %s' % filename)
        else:
          cpplint.ProcessFile(filename, cpplint._cpplint_state.verbose_level,
                              extra_check_functions)
      else:
        print('Skipping file %s' % filename)
  finally:
    os.chdir(previous_cwd)
  print('Total errors found: %d\n' % cpplint._cpplint_state.error_count)
  if cpplint._cpplint_state.error_count != 0:
    return 1
  return 0


def CMDpresubmit(parser, args):
  """Runs presubmit tests on the current changelist."""
  parser.add_option('-u', '--upload', action='store_true',
                    help='Run upload hook instead of the push/dcommit hook')
  parser.add_option('-f', '--force', action='store_true',
                    help='Run checks even if tree is dirty')
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)

  if not options.force and git_common.is_dirty_git_tree('presubmit'):
    print('use --force to check even if tree is dirty.')
    return 1

  cl = Changelist(auth_config=auth_config)
  if args:
    base_branch = args[0]
  else:
    # Default to diffing against the common ancestor of the upstream branch.
    base_branch = cl.GetCommonAncestorWithUpstream()

  cl.RunHook(
      committing=not options.upload,
      may_prompt=False,
      verbose=options.verbose,
      change=cl.GetChange(base_branch, None))
  return 0


def GenerateGerritChangeId(message):
  """Returns Ixxxxxx...xxx change id.

  Works the same way as
  https://gerrit-review.googlesource.com/tools/hooks/commit-msg
  but can be called on demand on all platforms.

  The basic idea is to generate git hash of a state of the tree, original commit
  message, author/committer info and timestamps.
  """
  lines = []
  tree_hash = RunGitSilent(['write-tree'])
  lines.append('tree %s' % tree_hash.strip())
  code, parent = RunGitWithCode(['rev-parse', 'HEAD~0'], suppress_stderr=False)
  if code == 0:
    lines.append('parent %s' % parent.strip())
  author = RunGitSilent(['var', 'GIT_AUTHOR_IDENT'])
  lines.append('author %s' % author.strip())
  committer = RunGitSilent(['var', 'GIT_COMMITTER_IDENT'])
  lines.append('committer %s' % committer.strip())
  lines.append('')
  # Note: Gerrit's commit-hook actually cleans message of some lines and
  # whitespace. This code is not doing this, but it clearly won't decrease
  # entropy.
  lines.append(message)
  change_hash = RunCommand(['git', 'hash-object', '-t', 'commit', '--stdin'],
                           stdin='\n'.join(lines))
  return 'I%s' % change_hash.strip()


def GetTargetRef(remote, remote_branch, target_branch, pending_prefix):
  """Computes the remote branch ref to use for the CL.

  Args:
    remote (str): The git remote for the CL.
    remote_branch (str): The git remote branch for the CL.
    target_branch (str): The target branch specified by the user.
    pending_prefix (str): The pending prefix from the settings.
  """
  if not (remote and remote_branch):
    return None

  if target_branch:
    # Cannonicalize branch references to the equivalent local full symbolic
    # refs, which are then translated into the remote full symbolic refs
    # below.
    if '/' not in target_branch:
      remote_branch = 'refs/remotes/%s/%s' % (remote, target_branch)
    else:
      prefix_replacements = (
        ('^((refs/)?remotes/)?branch-heads/', 'refs/remotes/branch-heads/'),
        ('^((refs/)?remotes/)?%s/' % remote,  'refs/remotes/%s/' % remote),
        ('^(refs/)?heads/',                   'refs/remotes/%s/' % remote),
      )
      match = None
      for regex, replacement in prefix_replacements:
        match = re.search(regex, target_branch)
        if match:
          remote_branch = target_branch.replace(match.group(0), replacement)
          break
      if not match:
        # This is a branch path but not one we recognize; use as-is.
        remote_branch = target_branch
  elif remote_branch in REFS_THAT_ALIAS_TO_OTHER_REFS:
    # Handle the refs that need to land in different refs.
    remote_branch = REFS_THAT_ALIAS_TO_OTHER_REFS[remote_branch]

  # Create the true path to the remote branch.
  # Does the following translation:
  # * refs/remotes/origin/refs/diff/test -> refs/diff/test
  # * refs/remotes/origin/master -> refs/heads/master
  # * refs/remotes/branch-heads/test -> refs/branch-heads/test
  if remote_branch.startswith('refs/remotes/%s/refs/' % remote):
    remote_branch = remote_branch.replace('refs/remotes/%s/' % remote, '')
  elif remote_branch.startswith('refs/remotes/%s/' % remote):
    remote_branch = remote_branch.replace('refs/remotes/%s/' % remote,
                                          'refs/heads/')
  elif remote_branch.startswith('refs/remotes/branch-heads'):
    remote_branch = remote_branch.replace('refs/remotes/', 'refs/')
  # If a pending prefix exists then replace refs/ with it.
  if pending_prefix:
    remote_branch = remote_branch.replace('refs/', pending_prefix)
  return remote_branch


def cleanup_list(l):
  """Fixes a list so that comma separated items are put as individual items.

  So that "--reviewers joe@c,john@c --reviewers joa@c" results in
  options.reviewers == sorted(['joe@c', 'john@c', 'joa@c']).
  """
  items = sum((i.split(',') for i in l), [])
  stripped_items = (i.strip() for i in items)
  return sorted(filter(None, stripped_items))


@subcommand.usage('[args to "git diff"]')
def CMDupload(parser, args):
  """Uploads the current changelist to codereview.

  Can skip dependency patchset uploads for a branch by running:
    git config branch.branch_name.skip-deps-uploads True
  To unset run:
    git config --unset branch.branch_name.skip-deps-uploads
  Can also set the above globally by using the --global flag.
  """
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('--bypass-watchlists', action='store_true',
                    dest='bypass_watchlists',
                    help='bypass watchlists auto CC-ing reviewers')
  parser.add_option('-f', action='store_true', dest='force',
                    help="force yes to questions (don't prompt)")
  parser.add_option('-m', dest='message', help='message for patchset')
  parser.add_option('-b', '--bug',
                    help='pre-populate the bug number(s) for this issue. '
                         'If several, separate with commas')
  parser.add_option('--message-file', dest='message_file',
                    help='file which contains message for patchset')
  parser.add_option('-t', dest='title',
                    help='title for patchset (Rietveld only)')
  parser.add_option('-r', '--reviewers',
                    action='append', default=[],
                    help='reviewer email addresses')
  parser.add_option('--cc',
                    action='append', default=[],
                    help='cc email addresses')
  parser.add_option('-s', '--send-mail', action='store_true',
                    help='send email to reviewer immediately')
  parser.add_option('--emulate_svn_auto_props',
                    '--emulate-svn-auto-props',
                    action="store_true",
                    dest="emulate_svn_auto_props",
                    help="Emulate Subversion's auto properties feature.")
  parser.add_option('-c', '--use-commit-queue', action='store_true',
                    help='tell the commit queue to commit this patchset')
  parser.add_option('--private', action='store_true',
                    help='set the review private (rietveld only)')
  parser.add_option('--target_branch',
                    '--target-branch',
                    metavar='TARGET',
                    help='Apply CL to remote ref TARGET.  ' +
                         'Default: remote branch head, or master')
  parser.add_option('--squash', action='store_true',
                    help='Squash multiple commits into one (Gerrit only)')
  parser.add_option('--no-squash', action='store_true',
                    help='Don\'t squash multiple commits into one ' +
                         '(Gerrit only)')
  parser.add_option('--email', default=None,
                    help='email address to use to connect to Rietveld')
  parser.add_option('--tbr-owners', dest='tbr_owners', action='store_true',
                    help='add a set of OWNERS to TBR')
  parser.add_option('-d', '--cq-dry-run', dest='cq_dry_run',
                    action='store_true',
                    help='Send the patchset to do a CQ dry run right after '
                         'upload.')
  parser.add_option('--dependencies', action='store_true',
                    help='Uploads CLs of all the local branches that depend on '
                         'the current branch')

  orig_args = args
  add_git_similarity(parser)
  auth.add_auth_options(parser)
  _add_codereview_select_options(parser)
  (options, args) = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  auth_config = auth.extract_auth_config_from_options(options)

  if git_common.is_dirty_git_tree('upload'):
    return 1

  options.reviewers = cleanup_list(options.reviewers)
  options.cc = cleanup_list(options.cc)

  if options.message_file:
    if options.message:
      parser.error('only one of --message and --message-file allowed.')
    options.message = gclient_utils.FileRead(options.message_file)
    options.message_file = None

  if options.cq_dry_run and options.use_commit_queue:
    parser.error('only one of --use-commit-queue and --cq-dry-run allowed.')

  # For sanity of test expectations, do this otherwise lazy-loading *now*.
  settings.GetIsGerrit()

  cl = Changelist(auth_config=auth_config, codereview=options.forced_codereview)
  return cl.CMDUpload(options, args, orig_args)


def IsSubmoduleMergeCommit(ref):
  # When submodules are added to the repo, we expect there to be a single
  # non-git-svn merge commit at remote HEAD with a signature comment.
  pattern = '^SVN changes up to revision [0-9]*$'
  cmd = ['rev-list', '--merges', '--grep=%s' % pattern, '%s^!' % ref]
  return RunGit(cmd) != ''


def SendUpstream(parser, args, cmd):
  """Common code for CMDland and CmdDCommit

  In case of Gerrit, uses Gerrit REST api to "submit" the issue, which pushes
  upstream and closes the issue automatically and atomically.

  Otherwise (in case of Rietveld):
    Squashes branch into a single commit.
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
  add_git_similarity(parser)
  auth.add_auth_options(parser)
  (options, args) = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)

  cl = Changelist(auth_config=auth_config)

  # TODO(tandrii): refactor this into _RietveldChangelistImpl method.
  if cl.IsGerrit():
    if options.message:
      # This could be implemented, but it requires sending a new patch to
      # Gerrit, as Gerrit unlike Rietveld versions messages with patchsets.
      # Besides, Gerrit has the ability to change the commit message on submit
      # automatically, thus there is no need to support this option (so far?).
      parser.error('-m MESSAGE option is not supported for Gerrit.')
    if options.contributor:
      parser.error(
          '-c CONTRIBUTOR option is not supported for Gerrit.\n'
          'Before uploading a commit to Gerrit, ensure it\'s author field is '
          'the contributor\'s "name <email>". If you can\'t upload such a '
          'commit for review, contact your repository admin and request'
          '"Forge-Author" permission.')
    return cl._codereview_impl.CMDLand(options.force, options.bypass_hooks,
                                       options.verbose)

  current = cl.GetBranch()
  remote, upstream_branch = cl.FetchUpstreamTuple(cl.GetBranch())
  if not settings.GetIsGitSvn() and remote == '.':
    print()
    print('Attempting to push branch %r into another local branch!' % current)
    print()
    print('Either reparent this branch on top of origin/master:')
    print('  git reparent-branch --root')
    print()
    print('OR run `git rebase-update` if you think the parent branch is ')
    print('already committed.')
    print()
    print('  Current parent: %r' % upstream_branch)
    return 1

  if not args or cmd == 'land':
    # Default to merging against our best guess of the upstream branch.
    args = [cl.GetUpstreamBranch()]

  if options.contributor:
    if not re.match('^.*\s<\S+@\S+>$', options.contributor):
      print("Please provide contibutor as 'First Last <email@example.com>'")
      return 1

  base_branch = args[0]
  base_has_submodules = IsSubmoduleMergeCommit(base_branch)

  if git_common.is_dirty_git_tree(cmd):
    return 1

  # This rev-list syntax means "show all commits not in my branch that
  # are in base_branch".
  upstream_commits = RunGit(['rev-list', '^' + cl.GetBranchRef(),
                             base_branch]).splitlines()
  if upstream_commits:
    print('Base branch "%s" has %d commits '
          'not in this branch.' % (base_branch, len(upstream_commits)))
    print('Run "git merge %s" before attempting to %s.' % (base_branch, cmd))
    return 1

  # This is the revision `svn dcommit` will commit on top of.
  svn_head = None
  if cmd == 'dcommit' or base_has_submodules:
    svn_head = RunGit(['log', '--grep=^git-svn-id:', '-1',
                       '--pretty=format:%H'])

  if cmd == 'dcommit':
    # If the base_head is a submodule merge commit, the first parent of the
    # base_head should be a git-svn commit, which is what we're interested in.
    base_svn_head = base_branch
    if base_has_submodules:
      base_svn_head += '^1'

    extra_commits = RunGit(['rev-list', '^' + svn_head, base_svn_head])
    if extra_commits:
      print('This branch has %d additional commits not upstreamed yet.'
            % len(extra_commits.splitlines()))
      print('Upstream "%s" or rebase this branch on top of the upstream trunk '
            'before attempting to %s.' % (base_branch, cmd))
      return 1

  merge_base = RunGit(['merge-base', base_branch, 'HEAD']).strip()
  if not options.bypass_hooks:
    author = None
    if options.contributor:
      author = re.search(r'\<(.*)\>', options.contributor).group(1)
    hook_results = cl.RunHook(
        committing=True,
        may_prompt=not options.force,
        verbose=options.verbose,
        change=cl.GetChange(merge_base, author))
    if not hook_results.should_continue():
      return 1

    # Check the tree status if the tree status URL is set.
    status = GetTreeStatus()
    if 'closed' == status:
      print('The tree is closed.  Please wait for it to reopen. Use '
            '"git cl %s --bypass-hooks" to commit on a closed tree.' % cmd)
      return 1
    elif 'unknown' == status:
      print('Unable to determine tree status.  Please verify manually and '
            'use "git cl %s --bypass-hooks" to commit on a closed tree.' % cmd)
      return 1

  change_desc = ChangeDescription(options.message)
  if not change_desc.description and cl.GetIssue():
    change_desc = ChangeDescription(cl.GetDescription())

  if not change_desc.description:
    if not cl.GetIssue() and options.bypass_hooks:
      change_desc = ChangeDescription(CreateDescriptionFromLog([merge_base]))
    else:
      print('No description set.')
      print('Visit %s/edit to set it.' % (cl.GetIssueURL()))
      return 1

  # Keep a separate copy for the commit message, because the commit message
  # contains the link to the Rietveld issue, while the Rietveld message contains
  # the commit viewvc url.
  # Keep a separate copy for the commit message.
  if cl.GetIssue():
    change_desc.update_reviewers(cl.GetApprovingReviewers())

  commit_desc = ChangeDescription(change_desc.description)
  if cl.GetIssue():
    # Xcode won't linkify this URL unless there is a non-whitespace character
    # after it. Add a period on a new line to circumvent this. Also add a space
    # before the period to make sure that Gitiles continues to correctly resolve
    # the URL.
    commit_desc.append_footer('Review URL: %s .' % cl.GetIssueURL())
  if options.contributor:
    commit_desc.append_footer('Patch from %s.' % options.contributor)

  print('Description:')
  print(commit_desc.description)

  branches = [merge_base, cl.GetBranchRef()]
  if not options.force:
    print_stats(options.similarity, options.find_copies, branches)

  # We want to squash all this branch's commits into one commit with the proper
  # description. We do this by doing a "reset --soft" to the base branch (which
  # keeps the working copy the same), then dcommitting that.  If origin/master
  # has a submodule merge commit, we'll also need to cherry-pick the squashed
  # commit onto a branch based on the git-svn head.
  MERGE_BRANCH = 'git-cl-commit'
  CHERRY_PICK_BRANCH = 'git-cl-cherry-pick'
  # Delete the branches if they exist.
  for branch in [MERGE_BRANCH, CHERRY_PICK_BRANCH]:
    showref_cmd = ['show-ref', '--quiet', '--verify', 'refs/heads/%s' % branch]
    result = RunGitWithCode(showref_cmd)
    if result[0] == 0:
      RunGit(['branch', '-D', branch])

  # We might be in a directory that's present in this branch but not in the
  # trunk.  Move up to the top of the tree so that git commands that expect a
  # valid CWD won't fail after we check out the merge branch.
  rel_base_path = settings.GetRelativeRoot()
  if rel_base_path:
    os.chdir(rel_base_path)

  # Stuff our change into the merge branch.
  # We wrap in a try...finally block so if anything goes wrong,
  # we clean up the branches.
  retcode = -1
  pushed_to_pending = False
  pending_ref = None
  revision = None
  try:
    RunGit(['checkout', '-q', '-b', MERGE_BRANCH])
    RunGit(['reset', '--soft', merge_base])
    if options.contributor:
      RunGit(
          [
            'commit', '--author', options.contributor,
            '-m', commit_desc.description,
          ])
    else:
      RunGit(['commit', '-m', commit_desc.description])
    if base_has_submodules:
      cherry_pick_commit = RunGit(['rev-list', 'HEAD^!']).rstrip()
      RunGit(['branch', CHERRY_PICK_BRANCH, svn_head])
      RunGit(['checkout', CHERRY_PICK_BRANCH])
      RunGit(['cherry-pick', cherry_pick_commit])
    if cmd == 'land':
      remote, branch = cl.FetchUpstreamTuple(cl.GetBranch())
      mirror = settings.GetGitMirror(remote)
      pushurl = mirror.url if mirror else remote
      pending_prefix = settings.GetPendingRefPrefix()
      if not pending_prefix or branch.startswith(pending_prefix):
        # If not using refs/pending/heads/* at all, or target ref is already set
        # to pending, then push to the target ref directly.
        retcode, output = RunGitWithCode(
            ['push', '--porcelain', pushurl, 'HEAD:%s' % branch])
        pushed_to_pending = pending_prefix and branch.startswith(pending_prefix)
      else:
        # Cherry-pick the change on top of pending ref and then push it.
        assert branch.startswith('refs/'), branch
        assert pending_prefix[-1] == '/', pending_prefix
        pending_ref = pending_prefix + branch[len('refs/'):]
        retcode, output = PushToGitPending(pushurl, pending_ref, branch)
        pushed_to_pending = (retcode == 0)
      if retcode == 0:
        revision = RunGit(['rev-parse', 'HEAD']).strip()
    else:
      # dcommit the merge branch.
      cmd_args = [
        'svn', 'dcommit',
        '-C%s' % options.similarity,
        '--no-rebase', '--rmdir',
      ]
      if settings.GetForceHttpsCommitUrl():
        # Allow forcing https commit URLs for some projects that don't allow
        # committing to http URLs (like Google Code).
        remote_url = cl.GetGitSvnRemoteUrl()
        if urlparse.urlparse(remote_url).scheme == 'http':
          remote_url = remote_url.replace('http://', 'https://')
        cmd_args.append('--commit-url=%s' % remote_url)
      _, output = RunGitWithCode(cmd_args)
      if 'Committed r' in output:
        revision = re.match(
          '.*?\nCommitted r(\\d+)', output, re.DOTALL).group(1)
    logging.debug(output)
  finally:
    # And then swap back to the original branch and clean up.
    RunGit(['checkout', '-q', cl.GetBranch()])
    RunGit(['branch', '-D', MERGE_BRANCH])
    if base_has_submodules:
      RunGit(['branch', '-D', CHERRY_PICK_BRANCH])

  if not revision:
    print('Failed to push. If this persists, please file a bug.')
    return 1

  killed = False
  if pushed_to_pending:
    try:
      revision = WaitForRealCommit(remote, revision, base_branch, branch)
      # We set pushed_to_pending to False, since it made it all the way to the
      # real ref.
      pushed_to_pending = False
    except KeyboardInterrupt:
      killed = True

  if cl.GetIssue():
    to_pending = ' to pending queue' if pushed_to_pending else ''
    viewvc_url = settings.GetViewVCUrl()
    if not to_pending:
      if viewvc_url and revision:
        change_desc.append_footer(
            'Committed: %s%s' % (viewvc_url, revision))
      elif revision:
        change_desc.append_footer('Committed: %s' % (revision,))
    print('Closing issue '
          '(you may be prompted for your codereview password)...')
    cl.UpdateDescription(change_desc.description)
    cl.CloseIssue()
    props = cl.GetIssueProperties()
    patch_num = len(props['patchsets'])
    comment = "Committed patchset #%d (id:%d)%s manually as %s" % (
        patch_num, props['patchsets'][-1], to_pending, revision)
    if options.bypass_hooks:
      comment += ' (tree was closed).' if GetTreeStatus() == 'closed' else '.'
    else:
      comment += ' (presubmit successful).'
    cl.RpcServer().add_comment(cl.GetIssue(), comment)

  if pushed_to_pending:
    _, branch = cl.FetchUpstreamTuple(cl.GetBranch())
    print('The commit is in the pending queue (%s).' % pending_ref)
    print('It will show up on %s in ~1 min, once it gets a Cr-Commit-Position '
          'footer.' % branch)

  hook = POSTUPSTREAM_HOOK_PATTERN % cmd
  if os.path.isfile(hook):
    RunCommand([hook, merge_base], error_ok=True)

  return 1 if killed else 0


def WaitForRealCommit(remote, pushed_commit, local_base_ref, real_ref):
  print()
  print('Waiting for commit to be landed on %s...' % real_ref)
  print('(If you are impatient, you may Ctrl-C once without harm)')
  target_tree = RunGit(['rev-parse', '%s:' % pushed_commit]).strip()
  current_rev = RunGit(['rev-parse', local_base_ref]).strip()
  mirror = settings.GetGitMirror(remote)

  loop = 0
  while True:
    sys.stdout.write('fetching (%d)...        \r' % loop)
    sys.stdout.flush()
    loop += 1

    if mirror:
      mirror.populate()
    RunGit(['retry', 'fetch', remote, real_ref], stderr=subprocess2.VOID)
    to_rev = RunGit(['rev-parse', 'FETCH_HEAD']).strip()
    commits = RunGit(['rev-list', '%s..%s' % (current_rev, to_rev)])
    for commit in commits.splitlines():
      if RunGit(['rev-parse', '%s:' % commit]).strip() == target_tree:
        print('Found commit on %s' % real_ref)
        return commit

    current_rev = to_rev


def PushToGitPending(remote, pending_ref, upstream_ref):
  """Fetches pending_ref, cherry-picks current HEAD on top of it, pushes.

  Returns:
    (retcode of last operation, output log of last operation).
  """
  assert pending_ref.startswith('refs/'), pending_ref
  local_pending_ref = 'refs/git-cl/' + pending_ref[len('refs/'):]
  cherry = RunGit(['rev-parse', 'HEAD']).strip()
  code = 0
  out = ''
  max_attempts = 3
  attempts_left = max_attempts
  while attempts_left:
    if attempts_left != max_attempts:
      print('Retrying, %d attempts left...' % (attempts_left - 1,))
    attempts_left -= 1

    # Fetch. Retry fetch errors.
    print('Fetching pending ref %s...' % pending_ref)
    code, out = RunGitWithCode(
        ['retry', 'fetch', remote, '+%s:%s' % (pending_ref, local_pending_ref)])
    if code:
      print('Fetch failed with exit code %d.' % code)
      if out.strip():
        print(out.strip())
      continue

    # Try to cherry pick. Abort on merge conflicts.
    print('Cherry-picking commit on top of pending ref...')
    RunGitWithCode(['checkout', local_pending_ref], suppress_stderr=True)
    code, out = RunGitWithCode(['cherry-pick', cherry])
    if code:
      print('Your patch doesn\'t apply cleanly to ref \'%s\', '
            'the following files have merge conflicts:' % pending_ref)
      print(RunGit(['diff', '--name-status', '--diff-filter=U']).strip())
      print('Please rebase your patch and try again.')
      RunGitWithCode(['cherry-pick', '--abort'])
      return code, out

    # Applied cleanly, try to push now. Retry on error (flake or non-ff push).
    print('Pushing commit to %s... It can take a while.' % pending_ref)
    code, out = RunGitWithCode(
        ['retry', 'push', '--porcelain', remote, 'HEAD:%s' % pending_ref])
    if code == 0:
      # Success.
      print('Commit pushed to pending ref successfully!')
      return code, out

    print('Push failed with exit code %d.' % code)
    if out.strip():
      print(out.strip())
    if IsFatalPushFailure(out):
      print('Fatal push error. Make sure your .netrc credentials and git '
            'user.email are correct and you have push access to the repo.')
      return code, out

  print('All attempts to push to pending ref failed.')
  return code, out


def IsFatalPushFailure(push_stdout):
  """True if retrying push won't help."""
  return '(prohibited by Gerrit)' in push_stdout


@subcommand.usage('[upstream branch to apply against]')
def CMDdcommit(parser, args):
  """Commits the current changelist via git-svn."""
  if not settings.GetIsGitSvn():
    if git_footers.get_footer_svn_id():
      # If it looks like previous commits were mirrored with git-svn.
      message = """This repository appears to be a git-svn mirror, but no
upstream SVN master is set. You probably need to run 'git auto-svn' once."""
    else:
      message = """This doesn't appear to be an SVN repository.
If your project has a true, writeable git repository, you probably want to run
'git cl land' instead.
If your project has a git mirror of an upstream SVN master, you probably need
to run 'git svn init'.

Using the wrong command might cause your commit to appear to succeed, and the
review to be closed, without actually landing upstream. If you choose to
proceed, please verify that the commit lands upstream as expected."""
    print(message)
    ask_for_data('[Press enter to dcommit or ctrl-C to quit]')
  # TODO(tandrii): kill this post SVN migration with
  # https://codereview.chromium.org/2076683002
  print('WARNING: chrome infrastructure is migrating SVN repos to Git.\n'
        'Please let us know of this project you are committing to:'
        '    http://crbug.com/600451')
  return SendUpstream(parser, args, 'dcommit')


@subcommand.usage('[upstream branch to apply against]')
def CMDland(parser, args):
  """Commits the current changelist via git."""
  if settings.GetIsGitSvn() or git_footers.get_footer_svn_id():
    print('This appears to be an SVN repository.')
    print('Are you sure you didn\'t mean \'git cl dcommit\'?')
    print('(Ignore if this is the first commit after migrating from svn->git)')
    ask_for_data('[Press enter to push or ctrl-C to quit]')
  return SendUpstream(parser, args, 'land')


@subcommand.usage('<patch url or issue id or issue url>')
def CMDpatch(parser, args):
  """Patches in a code review."""
  parser.add_option('-b', dest='newbranch',
                    help='create a new branch off trunk for the patch')
  parser.add_option('-f', '--force', action='store_true',
                    help='with -b, clobber any existing branch')
  parser.add_option('-d', '--directory', action='store', metavar='DIR',
                    help='Change to the directory DIR immediately, '
                         'before doing anything else. Rietveld only.')
  parser.add_option('--reject', action='store_true',
                    help='failed patches spew .rej files rather than '
                        'attempting a 3-way merge. Rietveld only.')
  parser.add_option('-n', '--no-commit', action='store_true', dest='nocommit',
                    help='don\'t commit after patch applies. Rietveld only.')


  group = optparse.OptionGroup(
      parser,
      'Options for continuing work on the current issue uploaded from a '
      'different clone (e.g. different machine). Must be used independently '
      'from the other options. No issue number should be specified, and the '
      'branch must have an issue number associated with it')
  group.add_option('--reapply', action='store_true', dest='reapply',
                   help='Reset the branch and reapply the issue.\n'
                        'CAUTION: This will undo any local changes in this '
                        'branch')

  group.add_option('--pull', action='store_true', dest='pull',
                    help='Performs a pull before reapplying.')
  parser.add_option_group(group)

  auth.add_auth_options(parser)
  _add_codereview_select_options(parser)
  (options, args) = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  auth_config = auth.extract_auth_config_from_options(options)


  if options.reapply :
    if options.newbranch:
      parser.error('--reapply works on the current branch only')
    if len(args) > 0:
      parser.error('--reapply implies no additional arguments')

    cl = Changelist(auth_config=auth_config,
                    codereview=options.forced_codereview)
    if not cl.GetIssue():
      parser.error('current branch must have an associated issue')

    upstream = cl.GetUpstreamBranch()
    if upstream == None:
      parser.error('No upstream branch specified. Cannot reset branch')

    RunGit(['reset', '--hard', upstream])
    if options.pull:
      RunGit(['pull'])

    return cl.CMDPatchIssue(cl.GetIssue(), options.reject, options.nocommit,
                            options.directory)

  if len(args) != 1 or not args[0]:
    parser.error('Must specify issue number or url')

  # We don't want uncommitted changes mixed up with the patch.
  if git_common.is_dirty_git_tree('patch'):
    return 1

  if options.newbranch:
    if options.force:
      RunGit(['branch', '-D', options.newbranch],
             stderr=subprocess2.PIPE, error_ok=True)
    RunGit(['new-branch', options.newbranch])

  cl = Changelist(auth_config=auth_config, codereview=options.forced_codereview)

  if cl.IsGerrit():
    if options.reject:
      parser.error('--reject is not supported with Gerrit codereview.')
    if options.nocommit:
      parser.error('--nocommit is not supported with Gerrit codereview.')
    if options.directory:
      parser.error('--directory is not supported with Gerrit codereview.')

  return cl.CMDPatchIssue(args[0], options.reject, options.nocommit,
                          options.directory)


def CMDrebase(parser, args):
  """Rebases current branch on top of svn repo."""
  # Provide a wrapper for git svn rebase to help avoid accidental
  # git svn dcommit.
  # It's the only command that doesn't use parser at all since we just defer
  # execution to git-svn.

  return RunGitWithCode(['svn', 'rebase'] + args)[1]


def GetTreeStatus(url=None):
  """Fetches the tree status and returns either 'open', 'closed',
  'unknown' or 'unset'."""
  url = url or settings.GetTreeStatusUrl(error_ok=True)
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
  url = settings.GetTreeStatusUrl()
  json_url = urlparse.urljoin(url, '/current?format=json')
  connection = urllib2.urlopen(json_url)
  status = json.loads(connection.read())
  connection.close()
  return status['message']


def GetBuilderMaster(bot_list):
  """For a given builder, fetch the master from AE if available."""
  map_url = 'https://builders-map.appspot.com/'
  try:
    master_map = json.load(urllib2.urlopen(map_url))
  except urllib2.URLError as e:
    return None, ('Failed to fetch builder-to-master map from %s. Error: %s.' %
                  (map_url, e))
  except ValueError as e:
    return None, ('Invalid json string from %s. Error: %s.' % (map_url, e))
  if not master_map:
    return None, 'Failed to build master map.'

  result_master = ''
  for bot in bot_list:
    builder = bot.split(':', 1)[0]
    master_list = master_map.get(builder, [])
    if not master_list:
      return None, ('No matching master for builder %s.' % builder)
    elif len(master_list) > 1:
      return None, ('The builder name %s exists in multiple masters %s.' %
                    (builder, master_list))
    else:
      cur_master = master_list[0]
      if not result_master:
        result_master = cur_master
      elif result_master != cur_master:
        return None, 'The builders do not belong to the same master.'
  return result_master, None


def CMDtree(parser, args):
  """Shows the status of the tree."""
  _, args = parser.parse_args(args)
  status = GetTreeStatus()
  if 'unset' == status:
    print('You must configure your tree status URL by running "git cl config".')
    return 2

  print('The tree is %s' % status)
  print()
  print(GetTreeStatusReason())
  if status != 'open':
    return 1
  return 0


def CMDtry(parser, args):
  """Triggers try jobs through BuildBucket."""
  group = optparse.OptionGroup(parser, "Try job options")
  group.add_option(
      "-b", "--bot", action="append",
      help=("IMPORTANT: specify ONE builder per --bot flag. Use it multiple "
            "times to specify multiple builders. ex: "
            "'-b win_rel -b win_layout'. See "
            "the try server waterfall for the builders name and the tests "
            "available."))
  group.add_option(
      "-m", "--master", default='',
      help=("Specify a try master where to run the tries."))
  group.add_option( "--luci", action='store_true')
  group.add_option(
      "-r", "--revision",
      help="Revision to use for the try job; default: the "
           "revision will be determined by the try server; see "
           "its waterfall for more info")
  group.add_option(
      "-c", "--clobber", action="store_true", default=False,
      help="Force a clobber before building; e.g. don't do an "
           "incremental build")
  group.add_option(
      "--project",
      help="Override which project to use. Projects are defined "
           "server-side to define what default bot set to use")
  group.add_option(
      "-p", "--property", dest="properties", action="append", default=[],
      help="Specify generic properties in the form -p key1=value1 -p "
           "key2=value2 etc (buildbucket only). The value will be treated as "
           "json if decodable, or as string otherwise.")
  group.add_option(
      "-n", "--name", help="Try job name; default to current branch name")
  group.add_option(
      "--use-rietveld", action="store_true", default=False,
      help="Use Rietveld to trigger try jobs.")
  group.add_option(
      "--buildbucket-host", default='cr-buildbucket.appspot.com',
      help="Host of buildbucket. The default host is %default.")
  parser.add_option_group(group)
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)

  if options.use_rietveld and options.properties:
    parser.error('Properties can only be specified with buildbucket')

  # Make sure that all properties are prop=value pairs.
  bad_params = [x for x in options.properties if '=' not in x]
  if bad_params:
    parser.error('Got properties with missing "=": %s' % bad_params)

  if args:
    parser.error('Unknown arguments: %s' % args)

  cl = Changelist(auth_config=auth_config)
  if not cl.GetIssue():
    parser.error('Need to upload first')

  if cl.IsGerrit():
    parser.error(
        'Not yet supported for Gerrit (http://crbug.com/599931).\n'
        'If your project has Commit Queue, dry run is a workaround:\n'
        '  git cl set-commit --dry-run')
  # Code below assumes Rietveld issue.
  # TODO(tandrii): actually implement for Gerrit http://crbug.com/599931.

  props = cl.GetIssueProperties()
  if props.get('closed'):
    parser.error('Cannot send tryjobs for a closed CL')

  if props.get('private'):
    parser.error('Cannot use trybots with private issue')

  if not options.name:
    options.name = cl.GetBranch()

  if options.bot and not options.master:
    options.master, err_msg = GetBuilderMaster(options.bot)
    if err_msg:
      parser.error('Tryserver master cannot be found because: %s\n'
                   'Please manually specify the tryserver master'
                   ', e.g. "-m tryserver.chromium.linux".' % err_msg)

  def GetMasterMap():
    # Process --bot.
    if not options.bot:
      change = cl.GetChange(cl.GetCommonAncestorWithUpstream(), None)

      # Get try masters from PRESUBMIT.py files.
      masters = presubmit_support.DoGetTryMasters(
          change,
          change.LocalPaths(),
          settings.GetRoot(),
          None,
          None,
          options.verbose,
          sys.stdout)
      if masters:
        return masters

      # Fall back to deprecated method: get try slaves from PRESUBMIT.py files.
      options.bot = presubmit_support.DoGetTrySlaves(
          change,
          change.LocalPaths(),
          settings.GetRoot(),
          None,
          None,
          options.verbose,
          sys.stdout)

    if not options.bot:
      return {}

    builders_and_tests = {}
    # TODO(machenbach): The old style command-line options don't support
    # multiple try masters yet.
    old_style = filter(lambda x: isinstance(x, basestring), options.bot)
    new_style = filter(lambda x: isinstance(x, tuple), options.bot)

    for bot in old_style:
      if ':' in bot:
        parser.error('Specifying testfilter is no longer supported')
      elif ',' in bot:
        parser.error('Specify one bot per --bot flag')
      else:
        builders_and_tests.setdefault(bot, [])

    for bot, tests in new_style:
      builders_and_tests.setdefault(bot, []).extend(tests)

    # Return a master map with one master to be backwards compatible. The
    # master name defaults to an empty string, which will cause the master
    # not to be set on rietveld (deprecated).
    return {options.master: builders_and_tests}

  masters = GetMasterMap()
  if not masters:
    # Default to triggering Dry Run (see http://crbug.com/625697).
    if options.verbose:
      print('git cl try with no bots now defaults to CQ Dry Run.')
    try:
      cl.SetCQState(_CQState.DRY_RUN)
      print('scheduled CQ Dry Run on %s' % cl.GetIssueURL())
      return 0
    except KeyboardInterrupt:
      raise
    except:
      print('WARNING: failed to trigger CQ Dry Run.\n'
            'Either:\n'
            ' * your project has no CQ\n'
            ' * you don\'t have permission to trigger Dry Run\n'
            ' * bug in this code (see stack trace below).\n'
            'Consider specifying which bots to trigger manually '
            'or asking your project owners for permissions '
            'or contacting Chrome Infrastructure team at '
            'https://www.chromium.org/infra\n\n')
      # Still raise exception so that stack trace is printed.
      raise

  for builders in masters.itervalues():
    if any('triggered' in b for b in builders):
      print('ERROR You are trying to send a job to a triggered bot. This type '
            'of bot requires an\ninitial job from a parent (usually a builder).'
            '  Instead send your job to the parent.\n'
            'Bot list: %s' % builders, file=sys.stderr)
      return 1

  patchset = cl.GetMostRecentPatchset()
  if patchset and patchset != cl.GetPatchset():
    print(
        '\nWARNING Mismatch between local config and server. Did a previous '
        'upload fail?\ngit-cl try always uses latest patchset from rietveld. '
        'Continuing using\npatchset %s.\n' % patchset)
  if options.luci:
    trigger_luci_job(cl, masters, options)
  elif not options.use_rietveld:
    try:
      trigger_try_jobs(auth_config, cl, options, masters, 'git_cl_try')
    except BuildbucketResponseException as ex:
      print('ERROR: %s' % ex)
      return 1
    except Exception as e:
      stacktrace = (''.join(traceback.format_stack()) + traceback.format_exc())
      print('ERROR: Exception when trying to trigger tryjobs: %s\n%s' %
            (e, stacktrace))
      return 1
  else:
    try:
      cl.RpcServer().trigger_distributed_try_jobs(
          cl.GetIssue(), patchset, options.name, options.clobber,
          options.revision, masters)
    except urllib2.HTTPError as e:
      if e.code == 404:
        print('404 from rietveld; '
              'did you mean to use "git try" instead of "git cl try"?')
        return 1
    print('Tried jobs on:')

    for (master, builders) in sorted(masters.iteritems()):
      if master:
        print('Master: %s' % master)
      length = max(len(builder) for builder in builders)
      for builder in sorted(builders):
        print('  %*s: %s' % (length, builder, ','.join(builders[builder])))
  return 0


def CMDtry_results(parser, args):
  group = optparse.OptionGroup(parser, "Try job results options")
  group.add_option(
      "-p", "--patchset", type=int, help="patchset number if not current.")
  group.add_option(
      "--print-master", action='store_true', help="print master name as well.")
  group.add_option(
      "--color", action='store_true', default=setup_color.IS_TTY,
      help="force color output, useful when piping output.")
  group.add_option(
      "--buildbucket-host", default='cr-buildbucket.appspot.com',
      help="Host of buildbucket. The default host is %default.")
  parser.add_option_group(group)
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  auth_config = auth.extract_auth_config_from_options(options)
  cl = Changelist(auth_config=auth_config)
  if not cl.GetIssue():
    parser.error('Need to upload first')

  if not options.patchset:
    options.patchset = cl.GetMostRecentPatchset()
    if options.patchset and options.patchset != cl.GetPatchset():
      print(
          '\nWARNING Mismatch between local config and server. Did a previous '
          'upload fail?\ngit-cl try always uses latest patchset from rietveld. '
          'Continuing using\npatchset %s.\n' % options.patchset)
  try:
    jobs = fetch_try_jobs(auth_config, cl, options)
  except BuildbucketResponseException as ex:
    print('Buildbucket error: %s' % ex)
    return 1
  except Exception as e:
    stacktrace = (''.join(traceback.format_stack()) + traceback.format_exc())
    print('ERROR: Exception when trying to fetch tryjobs: %s\n%s' %
          (e, stacktrace))
    return 1
  print_tryjobs(options, jobs)
  return 0


@subcommand.usage('[new upstream branch]')
def CMDupstream(parser, args):
  """Prints or sets the name of the upstream branch, if any."""
  _, args = parser.parse_args(args)
  if len(args) > 1:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  cl = Changelist()
  if args:
    # One arg means set upstream branch.
    branch = cl.GetBranch()
    RunGit(['branch', '--set-upstream', branch, args[0]])
    cl = Changelist()
    print('Upstream branch set to %s' % (cl.GetUpstreamBranch(),))

    # Clear configured merge-base, if there is one.
    git_common.remove_merge_base(branch)
  else:
    print(cl.GetUpstreamBranch())
  return 0


def CMDweb(parser, args):
  """Opens the current CL in the web browser."""
  _, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  issue_url = Changelist().GetIssueURL()
  if not issue_url:
    print('ERROR No issue to open', file=sys.stderr)
    return 1

  webbrowser.open(issue_url)
  return 0


def CMDset_commit(parser, args):
  """Sets the commit bit to trigger the Commit Queue."""
  parser.add_option('-d', '--dry-run', action='store_true',
                    help='trigger in dry run mode')
  parser.add_option('-c', '--clear', action='store_true',
                    help='stop CQ run, if any')
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))
  if options.dry_run and options.clear:
    parser.error('Make up your mind: both --dry-run and --clear not allowed')

  cl = Changelist(auth_config=auth_config)
  if options.clear:
    state = _CQState.NONE
  elif options.dry_run:
    state = _CQState.DRY_RUN
  else:
    state = _CQState.COMMIT
  if not cl.GetIssue():
    parser.error('Must upload the issue first')
  cl.SetCQState(state)
  return 0


def CMDset_close(parser, args):
  """Closes the issue."""
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))
  cl = Changelist(auth_config=auth_config)
  # Ensure there actually is an issue to close.
  cl.GetDescription()
  cl.CloseIssue()
  return 0


def CMDdiff(parser, args):
  """Shows differences between local tree and last upload."""
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  # Uncommitted (staged and unstaged) changes will be destroyed by
  # "git reset --hard" if there are merging conflicts in CMDPatchIssue().
  # Staged changes would be committed along with the patch from last
  # upload, hence counted toward the "last upload" side in the final
  # diff output, and this is not what we want.
  if git_common.is_dirty_git_tree('diff'):
    return 1

  cl = Changelist(auth_config=auth_config)
  issue = cl.GetIssue()
  branch = cl.GetBranch()
  if not issue:
    DieWithError('No issue found for current branch (%s)' % branch)
  TMP_BRANCH = 'git-cl-diff'
  base_branch = cl.GetCommonAncestorWithUpstream()

  # Create a new branch based on the merge-base
  RunGit(['checkout', '-q', '-b', TMP_BRANCH, base_branch])
  # Clear cached branch in cl object, to avoid overwriting original CL branch
  # properties.
  cl.ClearBranch()
  try:
    rtn = cl.CMDPatchIssue(issue, reject=False, nocommit=False, directory=None)
    if rtn != 0:
      RunGit(['reset', '--hard'])
      return rtn

    # Switch back to starting branch and diff against the temporary
    # branch containing the latest rietveld patch.
    subprocess2.check_call(['git', 'diff', TMP_BRANCH, branch, '--'])
  finally:
    RunGit(['checkout', '-q', branch])
    RunGit(['branch', '-D', TMP_BRANCH])

  return 0


def CMDowners(parser, args):
  """Interactively find the owners for reviewing."""
  parser.add_option(
      '--no-color',
      action='store_true',
      help='Use this option to disable color output')
  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth_config = auth.extract_auth_config_from_options(options)

  author = RunGit(['config', 'user.email']).strip() or None

  cl = Changelist(auth_config=auth_config)

  if args:
    if len(args) > 1:
      parser.error('Unknown args')
    base_branch = args[0]
  else:
    # Default to diffing against the common ancestor of the upstream branch.
    base_branch = cl.GetCommonAncestorWithUpstream()

  change = cl.GetChange(base_branch, None)
  return owners_finder.OwnersFinder(
      [f.LocalPath() for f in
          cl.GetChange(base_branch, None).AffectedFiles()],
      change.RepositoryRoot(), author,
      fopen=file, os_path=os.path,
      disable_color=options.no_color).run()


def BuildGitDiffCmd(diff_type, upstream_commit, args):
  """Generates a diff command."""
  # Generate diff for the current branch's changes.
  diff_cmd = ['diff', '--no-ext-diff', '--no-prefix', diff_type,
              upstream_commit, '--' ]

  if args:
    for arg in args:
      if os.path.isdir(arg) or os.path.isfile(arg):
        diff_cmd.append(arg)
      else:
        DieWithError('Argument "%s" is not a file or a directory' % arg)

  return diff_cmd

def MatchingFileType(file_name, extensions):
  """Returns true if the file name ends with one of the given extensions."""
  return bool([ext for ext in extensions if file_name.lower().endswith(ext)])

@subcommand.usage('[files or directories to diff]')
def CMDformat(parser, args):
  """Runs auto-formatting tools (clang-format etc.) on the diff."""
  CLANG_EXTS = ['.cc', '.cpp', '.h', '.m', '.mm', '.proto', '.java']
  GN_EXTS = ['.gn', '.gni', '.typemap']
  parser.add_option('--full', action='store_true',
                    help='Reformat the full content of all touched files')
  parser.add_option('--dry-run', action='store_true',
                    help='Don\'t modify any file on disk.')
  parser.add_option('--python', action='store_true',
                    help='Format python code with yapf (experimental).')
  parser.add_option('--diff', action='store_true',
                    help='Print diff to stdout rather than modifying files.')
  opts, args = parser.parse_args(args)

  # git diff generates paths against the root of the repository.  Change
  # to that directory so clang-format can find files even within subdirs.
  rel_base_path = settings.GetRelativeRoot()
  if rel_base_path:
    os.chdir(rel_base_path)

  # Grab the merge-base commit, i.e. the upstream commit of the current
  # branch when it was created or the last time it was rebased. This is
  # to cover the case where the user may have called "git fetch origin",
  # moving the origin branch to a newer commit, but hasn't rebased yet.
  upstream_commit = None
  cl = Changelist()
  upstream_branch = cl.GetUpstreamBranch()
  if upstream_branch:
    upstream_commit = RunGit(['merge-base', 'HEAD', upstream_branch])
    upstream_commit = upstream_commit.strip()

  if not upstream_commit:
    DieWithError('Could not find base commit for this branch. '
                 'Are you in detached state?')

  changed_files_cmd = BuildGitDiffCmd('--name-only', upstream_commit, args)
  diff_output = RunGit(changed_files_cmd)
  diff_files = diff_output.splitlines()
  # Filter out files deleted by this CL
  diff_files = [x for x in diff_files if os.path.isfile(x)]

  clang_diff_files = [x for x in diff_files if MatchingFileType(x, CLANG_EXTS)]
  python_diff_files = [x for x in diff_files if MatchingFileType(x, ['.py'])]
  dart_diff_files = [x for x in diff_files if MatchingFileType(x, ['.dart'])]
  gn_diff_files = [x for x in diff_files if MatchingFileType(x, GN_EXTS)]

  top_dir = os.path.normpath(
      RunGit(["rev-parse", "--show-toplevel"]).rstrip('\n'))

  # Set to 2 to signal to CheckPatchFormatted() that this patch isn't
  # formatted. This is used to block during the presubmit.
  return_value = 0

  if clang_diff_files:
    # Locate the clang-format binary in the checkout
    try:
      clang_format_tool = clang_format.FindClangFormatToolInChromiumTree()
    except clang_format.NotFoundError as e:
      DieWithError(e)

    if opts.full:
      cmd = [clang_format_tool]
      if not opts.dry_run and not opts.diff:
        cmd.append('-i')
      stdout = RunCommand(cmd + clang_diff_files, cwd=top_dir)
      if opts.diff:
        sys.stdout.write(stdout)
    else:
      env = os.environ.copy()
      env['PATH'] = str(os.path.dirname(clang_format_tool))
      try:
        script = clang_format.FindClangFormatScriptInChromiumTree(
            'clang-format-diff.py')
      except clang_format.NotFoundError as e:
        DieWithError(e)

      cmd = [sys.executable, script, '-p0']
      if not opts.dry_run and not opts.diff:
        cmd.append('-i')

      diff_cmd = BuildGitDiffCmd('-U0', upstream_commit, clang_diff_files)
      diff_output = RunGit(diff_cmd)

      stdout = RunCommand(cmd, stdin=diff_output, cwd=top_dir, env=env)
      if opts.diff:
        sys.stdout.write(stdout)
      if opts.dry_run and len(stdout) > 0:
        return_value = 2

  # Similar code to above, but using yapf on .py files rather than clang-format
  # on C/C++ files
  if opts.python:
    yapf_tool = gclient_utils.FindExecutable('yapf')
    if yapf_tool is None:
      DieWithError('yapf not found in PATH')

    if opts.full:
      if python_diff_files:
        cmd = [yapf_tool]
        if not opts.dry_run and not opts.diff:
          cmd.append('-i')
        stdout = RunCommand(cmd + python_diff_files, cwd=top_dir)
        if opts.diff:
          sys.stdout.write(stdout)
    else:
      # TODO(sbc): yapf --lines mode still has some issues.
      # https://github.com/google/yapf/issues/154
      DieWithError('--python currently only works with --full')

  # Dart's formatter does not have the nice property of only operating on
  # modified chunks, so hard code full.
  if dart_diff_files:
    try:
      command = [dart_format.FindDartFmtToolInChromiumTree()]
      if not opts.dry_run and not opts.diff:
        command.append('-w')
      command.extend(dart_diff_files)

      stdout = RunCommand(command, cwd=top_dir)
      if opts.dry_run and stdout:
        return_value = 2
    except dart_format.NotFoundError as e:
      print('Warning: Unable to check Dart code formatting. Dart SDK not '
            'found in this checkout. Files in other languages are still '
            'formatted.')

  # Format GN build files. Always run on full build files for canonical form.
  if gn_diff_files:
    cmd = ['gn', 'format']
    if not opts.dry_run and not opts.diff:
      cmd.append('--in-place')
    for gn_diff_file in gn_diff_files:
      stdout = RunCommand(cmd + [gn_diff_file],
                          shell=sys.platform == 'win32',
                          cwd=top_dir)
      if opts.diff:
        sys.stdout.write(stdout)

  return return_value


@subcommand.usage('<codereview url or issue id>')
def CMDcheckout(parser, args):
  """Checks out a branch associated with a given Rietveld or Gerrit issue."""
  _, args = parser.parse_args(args)

  if len(args) != 1:
    parser.print_help()
    return 1

  issue_arg = ParseIssueNumberArgument(args[0])
  if not issue_arg.valid:
    parser.print_help()
    return 1
  target_issue = str(issue_arg.issue)

  def find_issues(issueprefix):
    output = RunGit(['config', '--local', '--get-regexp',
                     r'branch\..*\.%s' % issueprefix],
                     error_ok=True)
    for key, issue in [x.split() for x in output.splitlines()]:
      if issue == target_issue:
        yield re.sub(r'branch\.(.*)\.%s' % issueprefix, r'\1', key)

  branches = []
  for cls in _CODEREVIEW_IMPLEMENTATIONS.values():
    branches.extend(find_issues(cls.IssueSettingSuffix()))
  if len(branches) == 0:
    print('No branch found for issue %s.' % target_issue)
    return 1
  if len(branches) == 1:
    RunGit(['checkout', branches[0]])
  else:
    print('Multiple branches match issue %s:' % target_issue)
    for i in range(len(branches)):
      print('%d: %s' % (i, branches[i]))
    which = raw_input('Choose by index: ')
    try:
      RunGit(['checkout', branches[int(which)]])
    except (IndexError, ValueError):
      print('Invalid selection, not checking out any branch.')
      return 1

  return 0


def CMDlol(parser, args):
  # This command is intentionally undocumented.
  print(zlib.decompress(base64.b64decode(
      'eNptkLEOwyAMRHe+wupCIqW57v0Vq84WqWtXyrcXnCBsmgMJ+/SSAxMZgRB6NzE'
      'E2ObgCKJooYdu4uAQVffUEoE1sRQLxAcqzd7uK2gmStrll1ucV3uZyaY5sXyDd9'
      'JAnN+lAXsOMJ90GANAi43mq5/VeeacylKVgi8o6F1SC63FxnagHfJUTfUYdCR/W'
      'Ofe+0dHL7PicpytKP750Fh1q2qnLVof4w8OZWNY')))
  return 0


class OptionParser(optparse.OptionParser):
  """Creates the option parse and add --verbose support."""
  def __init__(self, *args, **kwargs):
    optparse.OptionParser.__init__(
        self, *args, prog='git cl', version=__version__, **kwargs)
    self.add_option(
        '-v', '--verbose', action='count', default=0,
        help='Use 2 times for more debugging info')

  def parse_args(self, args=None, values=None):
    options, args = optparse.OptionParser.parse_args(self, args, values)
    levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(level=levels[min(options.verbose, len(levels) - 1)])
    return options, args


def main(argv):
  if sys.hexversion < 0x02060000:
    print('\nYour python version %s is unsupported, please upgrade.\n' %
          (sys.version.split(' ', 1)[0],), file=sys.stderr)
    return 2

  # Reload settings.
  global settings
  settings = Settings()

  colorize_CMDstatus_doc()
  dispatcher = subcommand.CommandDispatcher(__name__)
  try:
    return dispatcher.execute(OptionParser(), argv)
  except auth.AuthenticationError as e:
    DieWithError(str(e))
  except urllib2.HTTPError as e:
    if e.code != 500:
      raise
    DieWithError(
        ('AppEngine is misbehaving and returned HTTP %d, again. Keep faith '
          'and retry or visit go/isgaeup.\n%s') % (e.code, str(e)))
  return 0


if __name__ == '__main__':
  # These affect sys.stdout so do it outside of main() to simplify mocks in
  # unit testing.
  fix_encoding.fix_encoding()
  setup_color.init()
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(1)
