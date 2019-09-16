#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copyright (C) 2008 Evan Martin <martine@danga.com>

"""A git-command for integrating reviews on Gerrit."""

from __future__ import print_function

from distutils.version import LooseVersion
from multiprocessing.pool import ThreadPool
import base64
import collections
import contextlib
import datetime
import httplib
import itertools
import json
import logging
import multiprocessing
import optparse
import os
import re
import shutil
import stat
import sys
import tempfile
import textwrap
import time
import urllib
import urllib2
import urlparse
import uuid
import webbrowser
import zlib

from third_party import colorama
from third_party import httplib2
import auth
import clang_format
import dart_format
import fix_encoding
import gclient_utils
import gerrit_util
import git_common
import git_footers
import metrics
import metrics_utils
import owners
import owners_finder
import presubmit_support
import scm
import setup_color
import split_cl
import subcommand
import subprocess2
import watchlists

__version__ = '2.0'

# Traces for git push will be stored in a traces directory inside the
# depot_tools checkout.
DEPOT_TOOLS = os.path.dirname(os.path.abspath(__file__))
TRACES_DIR = os.path.join(DEPOT_TOOLS, 'traces')

# When collecting traces, Git hashes will be reduced to 6 characters to reduce
# the size after compression.
GIT_HASH_RE = re.compile(r'\b([a-f0-9]{6})[a-f0-9]{34}\b', flags=re.I)
# Used to redact the cookies from the gitcookies file.
GITCOOKIES_REDACT_RE = re.compile(r'1/.*')

# The maximum number of traces we will keep. Multiplied by 3 since we store
# 3 files per trace.
MAX_TRACES = 3 * 10
# Message to be displayed to the user to inform where to find the traces for a
# git-cl upload execution.
TRACES_MESSAGE = (
'\n'
'The traces of this git-cl execution have been recorded at:\n'
'  %(trace_name)s-traces.zip\n'
'Copies of your gitcookies file and git config have been recorded at:\n'
'  %(trace_name)s-git-info.zip\n')
# Format of the message to be stored as part of the traces to give developers a
# better context when they go through traces.
TRACES_README_FORMAT = (
'Date: %(now)s\n'
'\n'
'Change: https://%(gerrit_host)s/q/%(change_id)s\n'
'Title: %(title)s\n'
'\n'
'%(description)s\n'
'\n'
'Execution time: %(execution_time)s\n'
'Exit code: %(exit_code)s\n') + TRACES_MESSAGE

COMMIT_BOT_EMAIL = 'commit-bot@chromium.org'
POSTUPSTREAM_HOOK = '.git/hooks/post-cl-land'
DESCRIPTION_BACKUP_FILE = '~/.git_cl_description_backup'
REFS_THAT_ALIAS_TO_OTHER_REFS = {
    'refs/remotes/origin/lkgr': 'refs/remotes/origin/master',
    'refs/remotes/origin/lkcr': 'refs/remotes/origin/master',
}

# Valid extensions for files we want to lint.
DEFAULT_LINT_REGEX = r"(.*\.cpp|.*\.cc|.*\.h)"
DEFAULT_LINT_IGNORE_REGEX = r"$^"

# File name for yapf style config files.
YAPF_CONFIG_FILENAME = '.style.yapf'

# Shortcut since it quickly becomes repetitive.
Fore = colorama.Fore

# Initialized in main()
settings = None

# Used by tests/git_cl_test.py to add extra logging.
# Inside the weirdly failing test, add this:
# >>> self.mock(git_cl, '_IS_BEING_TESTED', True)
# And scroll up to see the stack trace printed.
_IS_BEING_TESTED = False


def DieWithError(message, change_desc=None):
  if change_desc:
    SaveDescriptionBackup(change_desc)

  print(message, file=sys.stderr)
  sys.exit(1)


def SaveDescriptionBackup(change_desc):
  backup_path = os.path.expanduser(DESCRIPTION_BACKUP_FILE)
  print('\nsaving CL description to %s\n' % backup_path)
  backup_file = open(backup_path, 'w')
  backup_file.write(change_desc.description)
  backup_file.close()


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
  if suppress_stderr:
    stderr = subprocess2.VOID
  else:
    stderr = sys.stderr
  try:
    (out, _), code = subprocess2.communicate(['git'] + args,
                                             env=GetNoGitPagerEnv(),
                                             stdout=subprocess2.PIPE,
                                             stderr=stderr)
    return code, out
  except subprocess2.CalledProcessError as e:
    logging.debug('Failed running %s', ['git'] + args)
    return e.returncode, e.stdout


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


def time_sleep(seconds):
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.sleep(seconds)


def time_time():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.time()


def datetime_now():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return datetime.datetime.now()


def ask_for_data(prompt):
  try:
    return raw_input(prompt)
  except KeyboardInterrupt:
    # Hide the exception.
    sys.exit(1)


def confirm_or_exit(prefix='', action='confirm'):
  """Asks user to press enter to continue or press Ctrl+C to abort."""
  if not prefix or prefix.endswith('\n'):
    mid = 'Press'
  elif prefix.endswith('.') or prefix.endswith('?'):
    mid = ' Press'
  elif prefix.endswith(' '):
    mid = 'press'
  else:
    mid = ' press'
  ask_for_data('%s%s Enter to %s, or Ctrl+C to abort' % (prefix, mid, action))


def ask_for_explicit_yes(prompt):
  """Returns whether user typed 'y' or 'yes' to confirm the given prompt."""
  result = ask_for_data(prompt + ' [Yes/No]: ').lower()
  while True:
    if 'yes'.startswith(result):
      return True
    if 'no'.startswith(result):
      return False
    result = ask_for_data('Please, type yes or no: ').lower()


def _git_branch_config_key(branch, key):
  """Helper method to return Git config key for a branch."""
  assert branch, 'branch name is required to set git config for it'
  return 'branch.%s.%s' % (branch, key)


def _git_get_branch_config_value(key, default=None, value_type=str,
                                 branch=False):
  """Returns git config value of given or current branch if any.

  Returns default in all other cases.
  """
  assert value_type in (int, str, bool)
  if branch is False:  # Distinguishing default arg value from None.
    branch = GetCurrentBranch()

  if not branch:
    return default

  args = ['config']
  if value_type == bool:
    args.append('--bool')
  # `git config` also has --int, but apparently git config suffers from integer
  # overflows (http://crbug.com/640115), so don't use it.
  args.append(_git_branch_config_key(branch, key))
  code, out = RunGitWithCode(args)
  if code == 0:
    value = out.strip()
    if value_type == int:
      return int(value)
    if value_type == bool:
      return bool(value.lower() == 'true')
    return value
  return default


def _git_set_branch_config_value(key, value, branch=None, **kwargs):
  """Sets or unsets the git branch config value.

  If value is None, the key will be unset, otherwise it will be set.
  If no branch is given, the currently checked out branch is used.
  """
  if not branch:
    branch = GetCurrentBranch()
    assert branch, 'a branch name OR currently checked out branch is required'
  args = ['config']
  # Check for boolean first, because bool is int, but int is not bool.
  if value is None:
    args.append('--unset')
  elif isinstance(value, bool):
    args.append('--bool')
    value = str(value).lower()
  else:
    # `git config` also has --int, but apparently git config suffers from
    # integer overflows (http://crbug.com/640115), so don't use it.
    value = str(value)
  args.append(_git_branch_config_key(branch, key))
  if value is not None:
    args.append(value)
  RunGit(args, **kwargs)


def _get_committer_timestamp(commit):
  """Returns Unix timestamp as integer of a committer in a commit.

  Commit can be whatever git show would recognize, such as HEAD, sha1 or ref.
  """
  # Git also stores timezone offset, but it only affects visual display;
  # actual point in time is defined by this timestamp only.
  return int(RunGit(['show', '-s', '--format=%ct', commit]).strip())


def _git_amend_head(message, committer_timestamp):
  """Amends commit with new message and desired committer_timestamp.

  Sets committer timezone to UTC.
  """
  env = os.environ.copy()
  env['GIT_COMMITTER_DATE'] = '%d+0000' % committer_timestamp
  return RunGit(['commit', '--amend', '-m', message], env=env)


def _get_properties_from_options(options):
  properties = dict(x.split('=', 1) for x in options.properties)
  for key, val in properties.iteritems():
    try:
      properties[key] = json.loads(val)
    except ValueError:
      pass  # If a value couldn't be evaluated, treat it as a string.
  return properties


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
      if content_json is None:
        raise BuildbucketResponseException(
            'Buildbucket returned invalid JSON content: %s.\n'
            'Please file bugs at http://crbug.com, '
            'component "Infra>Platform>Buildbucket".' %
            content)
      return content_json
    if response.status < 500 or try_count >= 2:
      raise httplib2.HttpLib2Error(content)

    # status >= 500 means transient failures.
    logging.debug('Transient errors when %s. Will retry.', operation_name)
    time_sleep(0.5 + (1.5 * try_count))
    try_count += 1
  assert False, 'unreachable'


def _get_bucket_map(changelist, options, option_parser):
  """Returns a dict mapping bucket names to builders and tests,
  for triggering tryjobs.
  """
  # If no bots are listed, we try to get a set of builders and tests based
  # on GetPreferredTryMasters functions in PRESUBMIT.py files.
  if not options.bot:
    change = changelist.GetChange(
        changelist.GetCommonAncestorWithUpstream(), None)
    # Get try masters from PRESUBMIT.py files.
    masters = presubmit_support.DoGetTryMasters(
        change=change,
        changed_files=change.LocalPaths(),
        repository_root=settings.GetRoot(),
        default_presubmit=None,
        project=None,
        verbose=options.verbose,
        output_stream=sys.stdout)
    if masters is None:
      return None
    return {m: b for m, b in masters.iteritems()}

  if options.bucket:
    return {options.bucket: {b: [] for b in options.bot}}
  option_parser.error(
      'Please specify the bucket, e.g. "-B luci.chromium.try".')


def _trigger_try_jobs(auth_config, changelist, buckets, options, patchset):
  """Sends a request to Buildbucket to trigger tryjobs for a changelist.

  Args:
    auth_config: AuthConfig for Buildbucket.
    changelist: Changelist that the tryjobs are associated with.
    buckets: A nested dict mapping bucket names to builders to tests.
    options: Command-line options.
  """
  assert changelist.GetIssue(), 'CL must be uploaded first'
  codereview_url = changelist.GetCodereviewServer()
  assert codereview_url, 'CL must be uploaded first'
  patchset = patchset or changelist.GetMostRecentPatchset()
  assert patchset, 'CL must be uploaded first'

  codereview_host = urlparse.urlparse(codereview_url).hostname
  # Cache the buildbucket credentials under the codereview host key, so that
  # users can use different credentials for different buckets.
  authenticator = auth.get_authenticator_for_host(codereview_host, auth_config)
  http = authenticator.authorize(httplib2.Http())
  http.force_exception_to_status_code = True

  buildbucket_put_url = (
      'https://{hostname}/_ah/api/buildbucket/v1/builds/batch'.format(
          hostname=options.buildbucket_host))
  buildset = 'patch/gerrit/{hostname}/{issue}/{patch}'.format(
      hostname=codereview_host,
      issue=changelist.GetIssue(),
      patch=patchset)

  shared_parameters_properties = changelist.GetTryJobProperties(patchset)
  shared_parameters_properties['category'] = options.category
  if options.clobber:
    shared_parameters_properties['clobber'] = True
  extra_properties = _get_properties_from_options(options)
  if extra_properties:
    shared_parameters_properties.update(extra_properties)

  batch_req_body = {'builds': []}
  print_text = []
  print_text.append('Tried jobs on:')
  for bucket, builders_and_tests in sorted(buckets.iteritems()):
    print_text.append('Bucket: %s' % bucket)
    for builder, tests in sorted(builders_and_tests.iteritems()):
      print_text.append('  %s: %s' % (builder, tests))
      parameters = {
          'builder_name': builder,
          'changes': [{
              'author': {'email': changelist.GetIssueOwner()},
              'revision': options.revision,
          }],
          'properties': shared_parameters_properties.copy(),
      }
      if 'presubmit' in builder.lower():
        parameters['properties']['dry_run'] = 'true'
      if tests:
        parameters['properties']['testfilter'] = tests

      tags = [
          'builder:%s' % builder,
          'buildset:%s' % buildset,
          'user_agent:git_cl_try',
      ]

      batch_req_body['builds'].append(
          {
              'bucket': bucket,
              'parameters_json': json.dumps(parameters),
              'client_operation_id': str(uuid.uuid4()),
              'tags': tags,
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


def fetch_try_jobs(auth_config, changelist, buildbucket_host,
                   patchset=None):
  """Fetches tryjobs from buildbucket.

  Returns a map from build id to build info as a dictionary.
  """
  assert buildbucket_host
  assert changelist.GetIssue(), 'CL must be uploaded first'
  assert changelist.GetCodereviewServer(), 'CL must be uploaded first'
  patchset = patchset or changelist.GetMostRecentPatchset()
  assert patchset, 'CL must be uploaded first'

  codereview_url = changelist.GetCodereviewServer()
  codereview_host = urlparse.urlparse(codereview_url).hostname
  authenticator = auth.get_authenticator_for_host(codereview_host, auth_config)
  if authenticator.has_cached_credentials():
    http = authenticator.authorize(httplib2.Http())
  else:
    print('Warning: Some results might be missing because %s' %
          # Get the message on how to login.
          (auth.LoginRequiredError(codereview_host).message,))
    http = httplib2.Http()

  http.force_exception_to_status_code = True

  buildset = 'patch/gerrit/{hostname}/{issue}/{patch}'.format(
      hostname=codereview_host,
      issue=changelist.GetIssue(),
      patch=patchset)
  params = {'tag': 'buildset:%s' % buildset}

  builds = {}
  while True:
    url = 'https://{hostname}/_ah/api/buildbucket/v1/search?{params}'.format(
        hostname=buildbucket_host,
        params=urllib.urlencode(params))
    content = _buildbucket_retry('fetching tryjobs', http, url, 'GET')
    for build in content.get('builds', []):
      builds[build['id']] = build
    if 'next_cursor' in content:
      params['start_cursor'] = content['next_cursor']
    else:
      break
  return builds


def print_try_jobs(options, builds):
  """Prints nicely result of fetch_try_jobs."""
  if not builds:
    print('No tryjobs scheduled.')
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
        print('WARNING: Failed to get builder name for build %s: %s' % (
              b['id'], error))
        name = None
      builder_names_cache[b['id']] = name
      return name

  if options.print_master:
    name_fmt = '%%-%ds %%-%ds' % (
        max(len(str(b['bucket'])) for b in builds.itervalues()),
        max(len(str(get_builder(b))) for b in builds.itervalues()))
    def get_name(b):
      return name_fmt % (b['bucket'], get_builder(b))
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


def _ComputeDiffLineRanges(files, upstream_commit):
  """Gets the changed line ranges for each file since upstream_commit.

  Parses a git diff on provided files and returns a dict that maps a file name
  to an ordered list of range tuples in the form (start_line, count).
  Ranges are in the same format as a git diff.
  """
  # If files is empty then diff_output will be a full diff.
  if len(files) == 0:
    return {}

  # Take the git diff and find the line ranges where there are changes.
  diff_cmd = BuildGitDiffCmd('-U0', upstream_commit, files, allow_prefix=True)
  diff_output = RunGit(diff_cmd)

  pattern = r'(?:^diff --git a/(?:.*) b/(.*))|(?:^@@.*\+(.*) @@)'
  # 2 capture groups
  # 0 == fname of diff file
  # 1 == 'diff_start,diff_count' or 'diff_start'
  # will match each of
  # diff --git a/foo.foo b/foo.py
  # @@ -12,2 +14,3 @@
  # @@ -12,2 +17 @@
  # running re.findall on the above string with pattern will give
  # [('foo.py', ''), ('', '14,3'), ('', '17')]

  curr_file = None
  line_diffs = {}
  for match in re.findall(pattern, diff_output, flags=re.MULTILINE):
    if match[0] != '':
      # Will match the second filename in diff --git a/a.py b/b.py.
      curr_file = match[0]
      line_diffs[curr_file] = []
    else:
      # Matches +14,3
      if ',' in match[1]:
        diff_start, diff_count = match[1].split(',')
      else:
        # Single line changes are of the form +12 instead of +12,1.
        diff_start = match[1]
        diff_count = 1

      diff_start = int(diff_start)
      diff_count = int(diff_count)

      # If diff_count == 0 this is a removal we can ignore.
      line_diffs[curr_file].append((diff_start, diff_count))

  return line_diffs


def _FindYapfConfigFile(fpath, yapf_config_cache, top_dir=None):
  """Checks if a yapf file is in any parent directory of fpath until top_dir.

  Recursively checks parent directories to find yapf file and if no yapf file
  is found returns None. Uses yapf_config_cache as a cache for previously found
  configs.
  """
  fpath = os.path.abspath(fpath)
  # Return result if we've already computed it.
  if fpath in yapf_config_cache:
    return yapf_config_cache[fpath]

  parent_dir = os.path.dirname(fpath)
  if os.path.isfile(fpath):
    ret = _FindYapfConfigFile(parent_dir, yapf_config_cache, top_dir)
  else:
    # Otherwise fpath is a directory
    yapf_file = os.path.join(fpath, YAPF_CONFIG_FILENAME)
    if os.path.isfile(yapf_file):
      ret = yapf_file
    elif fpath == top_dir or parent_dir == fpath:
      # If we're at the top level directory, or if we're at root
      # there is no provided style.
      ret = None
    else:
      # Otherwise recurse on the current directory.
      ret = _FindYapfConfigFile(parent_dir, yapf_config_cache, top_dir)
  yapf_config_cache[fpath] = ret
  return ret


def write_try_results_json(output_file, builds):
  """Writes a subset of the data from fetch_try_jobs to a file as JSON.

  The input |builds| dict is assumed to be generated by Buildbucket.
  Buildbucket documentation: http://goo.gl/G0s101
  """

  def convert_build_dict(build):
    """Extracts some of the information from one build dict."""
    parameters = json.loads(build.get('parameters_json', '{}')) or {}
    return {
        'buildbucket_id': build.get('id'),
        'bucket': build.get('bucket'),
        'builder_name': parameters.get('builder_name'),
        'created_ts': build.get('created_ts'),
        'experimental': build.get('experimental'),
        'failure_reason': build.get('failure_reason'),
        'result': build.get('result'),
        'status': build.get('status'),
        'tags': build.get('tags'),
        'url': build.get('url'),
    }

  converted = []
  for _, build in sorted(builds.items()):
    converted.append(convert_build_dict(build))
  write_json(output_file, converted)


def print_stats(args):
  """Prints statistics about the change to the user."""
  # --no-ext-diff is broken in some versions of Git, so try to work around
  # this by overriding the environment (but there is still a problem if the
  # git config key "diff.external" is used).
  env = GetNoGitPagerEnv()
  if 'GIT_EXTERNAL_DIFF' in env:
    del env['GIT_EXTERNAL_DIFF']

  try:
    stdout = sys.stdout.fileno()
  except AttributeError:
    stdout = None
  return subprocess2.call(
      ['git', 'diff', '--no-ext-diff', '--stat', '-l100000', '-C50'] + args,
      stdout=stdout, env=env)


class BuildbucketResponseException(Exception):
  pass


class Settings(object):
  def __init__(self):
    self.cc = None
    self.root = None
    self.tree_status_url = None
    self.viewvc_url = None
    self.updated = False
    self.is_gerrit = None
    self.squash_gerrit_uploads = None
    self.gerrit_skip_ensure_authenticated = None
    self.git_editor = None

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

  @staticmethod
  def GetRelativeRoot():
    return RunGit(['rev-parse', '--show-cdup']).strip()

  def GetRoot(self):
    if self.root is None:
      self.root = os.path.abspath(self.GetRelativeRoot())
    return self.root

  def GetTreeStatusUrl(self, error_ok=False):
    if not self.tree_status_url:
      error_message = ('You must configure your tree status URL by running '
                       '"git cl config".')
      self.tree_status_url = self._GetConfig(
          'rietveld.tree-status-url', error_ok=error_ok,
          error_message=error_message)
    return self.tree_status_url

  def GetViewVCUrl(self):
    if not self.viewvc_url:
      self.viewvc_url = self._GetConfig('rietveld.viewvc-url', error_ok=True)
    return self.viewvc_url

  def GetBugPrefix(self):
    return self._GetConfig('rietveld.bug-prefix', error_ok=True)

  def GetRunPostUploadHook(self):
    run_post_upload_hook = self._GetConfig(
        'rietveld.run-post-upload-hook', error_ok=True)
    return run_post_upload_hook == "True"

  def GetDefaultCCList(self):
    return self._GetConfig('rietveld.cc', error_ok=True)

  def GetIsGerrit(self):
    """Returns True if this repo is associated with Gerrit."""
    if self.is_gerrit is None:
      self.is_gerrit = (
          self._GetConfig('gerrit.host', error_ok=True).lower() == 'true')
    return self.is_gerrit

  def GetSquashGerritUploads(self):
    """Returns True if uploads to Gerrit should be squashed by default."""
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
    """Returns the editor specified in the git config, or None if none is."""
    if self.git_editor is None:
      # Git requires single quotes for paths with spaces. We need to replace
      # them with double quotes for Windows to treat such paths as a single
      # path.
      self.git_editor = self._GetConfig(
          'core.editor', error_ok=True).replace('\'', '"')
    return self.git_editor or None

  def GetLintRegex(self):
    return (self._GetConfig('rietveld.cpplint-regex', error_ok=True) or
            DEFAULT_LINT_REGEX)

  def GetLintIgnoreRegex(self):
    return (self._GetConfig('rietveld.cpplint-ignore-regex', error_ok=True) or
            DEFAULT_LINT_IGNORE_REGEX)

  def _GetConfig(self, param, **kwargs):
    self.LazyUpdateIfNeeded()
    return RunGit(['config', param], **kwargs).strip()


@contextlib.contextmanager
def _get_gerrit_project_config_file(remote_url):
  """Context manager to fetch and store Gerrit's project.config from
  refs/meta/config branch and store it in temp file.

  Provides a temporary filename or None if there was error.
  """
  error, _ = RunGitWithCode([
      'fetch', remote_url,
      '+refs/meta/config:refs/git_cl/meta/config'])
  if error:
    # Ref doesn't exist or isn't accessible to current user.
    print('WARNING: Failed to fetch project config for %s: %s' %
          (remote_url, error))
    yield None
    return

  error, project_config_data = RunGitWithCode(
      ['show', 'refs/git_cl/meta/config:project.config'])
  if error:
    print('WARNING: project.config file not found')
    yield None
    return

  with gclient_utils.temporary_directory() as tempdir:
    project_config_file = os.path.join(tempdir, 'project.config')
    gclient_utils.FileWrite(project_config_file, project_config_data)
    yield project_config_file


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
  """Enum for states of CL with respect to CQ."""
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

  return Changelist.ParseIssueURL(parsed_url) or fail_result


def _create_description_from_log(args):
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


class GerritChangeNotExists(Exception):
  def __init__(self, issue, url):
    self.issue = issue
    self.url = url
    super(GerritChangeNotExists, self).__init__()

  def __str__(self):
    return 'change %s at %s does not exist or you have no access to it' % (
        self.issue, self.url)


_CommentSummary = collections.namedtuple(
    '_CommentSummary', ['date', 'message', 'sender', 'autogenerated',
                        # TODO(tandrii): these two aren't known in Gerrit.
                        'approval', 'disapproval'])


class Changelist(object):
  """Changelist works with one changelist in local branch.

  Notes:
    * Not safe for concurrent multi-{thread,process} use.
    * Caches values from current branch. Therefore, re-use after branch change
      with great care.
  """

  def __init__(self, branchref=None, issue=None, codereview_host=None):
    """Create a new ChangeList instance.

    **kwargs will be passed directly to Gerrit implementation.
    """
    # Poke settings so we get the "configure your server" message if necessary.
    global settings
    if not settings:
      # Happens when git_cl.py is used as a utility library.
      settings = Settings()

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
    self.more_cc = []
    self._remote = None
    self._cached_remote_url = (False, None)  # (is_cached, value)

    self._change_id = None
    # Lazily cached values.
    self._gerrit_host = None    # e.g. chromium-review.googlesource.com
    self._gerrit_server = None  # e.g. https://chromium-review.googlesource.com
    # Map from change number (issue) to its detail cache.
    self._detail_cache = {}

    if codereview_host is not None:
      assert not codereview_host.startswith('https://'), codereview_host
      self._gerrit_host = codereview_host
      self._gerrit_server = 'https://%s' % codereview_host

  def GetCCList(self):
    """Returns the users cc'd on this CL.

    The return value is a string suitable for passing to git cl with the --cc
    flag.
    """
    if self.cc is None:
      base_cc = settings.GetDefaultCCList()
      more_cc = ','.join(self.more_cc)
      self.cc = ','.join(filter(None, (base_cc, more_cc))) or ''
    return self.cc

  def GetCCListWithoutDefault(self):
    """Return the users cc'd on this CL excluding default ones."""
    if self.cc is None:
      self.cc = ','.join(self.more_cc)
    return self.cc

  def ExtendCC(self, more_cc):
    """Extends the list of users to cc on this CL based on the changed files."""
    self.more_cc.extend(more_cc)

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

  def _GitGetBranchConfigValue(self, key, default=None, **kwargs):
    assert 'branch' not in kwargs, 'this CL branch is used automatically'
    kwargs['branch'] = self.GetBranch()
    return _git_get_branch_config_value(key, default, **kwargs)

  def _GitSetBranchConfigValue(self, key, value, **kwargs):
    assert 'branch' not in kwargs, 'this CL branch is used automatically'
    assert self.GetBranch(), (
        'this CL must have an associated branch to %sset %s%s' %
          ('un' if value is None else '',
           key,
           '' if value is None else ' to %r' % value))
    kwargs['branch'] = self.GetBranch()
    return _git_set_branch_config_value(key, value, **kwargs)

  @staticmethod
  def FetchUpstreamTuple(branch):
    """Returns a tuple containing remote and remote ref,
       e.g. 'origin', 'refs/heads/master'
    """
    remote = '.'
    upstream_branch = _git_get_branch_config_value('merge', branch=branch)

    if upstream_branch:
      remote = _git_get_branch_config_value('remote', branch=branch)
    else:
      upstream_branch = RunGit(['config', 'rietveld.upstream-branch'],
                               error_ok=True).strip()
      if upstream_branch:
        remote = RunGit(['config', 'rietveld.upstream-remote']).strip()
      else:
        # Else, try to guess the origin remote.
        remote_branches = RunGit(['branch', '-r']).split()
        if 'origin/master' in remote_branches:
          # Fall back on origin/master if it exits.
          remote = 'origin'
          upstream_branch = 'refs/heads/master'
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
      if remote != '.':
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
          logging.warn('Could not determine which remote this change is '
                       'associated with, so defaulting to "%s".' % self._remote)
        else:
          logging.warn('Could not determine which remote this change is '
                       'associated with.')
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
        print('ERROR: Unable to determine current branch (detached HEAD?)',
              file=sys.stderr)
      else:
        print('ERROR: No upstream branch.', file=sys.stderr)
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
    return self._GitGetBranchConfigValue('base-url')

  def GetRemoteUrl(self):
    """Return the configured remote URL, e.g. 'git://example.org/foo.git/'.

    Returns None if there is no remote.
    """
    is_cached, value = self._cached_remote_url
    if is_cached:
      return value

    remote, _ = self.GetRemoteBranch()
    url = RunGit(['config', 'remote.%s.url' % remote], error_ok=True).strip()

    # Check if the remote url can be parsed as an URL.
    host = urlparse.urlparse(url).netloc
    if host:
      self._cached_remote_url = (True, url)
      return url

    # If it cannot be parsed as an url, assume it is a local directory,
    # probably a git cache.
    logging.warning('"%s" doesn\'t appear to point to a git host. '
                    'Interpreting it as a local directory.', url)
    if not os.path.isdir(url):
      logging.error(
          'Remote "%s" for branch "%s" points to "%s", but it doesn\'t exist.',
          remote, self.GetBranch(), url)
      return None

    cache_path = url
    url = RunGit(['config', 'remote.%s.url' % remote],
                 error_ok=True,
                 cwd=url).strip()

    host = urlparse.urlparse(url).netloc
    if not host:
      logging.error(
          'Remote "%(remote)s" for branch "%(branch)s" points to '
          '"%(cache_path)s", but it is misconfigured.\n'
          '"%(cache_path)s" must be a git repo and must have a remote named '
          '"%(remote)s" pointing to the git host.', {
              'remote': remote,
              'cache_path': cache_path,
              'branch': self.GetBranch()})
      return None

    self._cached_remote_url = (True, url)
    return url

  def GetIssue(self):
    """Returns the issue number as a int or None if not set."""
    if self.issue is None and not self.lookedup_issue:
      self.issue = self._GitGetBranchConfigValue(
          self.IssueConfigKey(), value_type=int)
      self.lookedup_issue = True
    return self.issue

  def GetIssueURL(self):
    """Get the URL for a particular issue."""
    issue = self.GetIssue()
    if not issue:
      return None
    return '%s/%s' % (self.GetCodereviewServer(), issue)

  def GetDescription(self, pretty=False, force=False):
    if not self.has_description or force:
      if self.GetIssue():
        self.description = self.FetchDescription(force=force)
      self.has_description = True
    if pretty:
      # Set width to 72 columns + 2 space indent.
      wrapper = textwrap.TextWrapper(width=74, replace_whitespace=True)
      wrapper.initial_indent = wrapper.subsequent_indent = '  '
      lines = self.description.splitlines()
      return '\n'.join([wrapper.fill(line) for line in lines])
    return self.description

  def GetDescriptionFooters(self):
    """Returns (non_footer_lines, footers) for the commit message.

    Returns:
      non_footer_lines (list(str)) - Simple list of description lines without
        any footer. The lines do not contain newlines, nor does the list contain
        the empty line between the message and the footers.
      footers (list(tuple(KEY, VALUE))) - List of parsed footers, e.g.
        [("Change-Id", "Ideadbeef...."), ...]
    """
    raw_description = self.GetDescription()
    msg_lines, _, footers = git_footers.split_footers(raw_description)
    if footers:
      msg_lines = msg_lines[:len(msg_lines) - 1]
    return msg_lines, footers

  def GetPatchset(self):
    """Returns the patchset number as a int or None if not set."""
    if self.patchset is None and not self.lookedup_patchset:
      self.patchset = self._GitGetBranchConfigValue(
          self.PatchsetConfigKey(), value_type=int)
      self.lookedup_patchset = True
    return self.patchset

  def SetPatchset(self, patchset):
    """Set this branch's patchset. If patchset=0, clears the patchset."""
    assert self.GetBranch()
    if not patchset:
      self.patchset = None
    else:
      self.patchset = int(patchset)
    self._GitSetBranchConfigValue(
        self.PatchsetConfigKey(), self.patchset)

  def SetIssue(self, issue=None):
    """Set this branch's issue. If issue isn't given, clears the issue."""
    assert self.GetBranch()
    if issue:
      issue = int(issue)
      self._GitSetBranchConfigValue(
          self.IssueConfigKey(), issue)
      self.issue = issue
      codereview_server = self.GetCodereviewServer()
      if codereview_server:
        self._GitSetBranchConfigValue(
            self.CodereviewServerConfigKey(),
            codereview_server)
    else:
      # Reset all of these just to be clean.
      reset_suffixes = [
          'last-upload-hash',
          self.IssueConfigKey(),
          self.PatchsetConfigKey(),
          self.CodereviewServerConfigKey(),
      ] + self._PostUnsetIssueProperties()
      for prop in reset_suffixes:
        self._GitSetBranchConfigValue(prop, None, error_ok=True)
      msg = RunGit(['log', '-1', '--format=%B']).strip()
      if msg and git_footers.get_footer_change_id(msg):
        print('WARNING: The change patched into this branch has a Change-Id. '
              'Removing it.')
        RunGit(['commit', '--amend', '-m',
                git_footers.remove_footer(msg, 'Change-Id')])
      self.lookedup_issue = True
      self.issue = None
      self.patchset = None

  def GetChange(self, upstream_branch, author, local_description=False):
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
           '    git branch --set-upstream-to origin/master %s\n'
           'or replace origin/master with the relevant branch') %
          (upstream_branch, self.GetBranch()))

    issue = self.GetIssue()
    patchset = self.GetPatchset()
    if issue and not local_description:
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

  def UpdateDescription(self, description, force=False):
    self.UpdateDescriptionRemote(description, force=force)
    self.description = description
    self.has_description = True

  def UpdateDescriptionFooters(self, description_lines, footers, force=False):
    """Sets the description for this CL remotely.

    You can get description_lines and footers with GetDescriptionFooters.

    Args:
      description_lines (list(str)) - List of CL description lines without
        newline characters.
      footers (list(tuple(KEY, VALUE))) - List of footers, as returned by
        GetDescriptionFooters. Key must conform to the git footers format (i.e.
        `List-Of-Tokens`). It will be case-normalized so that each token is
        title-cased.
    """
    new_description = '\n'.join(description_lines)
    if footers:
      new_description += '\n'
      for k, v in footers:
        foot = '%s: %s' % (git_footers.normalize_name(k), v)
        if not git_footers.FOOTER_PATTERN.match(foot):
          raise ValueError('Invalid footer %r' % foot)
        new_description += foot + '\n'
    self.UpdateDescription(new_description, force)

  def RunHook(self, committing, may_prompt, verbose, change, parallel):
    """Calls sys.exit() if the hook fails; returns a HookResults otherwise."""
    try:
      start = time_time()
      result = presubmit_support.DoPresubmitChecks(change, committing,
          verbose=verbose, output_stream=sys.stdout, input_stream=sys.stdin,
          default_presubmit=None, may_prompt=may_prompt,
          gerrit_obj=self.GetGerritObjForPresubmit(),
          parallel=parallel)
      metrics.collector.add_repeated('sub_commands', {
        'command': 'presubmit',
        'execution_time': time_time() - start,
        'exit_code': 0 if result.should_continue() else 1,
      })
      return result
    except presubmit_support.PresubmitFailure as e:
      DieWithError('%s\nMaybe your depot_tools is out of date?' % e)

  def CMDPatchIssue(self, issue_arg, nocommit):
    """Fetches and applies the issue patch from codereview to local branch."""
    if isinstance(issue_arg, (int, long)) or issue_arg.isdigit():
      parsed_issue_arg = _ParsedIssueNumberArgument(int(issue_arg))
    else:
      # Assume url.
      parsed_issue_arg = self.ParseIssueURL(
          urlparse.urlparse(issue_arg))
    if not parsed_issue_arg or not parsed_issue_arg.valid:
      DieWithError('Failed to parse issue argument "%s". '
                   'Must be an issue number or a valid URL.' % issue_arg)
    return self.CMDPatchWithParsedIssue(
        parsed_issue_arg, nocommit, False)

  def CMDUpload(self, options, git_diff_args, orig_args):
    """Uploads a change to codereview."""
    custom_cl_base = None
    if git_diff_args:
      custom_cl_base = base_branch = git_diff_args[0]
    else:
      if self.GetBranch() is None:
        DieWithError('Can\'t upload from detached HEAD state. Get on a branch!')

      # Default to diffing against common ancestor of upstream branch
      base_branch = self.GetCommonAncestorWithUpstream()
      git_diff_args = [base_branch, 'HEAD']

    # Fast best-effort checks to abort before running potentially expensive
    # hooks if uploading is likely to fail anyway. Passing these checks does
    # not guarantee that uploading will not fail.
    self.EnsureAuthenticated(force=options.force)
    self.EnsureCanUploadPatchset(force=options.force)

    # Apply watchlists on upload.
    change = self.GetChange(base_branch, None)
    watchlist = watchlists.Watchlists(change.RepositoryRoot())
    files = [f.LocalPath() for f in change.AffectedFiles()]
    if not options.bypass_watchlists:
      self.ExtendCC(watchlist.GetWatchersForPaths(files))

    if not options.bypass_hooks:
      if options.reviewers or options.tbrs or options.add_owners_to:
        # Set the reviewer list now so that presubmit checks can access it.
        change_description = ChangeDescription(change.FullDescriptionText())
        change_description.update_reviewers(options.reviewers,
                                            options.tbrs,
                                            options.add_owners_to,
                                            change)
        change.SetDescriptionText(change_description.description)
      hook_results = self.RunHook(committing=False,
                                  may_prompt=not options.force,
                                  verbose=options.verbose,
                                  change=change, parallel=options.parallel)
      if not hook_results.should_continue():
        return 1
      if not options.reviewers and hook_results.reviewers:
        options.reviewers = hook_results.reviewers.split(',')
      self.ExtendCC(hook_results.more_cc)

    print_stats(git_diff_args)
    ret = self.CMDUploadChange(options, git_diff_args, custom_cl_base, change)
    if not ret:
      _git_set_branch_config_value('last-upload-hash',
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
    """Updates the CQ state for the latest patchset.

    Issue must have been already uploaded and known.
    """
    assert new_state in _CQState.ALL_STATES
    assert self.GetIssue()
    try:
      vote_map = {
        _CQState.NONE: 0,
        _CQState.DRY_RUN: 1,
        _CQState.COMMIT: 2,
      }
      labels = {'Commit-Queue': vote_map[new_state]}
      notify = False if new_state == _CQState.DRY_RUN else None
      gerrit_util.SetReview(
          self._GetGerritHost(), self._GerritChangeIdentifier(),
          labels=labels, notify=notify)
      return 0
    except KeyboardInterrupt:
      raise
    except:
      print('WARNING: Failed to %s.\n'
            'Either:\n'
            ' * Your project has no CQ,\n'
            ' * You don\'t have permission to change the CQ state,\n'
            ' * There\'s a bug in this code (see stack trace below).\n'
            'Consider specifying which bots to trigger manually or asking your '
            'project owners for permissions or contacting Chrome Infra at:\n'
            'https://www.chromium.org/infra\n\n' %
            ('cancel CQ' if new_state == _CQState.NONE else 'trigger CQ'))
      # Still raise exception so that stack trace is printed.
      raise

  def _GetGerritHost(self):
    # Lazy load of configs.
    self.GetCodereviewServer()
    if self._gerrit_host and '.' not in self._gerrit_host:
      # Abbreviated domain like "chromium" instead of chromium.googlesource.com.
      # This happens for internal stuff http://crbug.com/614312.
      parsed = urlparse.urlparse(self.GetRemoteUrl())
      if parsed.scheme == 'sso':
        print('WARNING: using non-https URLs for remote is likely broken\n'
              '  Your current remote is: %s' % self.GetRemoteUrl())
        self._gerrit_host = '%s.googlesource.com' % self._gerrit_host
        self._gerrit_server = 'https://%s' % self._gerrit_host
    return self._gerrit_host

  def _GetGitHost(self):
    """Returns git host to be used when uploading change to Gerrit."""
    remote_url = self.GetRemoteUrl()
    if not remote_url:
      return None
    return urlparse.urlparse(remote_url).netloc

  def GetCodereviewServer(self):
    if not self._gerrit_server:
      # If we're on a branch then get the server potentially associated
      # with that branch.
      if self.GetIssue():
        self._gerrit_server = self._GitGetBranchConfigValue(
            self.CodereviewServerConfigKey())
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

  def _GetGerritProject(self):
    """Returns Gerrit project name based on remote git URL."""
    remote_url = self.GetRemoteUrl()
    if remote_url is None:
      logging.warn('can\'t detect Gerrit project.')
      return None
    project = urlparse.urlparse(remote_url).path.strip('/')
    if project.endswith('.git'):
      project = project[:-len('.git')]
    # *.googlesource.com hosts ensure that Git/Gerrit projects don't start with
    # 'a/' prefix, because 'a/' prefix is used to force authentication in
    # gitiles/git-over-https protocol. E.g.,
    # https://chromium.googlesource.com/a/v8/v8 refers to the same repo/project
    # as
    # https://chromium.googlesource.com/v8/v8
    if project.startswith('a/'):
      project = project[len('a/'):]
    return project

  def _GerritChangeIdentifier(self):
    """Handy method for gerrit_util.ChangeIdentifier for a given CL.

    Not to be confused by value of "Change-Id:" footer.
    If Gerrit project can be determined, this will speed up Gerrit HTTP API RPC.
    """
    project = self._GetGerritProject()
    if project:
      return gerrit_util.ChangeIdentifier(project, self.GetIssue())
    # Fall back on still unique, but less efficient change number.
    return str(self.GetIssue())

  @classmethod
  def IssueConfigKey(cls):
    return 'gerritissue'

  @classmethod
  def PatchsetConfigKey(cls):
    return 'gerritpatchset'

  @classmethod
  def CodereviewServerConfigKey(cls):
    return 'gerritserver'

  def EnsureAuthenticated(self, force, refresh=None):
    """Best effort check that user is authenticated with Gerrit server."""
    if settings.GetGerritSkipEnsureAuthenticated():
      # For projects with unusual authentication schemes.
      # See http://crbug.com/603378.
      return

    # Check presence of cookies only if using cookies-based auth method.
    cookie_auth = gerrit_util.Authenticator.get()
    if not isinstance(cookie_auth, gerrit_util.CookiesAuthenticator):
      return

    if urlparse.urlparse(self.GetRemoteUrl()).scheme != 'https':
      print('WARNING: Ignoring branch %s with non-https remote %s' %
            (self.branch, self.GetRemoteUrl()))
      return

    # Lazy-loader to identify Gerrit and Git hosts.
    self.GetCodereviewServer()
    git_host = self._GetGitHost()
    assert self._gerrit_server and self._gerrit_host and git_host

    gerrit_auth = cookie_auth.get_auth_header(self._gerrit_host)
    git_auth = cookie_auth.get_auth_header(git_host)
    if gerrit_auth and git_auth:
      if gerrit_auth == git_auth:
        return
      all_gsrc = cookie_auth.get_auth_header('d0esN0tEx1st.googlesource.com')
      print(
          'WARNING: You have different credentials for Gerrit and git hosts:\n'
          '           %s\n'
          '           %s\n'
          '        Consider running the following command:\n'
          '          git cl creds-check\n'
          '        %s\n'
          '        %s' %
          (git_host, self._gerrit_host,
           ('Hint: delete creds for .googlesource.com' if all_gsrc else ''),
           cookie_auth.get_new_password_message(git_host)))
      if not force:
        confirm_or_exit('If you know what you are doing', action='continue')
      return
    else:
      missing = (
          ([] if gerrit_auth else [self._gerrit_host]) +
          ([] if git_auth else [git_host]))
      DieWithError('Credentials for the following hosts are required:\n'
                   '  %s\n'
                   'These are read from %s (or legacy %s)\n'
                   '%s' % (
                     '\n  '.join(missing),
                     cookie_auth.get_gitcookies_path(),
                     cookie_auth.get_netrc_path(),
                     cookie_auth.get_new_password_message(git_host)))

  def EnsureCanUploadPatchset(self, force):
    if not self.GetIssue():
      return

    # Warm change details cache now to avoid RPCs later, reducing latency for
    # developers.
    self._GetChangeDetail(
        ['DETAILED_ACCOUNTS', 'CURRENT_REVISION', 'CURRENT_COMMIT', 'LABELS'])

    status = self._GetChangeDetail()['status']
    if status in ('MERGED', 'ABANDONED'):
      DieWithError('Change %s has been %s, new uploads are not allowed' %
                   (self.GetIssueURL(),
                    'submitted' if status == 'MERGED' else 'abandoned'))

    # TODO(vadimsh): For some reason the chunk of code below was skipped if
    # 'is_gce' is True. I'm just refactoring it to be 'skip if not cookies'.
    # Apparently this check is not very important? Otherwise get_auth_email
    # could have been added to other implementations of Authenticator.
    cookies_auth = gerrit_util.Authenticator.get()
    if not isinstance(cookies_auth, gerrit_util.CookiesAuthenticator):
      return

    cookies_user = cookies_auth.get_auth_email(self._GetGerritHost())
    if self.GetIssueOwner() == cookies_user:
      return
    logging.debug('change %s owner is %s, cookies user is %s',
                  self.GetIssue(), self.GetIssueOwner(), cookies_user)
    # Maybe user has linked accounts or something like that,
    # so ask what Gerrit thinks of this user.
    details = gerrit_util.GetAccountDetails(self._GetGerritHost(), 'self')
    if details['email'] == self.GetIssueOwner():
      return
    if not force:
      print('WARNING: Change %s is owned by %s, but you authenticate to Gerrit '
            'as %s.\n'
            'Uploading may fail due to lack of permissions.' %
            (self.GetIssue(), self.GetIssueOwner(), details['email']))
      confirm_or_exit(action='upload')

  def _PostUnsetIssueProperties(self):
    """Which branch-specific properties to erase when unsetting issue."""
    return ['gerritsquashhash']

  def GetGerritObjForPresubmit(self):
    return presubmit_support.GerritAccessor(self._GetGerritHost())

  def GetStatus(self):
    """Applies a rough heuristic to give a simple summary of an issue's review
    or CQ status, assuming adherence to a common workflow.

    Returns None if no issue for this branch, or one of the following keywords:
      * 'error'   - error from review tool (including deleted issues)
      * 'unsent'  - no reviewers added
      * 'waiting' - waiting for review
      * 'reply'   - waiting for uploader to reply to review
      * 'lgtm'    - Code-Review label has been set
      * 'dry-run' - dry-running in the CQ
      * 'commit'  - in the CQ
      * 'closed'  - successfully submitted or abandoned
    """
    if not self.GetIssue():
      return None

    try:
      data = self._GetChangeDetail([
          'DETAILED_LABELS', 'CURRENT_REVISION', 'SUBMITTABLE'])
    except (httplib.HTTPException, GerritChangeNotExists):
      return 'error'

    if data['status'] in ('ABANDONED', 'MERGED'):
      return 'closed'

    cq_label = data['labels'].get('Commit-Queue', {})
    max_cq_vote = 0
    for vote in cq_label.get('all', []):
      max_cq_vote = max(max_cq_vote, vote.get('value', 0))
    if max_cq_vote == 2:
      return 'commit'
    if max_cq_vote == 1:
      return 'dry-run'

    if data['labels'].get('Code-Review', {}).get('approved'):
      return 'lgtm'

    if not data.get('reviewers', {}).get('REVIEWER', []):
      return 'unsent'

    owner = data['owner'].get('_account_id')
    messages = sorted(data.get('messages', []), key=lambda m: m.get('updated'))
    last_message_author = messages.pop().get('author', {})
    while last_message_author:
      if last_message_author.get('email') == COMMIT_BOT_EMAIL:
        # Ignore replies from CQ.
        last_message_author = messages.pop().get('author', {})
        continue
      if last_message_author.get('_account_id') == owner:
        # Most recent message was by owner.
        return 'waiting'
      else:
        # Some reply from non-owner.
        return 'reply'

    # Somehow there are no messages even though there are reviewers.
    return 'unsent'

  def GetMostRecentPatchset(self):
    data = self._GetChangeDetail(['CURRENT_REVISION'])
    patchset = data['revisions'][data['current_revision']]['_number']
    self.SetPatchset(patchset)
    return patchset

  def FetchDescription(self, force=False):
    data = self._GetChangeDetail(['CURRENT_REVISION', 'CURRENT_COMMIT'],
                                 no_cache=force)
    current_rev = data['current_revision']
    return data['revisions'][current_rev]['commit']['message'].encode(
        'utf-8', 'ignore')

  def UpdateDescriptionRemote(self, description, force=False):
    if gerrit_util.HasPendingChangeEdit(
        self._GetGerritHost(), self._GerritChangeIdentifier()):
      if not force:
        confirm_or_exit(
            'The description cannot be modified while the issue has a pending '
            'unpublished edit. Either publish the edit in the Gerrit web UI '
            'or delete it.\n\n', action='delete the unpublished edit')

      gerrit_util.DeletePendingChangeEdit(
        self._GetGerritHost(), self._GerritChangeIdentifier())
    gerrit_util.SetCommitMessage(
        self._GetGerritHost(), self._GerritChangeIdentifier(),
        description, notify='NONE')

  def AddComment(self, message, publish=None):
    gerrit_util.SetReview(
        self._GetGerritHost(), self._GerritChangeIdentifier(),
        msg=message, ready=publish)

  def GetCommentsSummary(self, readable=True):
    # DETAILED_ACCOUNTS is to get emails in accounts.
    # CURRENT_REVISION is included to get the latest patchset so that
    # only the robot comments from the latest patchset can be shown.
    messages = self._GetChangeDetail(
        options=['MESSAGES', 'DETAILED_ACCOUNTS',
                 'CURRENT_REVISION']).get('messages', [])
    file_comments = gerrit_util.GetChangeComments(
        self._GetGerritHost(), self._GerritChangeIdentifier())
    robot_file_comments = gerrit_util.GetChangeRobotComments(
        self._GetGerritHost(), self._GerritChangeIdentifier())

    # Add the robot comments onto the list of comments, but only
    # keep those that are from the latest pachset.
    latest_patch_set = self.GetMostRecentPatchset()
    for path, robot_comments in robot_file_comments.iteritems():
      line_comments = file_comments.setdefault(path, [])
      line_comments.extend(
          [c for c in robot_comments if c['patch_set'] == latest_patch_set])

    # Build dictionary of file comments for easy access and sorting later.
    # {author+date: {path: {patchset: {line: url+message}}}}
    comments = collections.defaultdict(
        lambda: collections.defaultdict(lambda: collections.defaultdict(dict)))
    for path, line_comments in file_comments.iteritems():
      for comment in line_comments:
        tag = comment.get('tag', '')
        if tag.startswith('autogenerated') and 'robot_id' not in comment:
          continue
        key = (comment['author']['email'], comment['updated'])
        if comment.get('side', 'REVISION') == 'PARENT':
          patchset = 'Base'
        else:
          patchset = 'PS%d' % comment['patch_set']
        line = comment.get('line', 0)
        url = ('https://%s/c/%s/%s/%s#%s%s' %
            (self._GetGerritHost(), self.GetIssue(), comment['patch_set'], path,
             'b' if comment.get('side') == 'PARENT' else '',
             str(line) if line else ''))
        comments[key][path][patchset][line] = (url, comment['message'])

    summaries = []
    for msg in messages:
      summary = self._BuildCommentSummary(msg, comments, readable)
      if summary:
        summaries.append(summary)
    return summaries

  @staticmethod
  def _BuildCommentSummary(msg, comments, readable):
    key = (msg['author']['email'], msg['date'])
    # Don't bother showing autogenerated messages that don't have associated
    # file or line comments. this will filter out most autogenerated
    # messages, but will keep robot comments like those from Tricium.
    is_autogenerated = msg.get('tag', '').startswith('autogenerated')
    if is_autogenerated and not comments.get(key):
      return None
    message = msg['message']
    # Gerrit spits out nanoseconds.
    assert len(msg['date'].split('.')[-1]) == 9
    date = datetime.datetime.strptime(msg['date'][:-3],
                                      '%Y-%m-%d %H:%M:%S.%f')
    if key in comments:
      message += '\n'
    for path, patchsets in sorted(comments.get(key, {}).items()):
      if readable:
        message += '\n%s' % path
      for patchset, lines in sorted(patchsets.items()):
        for line, (url, content) in sorted(lines.items()):
          if line:
            line_str = 'Line %d' % line
            path_str = '%s:%d:' % (path, line)
          else:
            line_str = 'File comment'
            path_str = '%s:0:' % path
          if readable:
            message += '\n  %s, %s: %s' % (patchset, line_str, url)
            message += '\n  %s\n' % content
          else:
            message += '\n%s ' % path_str
            message += '\n%s\n' % content

    return _CommentSummary(
      date=date,
      message=message,
      sender=msg['author']['email'],
      autogenerated=is_autogenerated,
      # These could be inferred from the text messages and correlated with
      # Code-Review label maximum, however this is not reliable.
      # Leaving as is until the need arises.
      approval=False,
      disapproval=False,
    )

  def CloseIssue(self):
    gerrit_util.AbandonChange(
        self._GetGerritHost(), self._GerritChangeIdentifier(), msg='')

  def SubmitIssue(self, wait_for_merge=True):
    gerrit_util.SubmitChange(
        self._GetGerritHost(), self._GerritChangeIdentifier(),
        wait_for_merge=wait_for_merge)

  def _GetChangeDetail(self, options=None, no_cache=False):
    """Returns details of associated Gerrit change and caching results.

    If fresh data is needed, set no_cache=True which will clear cache and
    thus new data will be fetched from Gerrit.
    """
    options = options or []
    assert self.GetIssue(), 'issue is required to query Gerrit'

    # Optimization to avoid multiple RPCs:
    if (('CURRENT_REVISION' in options or 'ALL_REVISIONS' in options) and
        'CURRENT_COMMIT' not in options):
      options.append('CURRENT_COMMIT')

    # Normalize issue and options for consistent keys in cache.
    cache_key = str(self.GetIssue())
    options = [o.upper() for o in options]

    # Check in cache first unless no_cache is True.
    if no_cache:
      self._detail_cache.pop(cache_key, None)
    else:
      options_set = frozenset(options)
      for cached_options_set, data in self._detail_cache.get(cache_key, []):
        # Assumption: data fetched before with extra options is suitable
        # for return for a smaller set of options.
        # For example, if we cached data for
        #     options=[CURRENT_REVISION, DETAILED_FOOTERS]
        #   and request is for options=[CURRENT_REVISION],
        # THEN we can return prior cached data.
        if options_set.issubset(cached_options_set):
          return data

    try:
      data = gerrit_util.GetChangeDetail(
          self._GetGerritHost(), self._GerritChangeIdentifier(), options)
    except gerrit_util.GerritError as e:
      if e.http_status == 404:
        raise GerritChangeNotExists(self.GetIssue(), self.GetCodereviewServer())
      raise

    self._detail_cache.setdefault(cache_key, []).append(
        (frozenset(options), data))
    return data

  def _GetChangeCommit(self):
    assert self.GetIssue(), 'issue must be set to query Gerrit'
    try:
      data = gerrit_util.GetChangeCommit(
          self._GetGerritHost(), self._GerritChangeIdentifier())
    except gerrit_util.GerritError as e:
      if e.http_status == 404:
        raise GerritChangeNotExists(self.GetIssue(), self.GetCodereviewServer())
      raise
    return data

  def _IsCqConfigured(self):
    detail = self._GetChangeDetail(['LABELS'])
    if u'Commit-Queue' not in detail.get('labels', {}):
      return False
    # TODO(crbug/753213): Remove temporary hack
    if ('https://chromium.googlesource.com/chromium/src' ==
        self.GetRemoteUrl() and
        detail['branch'].startswith('refs/branch-heads/')):
      return False
    return True

  def CMDLand(self, force, bypass_hooks, verbose, parallel):
    if git_common.is_dirty_git_tree('land'):
      return 1

    detail = self._GetChangeDetail(['CURRENT_REVISION', 'LABELS'])
    if not force and self._IsCqConfigured():
      confirm_or_exit('\nIt seems this repository has a CQ, '
                        'which can test and land changes for you. '
                        'Are you sure you wish to bypass it?\n',
                        action='bypass CQ')
    differs = True
    last_upload = self._GitGetBranchConfigValue('gerritsquashhash')
    # Note: git diff outputs nothing if there is no diff.
    if not last_upload or RunGit(['diff', last_upload]).strip():
      print('WARNING: Some changes from local branch haven\'t been uploaded.')
    else:
      if detail['current_revision'] == last_upload:
        differs = False
      else:
        print('WARNING: Local branch contents differ from latest uploaded '
              'patchset.')
    if differs:
      if not force:
        confirm_or_exit(
            'Do you want to submit latest Gerrit patchset and bypass hooks?\n',
            action='submit')
      print('WARNING: Bypassing hooks and submitting latest uploaded patchset.')
    elif not bypass_hooks:
      hook_results = self.RunHook(
          committing=True,
          may_prompt=not force,
          verbose=verbose,
          change=self.GetChange(self.GetCommonAncestorWithUpstream(), None),
          parallel=parallel)
      if not hook_results.should_continue():
        return 1

    self.SubmitIssue(wait_for_merge=True)
    print('Issue %s has been submitted.' % self.GetIssueURL())
    links = self._GetChangeCommit().get('web_links', [])
    for link in links:
      if link.get('name') == 'gitiles' and link.get('url'):
        print('Landed as: %s' % link.get('url'))
        break
    return 0

  def CMDPatchWithParsedIssue(self, parsed_issue_arg, nocommit, force):
    assert parsed_issue_arg.valid

    self.issue = parsed_issue_arg.issue

    if parsed_issue_arg.hostname:
      self._gerrit_host = parsed_issue_arg.hostname
      self._gerrit_server = 'https://%s' % self._gerrit_host

    try:
      detail = self._GetChangeDetail(['ALL_REVISIONS'])
    except GerritChangeNotExists as e:
      DieWithError(str(e))

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
        DieWithError('Couldn\'t find patchset %i in change %i' %
                     (parsed_issue_arg.patchset, self.GetIssue()))

    remote_url = self.GetRemoteUrl()
    if remote_url.endswith('.git'):
      remote_url = remote_url[:-len('.git')]
    remote_url = remote_url.rstrip('/')

    fetch_info = revision_info['fetch']['http']
    fetch_info['url'] = fetch_info['url'].rstrip('/')

    if remote_url != fetch_info['url']:
      DieWithError('Trying to patch a change from %s but this repo appears '
                   'to be %s.' % (fetch_info['url'], remote_url))

    RunGit(['fetch', fetch_info['url'], fetch_info['ref']])

    if force:
      RunGit(['reset', '--hard', 'FETCH_HEAD'])
      print('Checked out commit for change %i patchset %i locally' %
            (parsed_issue_arg.issue, patchset))
    elif nocommit:
      RunGit(['cherry-pick', '--no-commit', 'FETCH_HEAD'])
      print('Patch applied to index.')
    else:
      RunGit(['cherry-pick', 'FETCH_HEAD'])
      print('Committed patch for change %i patchset %i locally.' %
            (parsed_issue_arg.issue, patchset))
      print('Note: this created a local commit which does not have '
            'the same hash as the one uploaded for review. This will make '
            'uploading changes based on top of this branch difficult.\n'
            'If you want to do that, use "git cl patch --force" instead.')

    if self.GetBranch():
      self.SetIssue(parsed_issue_arg.issue)
      self.SetPatchset(patchset)
      fetched_hash = RunGit(['rev-parse', 'FETCH_HEAD']).strip()
      self._GitSetBranchConfigValue('last-upload-hash', fetched_hash)
      self._GitSetBranchConfigValue('gerritsquashhash', fetched_hash)
    else:
      print('WARNING: You are in detached HEAD state.\n'
            'The patch has been applied to your checkout, but you will not be '
            'able to upload a new patch set to the gerrit issue.\n'
            'Try using the \'-b\' option if you would like to work on a '
            'branch and/or upload a new patch set.')

    return 0

  @staticmethod
  def ParseIssueURL(parsed_url):
    if not parsed_url.scheme or not parsed_url.scheme.startswith('http'):
      return None
    # Gerrit's new UI is https://domain/c/project/+/<issue_number>[/[patchset]]
    # But old GWT UI is https://domain/#/c/project/+/<issue_number>[/[patchset]]
    # Short urls like https://domain/<issue_number> can be used, but don't allow
    # specifying the patchset (you'd 404), but we allow that here.
    if parsed_url.path == '/':
      part = parsed_url.fragment
    else:
      part = parsed_url.path
    match = re.match(r'(/c(/.*/\+)?)?/(\d+)(/(\d+)?/?)?$', part)
    if match:
      return _ParsedIssueNumberArgument(
          issue=int(match.group(3)),
          patchset=int(match.group(5)) if match.group(5) else None,
          hostname=parsed_url.netloc)
    return None

  def _GerritCommitMsgHookCheck(self, offer_removal):
    hook = os.path.join(settings.GetRoot(), '.git', 'hooks', 'commit-msg')
    if not os.path.exists(hook):
      return
    # Crude attempt to distinguish Gerrit Codereview hook from a potentially
    # custom developer-made one.
    data = gclient_utils.FileRead(hook)
    if not('From Gerrit Code Review' in data and 'add_ChangeId()' in data):
      return
    print('WARNING: You have Gerrit commit-msg hook installed.\n'
          'It is not necessary for uploading with git cl in squash mode, '
          'and may interfere with it in subtle ways.\n'
          'We recommend you remove the commit-msg hook.')
    if offer_removal:
      if ask_for_explicit_yes('Do you want to remove it now?'):
        gclient_utils.rm_file_or_tree(hook)
        print('Gerrit commit-msg hook removed.')
      else:
        print('OK, will keep Gerrit commit-msg hook in place.')

  def _CleanUpOldTraces(self):
    """Keep only the last |MAX_TRACES| traces."""
    try:
      traces = sorted([
        os.path.join(TRACES_DIR, f)
        for f in os.listdir(TRACES_DIR)
        if (os.path.isfile(os.path.join(TRACES_DIR, f))
            and not f.startswith('tmp'))
      ])
      traces_to_delete = traces[:-MAX_TRACES]
      for trace in traces_to_delete:
        os.remove(trace)
    except OSError:
      print('WARNING: Failed to remove old git traces from\n'
            '  %s'
            'Consider removing them manually.' % TRACES_DIR)

  def _WriteGitPushTraces(self, trace_name, traces_dir, git_push_metadata):
    """Zip and write the git push traces stored in traces_dir."""
    gclient_utils.safe_makedirs(TRACES_DIR)
    traces_zip = trace_name + '-traces'
    traces_readme = trace_name + '-README'
    # Create a temporary dir to store git config and gitcookies in. It will be
    # compressed and stored next to the traces.
    git_info_dir = tempfile.mkdtemp()
    git_info_zip = trace_name + '-git-info'

    git_push_metadata['now'] = datetime_now().strftime('%c')
    if sys.stdin.encoding and sys.stdin.encoding != 'utf-8':
      git_push_metadata['now'] = git_push_metadata['now'].decode(
          sys.stdin.encoding)

    git_push_metadata['trace_name'] = trace_name
    gclient_utils.FileWrite(
        traces_readme, TRACES_README_FORMAT % git_push_metadata)

    # Keep only the first 6 characters of the git hashes on the packet
    # trace. This greatly decreases size after compression.
    packet_traces = os.path.join(traces_dir, 'trace-packet')
    if os.path.isfile(packet_traces):
      contents = gclient_utils.FileRead(packet_traces)
      gclient_utils.FileWrite(
          packet_traces, GIT_HASH_RE.sub(r'\1', contents))
    shutil.make_archive(traces_zip, 'zip', traces_dir)

    # Collect and compress the git config and gitcookies.
    git_config = RunGit(['config', '-l'])
    gclient_utils.FileWrite(
        os.path.join(git_info_dir, 'git-config'),
        git_config)

    cookie_auth = gerrit_util.Authenticator.get()
    if isinstance(cookie_auth, gerrit_util.CookiesAuthenticator):
      gitcookies_path = cookie_auth.get_gitcookies_path()
      if os.path.isfile(gitcookies_path):
        gitcookies = gclient_utils.FileRead(gitcookies_path)
        gclient_utils.FileWrite(
            os.path.join(git_info_dir, 'gitcookies'),
            GITCOOKIES_REDACT_RE.sub('REDACTED', gitcookies))
    shutil.make_archive(git_info_zip, 'zip', git_info_dir)

    gclient_utils.rmtree(git_info_dir)

  def _RunGitPushWithTraces(
      self, change_desc, refspec, refspec_opts, git_push_metadata):
    """Run git push and collect the traces resulting from the execution."""
    # Create a temporary directory to store traces in. Traces will be compressed
    # and stored in a 'traces' dir inside depot_tools.
    traces_dir = tempfile.mkdtemp()
    trace_name = os.path.join(
        TRACES_DIR, datetime_now().strftime('%Y%m%dT%H%M%S.%f'))

    env = os.environ.copy()
    env['GIT_REDACT_COOKIES'] = 'o,SSO,GSSO_Uberproxy'
    env['GIT_TR2_EVENT'] = os.path.join(traces_dir, 'tr2-event')
    env['GIT_TRACE2_EVENT'] = os.path.join(traces_dir, 'tr2-event')
    env['GIT_TRACE_CURL'] = os.path.join(traces_dir, 'trace-curl')
    env['GIT_TRACE_CURL_NO_DATA'] = '1'
    env['GIT_TRACE_PACKET'] = os.path.join(traces_dir, 'trace-packet')

    try:
      push_returncode = 0
      remote_url = self.GetRemoteUrl()
      before_push = time_time()
      push_stdout = gclient_utils.CheckCallAndFilter(
          ['git', 'push', remote_url, refspec],
          env=env,
          print_stdout=True,
          # Flush after every line: useful for seeing progress when running as
          # recipe.
          filter_fn=lambda _: sys.stdout.flush())
    except subprocess2.CalledProcessError as e:
      push_returncode = e.returncode
      DieWithError('Failed to create a change. Please examine output above '
                   'for the reason of the failure.\n'
                   'Hint: run command below to diagnose common Git/Gerrit '
                   'credential problems:\n'
                   '  git cl creds-check\n'
                   '\n'
                   'If git-cl is not working correctly, file a bug under the '
                   'Infra>SDK component including the files below.\n'
                   'Review the files before upload, since they might contain '
                   'sensitive information.\n'
                   'Set the Restrict-View-Google label so that they are not '
                   'publicly accessible.\n'
                   + TRACES_MESSAGE % {'trace_name': trace_name},
                   change_desc)
    finally:
      execution_time = time_time() - before_push
      metrics.collector.add_repeated('sub_commands', {
        'command': 'git push',
        'execution_time': execution_time,
        'exit_code': push_returncode,
        'arguments': metrics_utils.extract_known_subcommand_args(refspec_opts),
      })

      git_push_metadata['execution_time'] = execution_time
      git_push_metadata['exit_code'] = push_returncode
      self._WriteGitPushTraces(trace_name, traces_dir, git_push_metadata)

      self._CleanUpOldTraces()
      gclient_utils.rmtree(traces_dir)

    return push_stdout

  def CMDUploadChange(self, options, git_diff_args, custom_cl_base, change):
    """Upload the current branch to Gerrit."""
    if options.squash is None:
      # Load default for user, repo, squash=true, in this order.
      options.squash = settings.GetSquashGerritUploads()

    remote, remote_branch = self.GetRemoteBranch()
    branch = GetTargetRef(remote, remote_branch, options.target_branch)
    # This may be None; default fallback value is determined in logic below.
    title = options.title

    # Extract bug number from branch name.
    bug = options.bug
    match = re.match(r'(?:bug|fix)[_-]?(\d+)', self.GetBranch())
    if not bug and match:
      bug = match.group(1)

    if options.squash:
      self._GerritCommitMsgHookCheck(offer_removal=not options.force)
      if self.GetIssue():
        # Try to get the message from a previous upload.
        message = self.GetDescription()
        if not message:
          DieWithError(
              'failed to fetch description from current Gerrit change %d\n'
              '%s' % (self.GetIssue(), self.GetIssueURL()))
        if not title:
          if options.message:
            # When uploading a subsequent patchset, -m|--message is taken
            # as the patchset title if --title was not provided.
            title = options.message.strip()
          else:
            default_title = RunGit(
                ['show', '-s', '--format=%s', 'HEAD']).strip()
            if options.force:
              title = default_title
            else:
              title = ask_for_data(
                  'Title for patchset [%s]: ' % default_title) or default_title
        change_id = self._GetChangeDetail()['change_id']
        while True:
          footer_change_ids = git_footers.get_footer_change_id(message)
          if footer_change_ids == [change_id]:
            break
          if not footer_change_ids:
            message = git_footers.add_footer_change_id(message, change_id)
            print('WARNING: appended missing Change-Id to change description.')
            continue
          # There is already a valid footer but with different or several ids.
          # Doing this automatically is non-trivial as we don't want to lose
          # existing other footers, yet we want to append just 1 desired
          # Change-Id. Thus, just create a new footer, but let user verify the
          # new description.
          message = '%s\n\nChange-Id: %s' % (message, change_id)
          print(
              'WARNING: change %s has Change-Id footer(s):\n'
              '  %s\n'
              'but change has Change-Id %s, according to Gerrit.\n'
              'Please, check the proposed correction to the description, '
              'and edit it if necessary but keep the "Change-Id: %s" footer\n'
              % (self.GetIssue(), '\n  '.join(footer_change_ids), change_id,
                 change_id))
          confirm_or_exit(action='edit')
          if not options.force:
            change_desc = ChangeDescription(message)
            change_desc.prompt(bug=bug)
            message = change_desc.description
            if not message:
              DieWithError("Description is empty. Aborting...")
          # Continue the while loop.
        # Sanity check of this code - we should end up with proper message
        # footer.
        assert [change_id] == git_footers.get_footer_change_id(message)
        change_desc = ChangeDescription(message)
      else:  # if not self.GetIssue()
        if options.message:
          message = options.message
        else:
          message = _create_description_from_log(git_diff_args)
          if options.title:
            message = options.title + '\n\n' + message
        change_desc = ChangeDescription(message)

        if not options.force:
          change_desc.prompt(bug=bug)
        # On first upload, patchset title is always this string, while
        # --title flag gets converted to first line of message.
        title = 'Initial upload'
        if not change_desc.description:
          DieWithError("Description is empty. Aborting...")
        change_ids = git_footers.get_footer_change_id(change_desc.description)
        if len(change_ids) > 1:
          DieWithError('too many Change-Id footers, at most 1 allowed.')
        if not change_ids:
          # Generate the Change-Id automatically.
          change_desc.set_description(git_footers.add_footer_change_id(
              change_desc.description,
              GenerateGerritChangeId(change_desc.description)))
          change_ids = git_footers.get_footer_change_id(change_desc.description)
          assert len(change_ids) == 1
        change_id = change_ids[0]

      if options.reviewers or options.tbrs or options.add_owners_to:
        change_desc.update_reviewers(options.reviewers, options.tbrs,
                                     options.add_owners_to, change)
      if options.preserve_tryjobs:
        change_desc.set_preserve_tryjobs()

      remote, upstream_branch = self.FetchUpstreamTuple(self.GetBranch())
      parent = self._ComputeParent(remote, upstream_branch, custom_cl_base,
                                   options.force, change_desc)
      tree = RunGit(['rev-parse', 'HEAD:']).strip()
      with tempfile.NamedTemporaryFile(delete=False) as desc_tempfile:
        desc_tempfile.write(change_desc.description)
        desc_tempfile.close()
        ref_to_push = RunGit(['commit-tree', tree, '-p', parent,
                              '-F', desc_tempfile.name]).strip()
        os.remove(desc_tempfile.name)
    else:
      change_desc = ChangeDescription(
          options.message or _create_description_from_log(git_diff_args))
      if not change_desc.description:
        DieWithError("Description is empty. Aborting...")

      if not git_footers.get_footer_change_id(change_desc.description):
        DownloadGerritHook(False)
        change_desc.set_description(
            self._AddChangeIdToCommitMessage(options, git_diff_args))
      if options.reviewers or options.tbrs or options.add_owners_to:
        change_desc.update_reviewers(options.reviewers, options.tbrs,
                                     options.add_owners_to, change)
      ref_to_push = 'HEAD'
      # For no-squash mode, we assume the remote called "origin" is the one we
      # want. It is not worthwhile to support different workflows for
      # no-squash mode.
      parent = 'origin/%s' % branch
      change_id = git_footers.get_footer_change_id(change_desc.description)[0]

    assert change_desc
    SaveDescriptionBackup(change_desc)
    commits = RunGitSilent(['rev-list', '%s..%s' % (parent,
                                                    ref_to_push)]).splitlines()
    if len(commits) > 1:
      print('WARNING: This will upload %d commits. Run the following command '
            'to see which commits will be uploaded: ' % len(commits))
      print('git log %s..%s' % (parent, ref_to_push))
      print('You can also use `git squash-branch` to squash these into a '
            'single commit.')
      confirm_or_exit(action='upload')

    if options.reviewers or options.tbrs or options.add_owners_to:
      change_desc.update_reviewers(options.reviewers, options.tbrs,
                                   options.add_owners_to, change)

    reviewers = sorted(change_desc.get_reviewers())
    # Add cc's from the CC_LIST and --cc flag (if any).
    if not options.private and not options.no_autocc:
      cc = self.GetCCList().split(',')
    else:
      cc = []
    if options.cc:
      cc.extend(options.cc)
    cc = filter(None, [email.strip() for email in cc])
    if change_desc.get_cced():
      cc.extend(change_desc.get_cced())
    if self._GetGerritHost() == 'chromium-review.googlesource.com':
      valid_accounts = set(reviewers + cc)
      # TODO(crbug/877717): relax this for all hosts.
    else:
      valid_accounts = gerrit_util.ValidAccounts(
          self._GetGerritHost(), reviewers + cc)
    logging.info('accounts %s are recognized, %s invalid',
                 sorted(valid_accounts),
                 set(reviewers + cc).difference(set(valid_accounts)))

    # Extra options that can be specified at push time. Doc:
    # https://gerrit-review.googlesource.com/Documentation/user-upload.html
    refspec_opts = []

    # By default, new changes are started in WIP mode, and subsequent patchsets
    # don't send email. At any time, passing --send-mail will mark the change
    # ready and send email for that particular patch.
    if options.send_mail:
      refspec_opts.append('ready')
      refspec_opts.append('notify=ALL')
    elif not self.GetIssue() and options.squash:
      refspec_opts.append('wip')
    else:
      refspec_opts.append('notify=NONE')

    # TODO(tandrii): options.message should be posted as a comment
    # if --send-mail is set on non-initial upload as Rietveld used to do it.

    if title:
      # Punctuation and whitespace in |title| must be percent-encoded.
      refspec_opts.append('m=' + gerrit_util.PercentEncodeForGitRef(title))

    if options.private:
      refspec_opts.append('private')

    for r in sorted(reviewers):
      if r in valid_accounts:
        refspec_opts.append('r=%s' % r)
        reviewers.remove(r)
      else:
        # TODO(tandrii): this should probably be a hard failure.
        print('WARNING: reviewer %s doesn\'t have a Gerrit account, skipping'
              % r)
    for c in sorted(cc):
      # refspec option will be rejected if cc doesn't correspond to an
      # account, even though REST call to add such arbitrary cc may succeed.
      if c in valid_accounts:
        refspec_opts.append('cc=%s' % c)
        cc.remove(c)

    if options.topic:
      # Documentation on Gerrit topics is here:
      # https://gerrit-review.googlesource.com/Documentation/user-upload.html#topic
      refspec_opts.append('topic=%s' % options.topic)

    if options.enable_auto_submit:
      refspec_opts.append('l=Auto-Submit+1')
    if options.use_commit_queue:
      refspec_opts.append('l=Commit-Queue+2')
    elif options.cq_dry_run:
      refspec_opts.append('l=Commit-Queue+1')

    if change_desc.get_reviewers(tbr_only=True):
      score = gerrit_util.GetCodeReviewTbrScore(
          self._GetGerritHost(),
          self._GetGerritProject())
      refspec_opts.append('l=Code-Review+%s' % score)

    # Gerrit sorts hashtags, so order is not important.
    hashtags = {change_desc.sanitize_hash_tag(t) for t in options.hashtags}
    if not self.GetIssue():
      hashtags.update(change_desc.get_hash_tags())
    refspec_opts += ['hashtag=%s' % t for t in sorted(hashtags)]

    refspec_suffix = ''
    if refspec_opts:
      refspec_suffix = '%' + ','.join(refspec_opts)
      assert ' ' not in refspec_suffix, (
          'spaces not allowed in refspec: "%s"' % refspec_suffix)
    refspec = '%s:refs/for/%s%s' % (ref_to_push, branch, refspec_suffix)

    git_push_metadata = {
        'gerrit_host': self._GetGerritHost(),
        'title': title or '<untitled>',
        'change_id': change_id,
        'description': change_desc.description,
    }
    push_stdout = self._RunGitPushWithTraces(
        change_desc, refspec, refspec_opts, git_push_metadata)

    if options.squash:
      regex = re.compile(r'remote:\s+https?://[\w\-\.\+\/#]*/(\d+)\s.*')
      change_numbers = [m.group(1)
                        for m in map(regex.match, push_stdout.splitlines())
                        if m]
      if len(change_numbers) != 1:
        DieWithError(
          ('Created|Updated %d issues on Gerrit, but only 1 expected.\n'
           'Change-Id: %s') % (len(change_numbers), change_id), change_desc)
      self.SetIssue(change_numbers[0])
      self._GitSetBranchConfigValue('gerritsquashhash', ref_to_push)

    if self.GetIssue() and (reviewers or cc):
      # GetIssue() is not set in case of non-squash uploads according to tests.
      # TODO(agable): non-squash uploads in git cl should be removed.
      gerrit_util.AddReviewers(
          self._GetGerritHost(),
          self._GerritChangeIdentifier(),
          reviewers, cc,
          notify=bool(options.send_mail))

    return 0

  def _ComputeParent(self, remote, upstream_branch, custom_cl_base, force,
                     change_desc):
    """Computes parent of the generated commit to be uploaded to Gerrit.

    Returns revision or a ref name.
    """
    if custom_cl_base:
      # Try to avoid creating additional unintended CLs when uploading, unless
      # user wants to take this risk.
      local_ref_of_target_remote = self.GetRemoteBranch()[1]
      code, _ = RunGitWithCode(['merge-base', '--is-ancestor', custom_cl_base,
                                local_ref_of_target_remote])
      if code == 1:
        print('\nWARNING: Manually specified base of this CL `%s` '
              'doesn\'t seem to belong to target remote branch `%s`.\n\n'
              'If you proceed with upload, more than 1 CL may be created by '
              'Gerrit as a result, in turn confusing or crashing git cl.\n\n'
              'If you are certain that specified base `%s` has already been '
              'uploaded to Gerrit as another CL, you may proceed.\n' %
              (custom_cl_base, local_ref_of_target_remote, custom_cl_base))
        if not force:
          confirm_or_exit(
              'Do you take responsibility for cleaning up potential mess '
              'resulting from proceeding with upload?',
              action='upload')
      return custom_cl_base

    if remote != '.':
      return self.GetCommonAncestorWithUpstream()

    # If our upstream branch is local, we base our squashed commit on its
    # squashed version.
    upstream_branch_name = scm.GIT.ShortBranchName(upstream_branch)

    if upstream_branch_name == 'master':
      return self.GetCommonAncestorWithUpstream()

    # Check the squashed hash of the parent.
    # TODO(tandrii): consider checking parent change in Gerrit and using its
    # hash if tree hash of latest parent revision (patchset) in Gerrit matches
    # the tree hash of the parent branch. The upside is less likely bogus
    # requests to reupload parent change just because it's uploadhash is
    # missing, yet the downside likely exists, too (albeit unknown to me yet).
    parent = RunGit(['config',
                     'branch.%s.gerritsquashhash' % upstream_branch_name],
                    error_ok=True).strip()
    # Verify that the upstream branch has been uploaded too, otherwise
    # Gerrit will create additional CLs when uploading.
    if not parent or (RunGitSilent(['rev-parse', upstream_branch + ':']) !=
                      RunGitSilent(['rev-parse', parent + ':'])):
      DieWithError(
          '\nUpload upstream branch %s first.\n'
          'It is likely that this branch has been rebased since its last '
          'upload, so you just need to upload it again.\n'
          '(If you uploaded it with --no-squash, then branch dependencies '
          'are not supported, and you should reupload with --squash.)'
          % upstream_branch_name,
          change_desc)
    return parent

  def _AddChangeIdToCommitMessage(self, options, args):
    """Re-commits using the current message, assumes the commit hook is in
    place.
    """
    log_desc = options.message or _create_description_from_log(args)
    git_command = ['commit', '--amend', '-m', log_desc]
    RunGit(git_command)
    new_log_desc = _create_description_from_log(args)
    if git_footers.get_footer_change_id(new_log_desc):
      print('git-cl: Added Change-Id to commit message.')
      return new_log_desc
    else:
      DieWithError('ERROR: Gerrit commit-msg hook not installed.')

  def CannotTriggerTryJobReason(self):
    try:
      data = self._GetChangeDetail()
    except GerritChangeNotExists:
      return 'Gerrit doesn\'t know about your change %s' % self.GetIssue()

    if data['status'] in ('ABANDONED', 'MERGED'):
      return 'CL %s is closed' % self.GetIssue()

  def GetTryJobProperties(self, patchset=None):
    """Returns dictionary of properties to launch a tryjob."""
    data = self._GetChangeDetail(['ALL_REVISIONS'])
    patchset = int(patchset or self.GetPatchset())
    assert patchset
    revision_data = None  # Pylint wants it to be defined.
    for revision_data in data['revisions'].itervalues():
      if int(revision_data['_number']) == patchset:
        break
    else:
      raise Exception('Patchset %d is not known in Gerrit change %d' %
                      (patchset, self.GetIssue()))
    return {
      'patch_issue': self.GetIssue(),
      'patch_set': patchset or self.GetPatchset(),
      'patch_project': data['project'],
      'patch_storage': 'gerrit',
      'patch_ref': revision_data['fetch']['http']['ref'],
      'patch_repository_url': revision_data['fetch']['http']['url'],
      'patch_gerrit_url': self.GetCodereviewServer(),
    }

  def GetIssueOwner(self):
    return self._GetChangeDetail(['DETAILED_ACCOUNTS'])['owner']['email']

  def GetReviewers(self):
    details = self._GetChangeDetail(['DETAILED_ACCOUNTS'])
    return [r['email'] for r in details['reviewers'].get('REVIEWER', [])]


_CODEREVIEW_IMPLEMENTATIONS = {
  'gerrit': Changelist,
}


def _add_codereview_issue_select_options(parser, extra=""):
  _add_codereview_select_options(parser)

  text = ('Operate on this issue number instead of the current branch\'s '
          'implicit issue.')
  if extra:
    text += ' '+extra
  parser.add_option('-i', '--issue', type=int, help=text)


def _add_codereview_select_options(parser):
  """Appends --gerrit option to force specific codereview."""
  parser.codereview_group = optparse.OptionGroup(
      parser, 'DEPRECATED! Codereview override options')
  parser.add_option_group(parser.codereview_group)
  parser.codereview_group.add_option(
      '--gerrit', action='store_true',
      help='Deprecated. Noop. Do not use.')


def _process_codereview_select_options(parser, options):
  options.forced_codereview = None
  if options.gerrit:
    options.forced_codereview = 'gerrit'


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
  CC_LINE = r'^[ \t]*(CC)[ \t]*=[ \t]*(.*?)[ \t]*$'
  BUG_LINE = r'^[ \t]*(?:(BUG)[ \t]*=|Bug:)[ \t]*(.*?)[ \t]*$'
  CHERRY_PICK_LINE = r'^\(cherry picked from commit [a-fA-F0-9]{40}\)$'
  STRIP_HASH_TAG_PREFIX = r'^(\s*(revert|reland)( "|:)?\s*)*'
  BRACKET_HASH_TAG = r'\s*\[([^\[\]]+)\]'
  COLON_SEPARATED_HASH_TAG = r'^([a-zA-Z0-9_\- ]+):'
  BAD_HASH_TAG_CHUNK = r'[^a-zA-Z0-9]+'

  def __init__(self, description):
    self._description_lines = (description or '').strip().splitlines()

  @property               # www.logilab.org/ticket/89786
  def description(self):  # pylint: disable=method-hidden
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

  def update_reviewers(self, reviewers, tbrs, add_owners_to=None, change=None):
    """Rewrites the R=/TBR= line(s) as a single line each.

    Args:
      reviewers (list(str)) - list of additional emails to use for reviewers.
      tbrs (list(str)) - list of additional emails to use for TBRs.
      add_owners_to (None|'R'|'TBR') - Pass to do an OWNERS lookup for files in
        the change that are missing OWNER coverage. If this is not None, you
        must also pass a value for `change`.
      change (Change) - The Change that should be used for OWNERS lookups.
    """
    assert isinstance(reviewers, list), reviewers
    assert isinstance(tbrs, list), tbrs

    assert add_owners_to in (None, 'TBR', 'R'), add_owners_to
    assert not add_owners_to or change, add_owners_to

    if not reviewers and not tbrs and not add_owners_to:
      return

    reviewers = set(reviewers)
    tbrs = set(tbrs)
    LOOKUP = {
      'TBR': tbrs,
      'R': reviewers,
    }

    # Get the set of R= and TBR= lines and remove them from the description.
    regexp = re.compile(self.R_LINE)
    matches = [regexp.match(line) for line in self._description_lines]
    new_desc = [l for i, l in enumerate(self._description_lines)
                if not matches[i]]
    self.set_description(new_desc)

    # Construct new unified R= and TBR= lines.

    # First, update tbrs/reviewers with names from the R=/TBR= lines (if any).
    for match in matches:
      if not match:
        continue
      LOOKUP[match.group(1)].update(cleanup_list([match.group(2).strip()]))

    # Next, maybe fill in OWNERS coverage gaps to either tbrs/reviewers.
    if add_owners_to:
      owners_db = owners.Database(change.RepositoryRoot(),
                                  fopen=file, os_path=os.path)
      missing_files = owners_db.files_not_covered_by(change.LocalPaths(),
                                                     (tbrs | reviewers))
      LOOKUP[add_owners_to].update(
        owners_db.reviewers_for(missing_files, change.author_email))

    # If any folks ended up in both groups, remove them from tbrs.
    tbrs -= reviewers

    new_r_line = 'R=' + ', '.join(sorted(reviewers)) if reviewers else None
    new_tbr_line = 'TBR=' + ', '.join(sorted(tbrs)) if tbrs else None

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

  def set_preserve_tryjobs(self):
    """Ensures description footer contains 'Cq-Do-Not-Cancel-Tryjobs: true'."""
    footers = git_footers.parse_footers(self.description)
    for v in footers.get('Cq-Do-Not-Cancel-Tryjobs', []):
      if v.lower() == 'true':
        return
    self.append_footer('Cq-Do-Not-Cancel-Tryjobs: true')

  def prompt(self, bug=None, git_footer=True):
    """Asks the user to update the description."""
    self.set_description([
      '# Enter a description of the change.',
      '# This will be displayed on the codereview site.',
      '# The first line will also be used as the subject of the review.',
      '#--------------------This line is 72 characters long'
      '--------------------',
    ] + self._description_lines)

    regexp = re.compile(self.BUG_LINE)
    prefix = settings.GetBugPrefix()
    if not any((regexp.match(line) for line in self._description_lines)):
      values = list(_get_bug_line_values(prefix, bug or '')) or [prefix]
      if git_footer:
        self.append_footer('Bug: %s' % ', '.join(values))
      else:
        for value in values:
          self.append_footer('BUG=%s' % value)

    content = gclient_utils.RunEditor(self.description, True,
                                      git_editor=settings.GetGitEditor())
    if not content:
      DieWithError('Running editor failed')
    lines = content.splitlines()

    # Strip off comments and default inserted "Bug:" line.
    clean_lines = [line.rstrip() for line in lines if not
                   (line.startswith('#') or
                    line.rstrip() == "Bug:" or
                    line.rstrip() == "Bug: " + prefix)]
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

  def get_reviewers(self, tbr_only=False):
    """Retrieves the list of reviewers."""
    matches = [re.match(self.R_LINE, line) for line in self._description_lines]
    reviewers = [match.group(2).strip()
                 for match in matches
                 if match and (not tbr_only or match.group(1).upper() == 'TBR')]
    return cleanup_list(reviewers)

  def get_cced(self):
    """Retrieves the list of reviewers."""
    matches = [re.match(self.CC_LINE, line) for line in self._description_lines]
    cced = [match.group(2).strip() for match in matches if match]
    return cleanup_list(cced)

  def get_hash_tags(self):
    """Extracts and sanitizes a list of Gerrit hashtags."""
    subject = (self._description_lines or ('',))[0]
    subject = re.sub(
        self.STRIP_HASH_TAG_PREFIX, '', subject, flags=re.IGNORECASE)

    tags = []
    start = 0
    bracket_exp = re.compile(self.BRACKET_HASH_TAG)
    while True:
      m = bracket_exp.match(subject, start)
      if not m:
        break
      tags.append(self.sanitize_hash_tag(m.group(1)))
      start = m.end()

    if not tags:
      # Try "Tag: " prefix.
      m = re.match(self.COLON_SEPARATED_HASH_TAG, subject)
      if m:
        tags.append(self.sanitize_hash_tag(m.group(1)))
    return tags

  @classmethod
  def sanitize_hash_tag(cls, tag):
    """Returns a sanitized Gerrit hash tag.

    A sanitized hashtag can be used as a git push refspec parameter value.
    """
    return re.sub(cls.BAD_HASH_TAG_CHUNK, '-', tag).strip('-').lower()

  def update_with_git_number_footers(self, parent_hash, parent_msg, dest_ref):
    """Updates this commit description given the parent.

    This is essentially what Gnumbd used to do.
    Consult https://goo.gl/WMmpDe for more details.
    """
    assert parent_msg  # No, orphan branch creation isn't supported.
    assert parent_hash
    assert dest_ref
    parent_footer_map = git_footers.parse_footers(parent_msg)
    # This will also happily parse svn-position, which GnumbD is no longer
    # supporting. While we'd generate correct footers, the verifier plugin
    # installed in Gerrit will block such commit (ie git push below will fail).
    parent_position = git_footers.get_position(parent_footer_map)

    # Cherry-picks may have last line obscuring their prior footers,
    # from git_footers perspective. This is also what Gnumbd did.
    cp_line = None
    if (self._description_lines and
        re.match(self.CHERRY_PICK_LINE, self._description_lines[-1])):
      cp_line = self._description_lines.pop()

    top_lines, footer_lines, _ = git_footers.split_footers(self.description)

    # Original-ify all Cr- footers, to avoid re-lands, cherry-picks, or just
    # user interference with actual footers we'd insert below.
    for i, line in enumerate(footer_lines):
      k, v = git_footers.parse_footer(line) or (None, None)
      if k and k.startswith('Cr-'):
        footer_lines[i] = '%s: %s' % ('Cr-Original-' + k[len('Cr-'):], v)

    # Add Position and Lineage footers based on the parent.
    lineage = list(reversed(parent_footer_map.get('Cr-Branched-From', [])))
    if parent_position[0] == dest_ref:
      # Same branch as parent.
      number = int(parent_position[1]) + 1
    else:
      number = 1  # New branch, and extra lineage.
      lineage.insert(0, '%s-%s@{#%d}' % (parent_hash, parent_position[0],
                                         int(parent_position[1])))

    footer_lines.append('Cr-Commit-Position: %s@{#%d}' % (dest_ref, number))
    footer_lines.extend('Cr-Branched-From: %s' % v for v in lineage)

    self._description_lines = top_lines
    if cp_line:
      self._description_lines.append(cp_line)
    if self._description_lines[-1] != '':
      self._description_lines.append('')  # Ensure footer separator.
    self._description_lines.extend(footer_lines)


def get_approving_reviewers(props, disapproval=False):
  """Retrieves the reviewers that approved a CL from the issue properties with
  messages.

  Note that the list may contain reviewers that are not committer, thus are not
  considered by the CQ.

  If disapproval is True, instead returns reviewers who 'not lgtm'd the CL.
  """
  approval_type = 'disapproval' if disapproval else 'approval'
  return sorted(
      set(
        message['sender']
        for message in props['messages']
        if message[approval_type] and message['sender'] in props['reviewers']
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
  """Parses a codereview.settings file and updates hooks."""
  keyvals = gclient_utils.ParseCodereviewSettingsContent(fileobj.read())

  def SetProperty(name, setting, unset_error_ok=False):
    fullname = 'rietveld.' + name
    if setting in keyvals:
      RunGit(['config', fullname, keyvals[setting]])
    else:
      RunGit(['config', '--unset-all', fullname], error_ok=unset_error_ok)

  if not keyvals.get('GERRIT_HOST', False):
    SetProperty('server', 'CODE_REVIEW_SERVER')
  # Only server setting is required. Other settings can be absent.
  # In that case, we ignore errors raised during option deletion attempt.
  SetProperty('cc', 'CC_LIST', unset_error_ok=True)
  SetProperty('tree-status-url', 'STATUS', unset_error_ok=True)
  SetProperty('viewvc-url', 'VIEW_VC', unset_error_ok=True)
  SetProperty('bug-prefix', 'BUG_PREFIX', unset_error_ok=True)
  SetProperty('cpplint-regex', 'LINT_REGEX', unset_error_ok=True)
  SetProperty('cpplint-ignore-regex', 'LINT_IGNORE_REGEX', unset_error_ok=True)
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
    # should be of the form
    # PUSH_URL_CONFIG: url.ssh://gitrw.chromium.org.pushinsteadof
    # ORIGIN_URL_CONFIG: http://src.chromium.org/git
    RunGit(['config', keyvals['PUSH_URL_CONFIG'],
            keyvals['ORIGIN_URL_CONFIG']])


def urlretrieve(source, destination):
  """Downloads a network object to a local file, like urllib.urlretrieve.

  This is necessary because urllib is broken for SSL connections via a proxy.
  """
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
  """Downloads and installs a Gerrit commit-msg hook.

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


class _GitCookiesChecker(object):
  """Provides facilities for validating and suggesting fixes to .gitcookies."""

  _GOOGLESOURCE = 'googlesource.com'

  def __init__(self):
    # Cached list of [host, identity, source], where source is either
    # .gitcookies or .netrc.
    self._all_hosts = None

  def ensure_configured_gitcookies(self):
    """Runs checks and suggests fixes to make git use .gitcookies from default
    path."""
    default = gerrit_util.CookiesAuthenticator.get_gitcookies_path()
    configured_path = RunGitSilent(
        ['config', '--global', 'http.cookiefile']).strip()
    configured_path = os.path.expanduser(configured_path)
    if configured_path:
      self._ensure_default_gitcookies_path(configured_path, default)
    else:
      self._configure_gitcookies_path(default)

  @staticmethod
  def _ensure_default_gitcookies_path(configured_path, default_path):
    assert configured_path
    if configured_path == default_path:
      print('git is already configured to use your .gitcookies from %s' %
            configured_path)
      return

    print('WARNING: You have configured custom path to .gitcookies: %s\n'
          'Gerrit and other depot_tools expect .gitcookies at %s\n' %
          (configured_path, default_path))

    if not os.path.exists(configured_path):
      print('However, your configured .gitcookies file is missing.')
      confirm_or_exit('Reconfigure git to use default .gitcookies?',
                      action='reconfigure')
      RunGit(['config', '--global', 'http.cookiefile', default_path])
      return

    if os.path.exists(default_path):
      print('WARNING: default .gitcookies file already exists %s' %
            default_path)
      DieWithError('Please delete %s manually and re-run git cl creds-check' %
                   default_path)

    confirm_or_exit('Move existing .gitcookies to default location?',
                    action='move')
    shutil.move(configured_path, default_path)
    RunGit(['config', '--global', 'http.cookiefile', default_path])
    print('Moved and reconfigured git to use .gitcookies from %s' %
          default_path)

  @staticmethod
  def _configure_gitcookies_path(default_path):
    netrc_path = gerrit_util.CookiesAuthenticator.get_netrc_path()
    if os.path.exists(netrc_path):
      print('You seem to be using outdated .netrc for git credentials: %s' %
            netrc_path)
    print('This tool will guide you through setting up recommended '
          '.gitcookies store for git credentials.\n'
          '\n'
          'IMPORTANT: If something goes wrong and you decide to go back, do:\n'
          '  git config --global --unset http.cookiefile\n'
          '  mv %s %s.backup\n\n' % (default_path, default_path))
    confirm_or_exit(action='setup .gitcookies')
    RunGit(['config', '--global', 'http.cookiefile', default_path])
    print('Configured git to use .gitcookies from %s' % default_path)

  def get_hosts_with_creds(self, include_netrc=False):
    if self._all_hosts is None:
      a = gerrit_util.CookiesAuthenticator()
      self._all_hosts = [
          (h, u, s)
          for h, u, s in itertools.chain(
              ((h, u, '.netrc') for h, (u, _, _) in a.netrc.hosts.iteritems()),
              ((h, u, '.gitcookies') for h, (u, _) in a.gitcookies.iteritems())
          )
          if h.endswith(self._GOOGLESOURCE)
      ]

    if include_netrc:
      return self._all_hosts
    return [(h, u, s) for h, u, s in self._all_hosts if s != '.netrc']

  def print_current_creds(self, include_netrc=False):
    hosts = sorted(self.get_hosts_with_creds(include_netrc=include_netrc))
    if not hosts:
      print('No Git/Gerrit credentials found')
      return
    lengths = [max(map(len, (row[i] for row in hosts))) for i in xrange(3)]
    header = [('Host', 'User', 'Which file'),
              ['=' * l for l in lengths]]
    for row in (header + hosts):
      print('\t'.join((('%%+%ds' % l) % s)
                       for l, s in zip(lengths, row)))

  @staticmethod
  def _parse_identity(identity):
    """Parses identity "git-<username>.domain" into <username> and domain."""
    # Special case: usernames that contain ".", which are generally not
    # distinguishable from sub-domains. But we do know typical domains:
    if identity.endswith('.chromium.org'):
      domain = 'chromium.org'
      username = identity[:-len('.chromium.org')]
    else:
      username, domain = identity.split('.', 1)
    if username.startswith('git-'):
      username = username[len('git-'):]
    return username, domain

  def _get_usernames_of_domain(self, domain):
    """Returns list of usernames referenced by .gitcookies in a given domain."""
    identities_by_domain = {}
    for _, identity, _ in self.get_hosts_with_creds():
      username, domain = self._parse_identity(identity)
      identities_by_domain.setdefault(domain, []).append(username)
    return identities_by_domain.get(domain)

  def _canonical_git_googlesource_host(self, host):
    """Normalizes Gerrit hosts (with '-review') to Git host."""
    assert host.endswith(self._GOOGLESOURCE)
    # Prefix doesn't include '.' at the end.
    prefix = host[:-(1 + len(self._GOOGLESOURCE))]
    if prefix.endswith('-review'):
      prefix = prefix[:-len('-review')]
    return prefix + '.' + self._GOOGLESOURCE

  def _canonical_gerrit_googlesource_host(self, host):
    git_host = self._canonical_git_googlesource_host(host)
    prefix = git_host.split('.', 1)[0]
    return prefix + '-review.' + self._GOOGLESOURCE

  def _get_counterpart_host(self, host):
    assert host.endswith(self._GOOGLESOURCE)
    git = self._canonical_git_googlesource_host(host)
    gerrit = self._canonical_gerrit_googlesource_host(git)
    return git if gerrit == host else gerrit

  def has_generic_host(self):
    """Returns whether generic .googlesource.com has been configured.

    Chrome Infra recommends to use explicit ${host}.googlesource.com instead.
    """
    for host, _, _ in self.get_hosts_with_creds(include_netrc=False):
      if host == '.' + self._GOOGLESOURCE:
        return True
    return False

  def _get_git_gerrit_identity_pairs(self):
    """Returns map from canonic host to pair of identities (Git, Gerrit).

    One of identities might be None, meaning not configured.
    """
    host_to_identity_pairs = {}
    for host, identity, _ in self.get_hosts_with_creds():
      canonical = self._canonical_git_googlesource_host(host)
      pair = host_to_identity_pairs.setdefault(canonical, [None, None])
      idx = 0 if canonical == host else 1
      pair[idx] = identity
    return host_to_identity_pairs

  def get_partially_configured_hosts(self):
    return set(
        (host if i1 else self._canonical_gerrit_googlesource_host(host))
        for host, (i1, i2) in self._get_git_gerrit_identity_pairs().iteritems()
        if None in (i1, i2) and host != '.' + self._GOOGLESOURCE)

  def get_conflicting_hosts(self):
    return set(
        host
        for host, (i1, i2) in self._get_git_gerrit_identity_pairs().iteritems()
        if None not in (i1, i2) and i1 != i2)

  def get_duplicated_hosts(self):
    counters = collections.Counter(h for h, _, _ in self.get_hosts_with_creds())
    return set(host for host, count in counters.iteritems() if count > 1)

  _EXPECTED_HOST_IDENTITY_DOMAINS = {
    'chromium.googlesource.com': 'chromium.org',
    'chrome-internal.googlesource.com': 'google.com',
  }

  def get_hosts_with_wrong_identities(self):
    """Finds hosts which **likely** reference wrong identities.

    Note: skips hosts which have conflicting identities for Git and Gerrit.
    """
    hosts = set()
    for host, expected in self._EXPECTED_HOST_IDENTITY_DOMAINS.iteritems():
      pair = self._get_git_gerrit_identity_pairs().get(host)
      if pair and pair[0] == pair[1]:
        _, domain = self._parse_identity(pair[0])
        if domain != expected:
          hosts.add(host)
    return hosts

  @staticmethod
  def _format_hosts(hosts, extra_column_func=None):
    hosts = sorted(hosts)
    assert hosts
    if extra_column_func is None:
      extras = [''] * len(hosts)
    else:
      extras = [extra_column_func(host) for host in hosts]
    tmpl = '%%-%ds    %%-%ds' % (max(map(len, hosts)), max(map(len, extras)))
    lines = []
    for he in zip(hosts, extras):
      lines.append(tmpl % he)
    return lines

  def _find_problems(self):
    if self.has_generic_host():
      yield ('.googlesource.com wildcard record detected',
             ['Chrome Infrastructure team recommends to list full host names '
              'explicitly.'],
             None)

    dups = self.get_duplicated_hosts()
    if dups:
      yield ('The following hosts were defined twice',
             self._format_hosts(dups),
             None)

    partial = self.get_partially_configured_hosts()
    if partial:
      yield ('Credentials should come in pairs for Git and Gerrit hosts. '
             'These hosts are missing',
             self._format_hosts(partial, lambda host: 'but %s defined' %
                self._get_counterpart_host(host)),
             partial)

    conflicting = self.get_conflicting_hosts()
    if conflicting:
      yield ('The following Git hosts have differing credentials from their '
             'Gerrit counterparts',
             self._format_hosts(conflicting, lambda host: '%s vs %s' %
                 tuple(self._get_git_gerrit_identity_pairs()[host])),
             conflicting)

    wrong = self.get_hosts_with_wrong_identities()
    if wrong:
      yield ('These hosts likely use wrong identity',
             self._format_hosts(wrong, lambda host: '%s but %s recommended' %
                (self._get_git_gerrit_identity_pairs()[host][0],
                 self._EXPECTED_HOST_IDENTITY_DOMAINS[host])),
             wrong)

  def find_and_report_problems(self):
    """Returns True if there was at least one problem, else False."""
    found = False
    bad_hosts = set()
    for title, sublines, hosts in self._find_problems():
      if not found:
        found = True
        print('\n\n.gitcookies problem report:\n')
      bad_hosts.update(hosts or [])
      print('  %s%s' % (title, (':' if sublines else '')))
      if sublines:
        print()
        print('    %s' % '\n    '.join(sublines))
      print()

    if bad_hosts:
      assert found
      print('  You can manually remove corresponding lines in your %s file and '
            'visit the following URLs with correct account to generate '
            'correct credential lines:\n' %
            gerrit_util.CookiesAuthenticator.get_gitcookies_path())
      print('    %s' % '\n    '.join(sorted(set(
          gerrit_util.CookiesAuthenticator().get_new_password_url(
              self._canonical_git_googlesource_host(host))
          for host in bad_hosts
      ))))
    return found


@metrics.collector.collect_metrics('git cl creds-check')
def CMDcreds_check(parser, args):
  """Checks credentials and suggests changes."""
  _, _ = parser.parse_args(args)

  # Code below checks .gitcookies. Abort if using something else.
  authn = gerrit_util.Authenticator.get()
  if not isinstance(authn, gerrit_util.CookiesAuthenticator):
    if isinstance(authn, gerrit_util.GceAuthenticator):
      DieWithError(
          'This command is not designed for GCE, are you on a bot?\n'
          'If you need to run this on GCE, export SKIP_GCE_AUTH_FOR_GIT=1 '
          'in your env.')
    DieWithError(
        'This command is not designed for bot environment. It checks '
        '~/.gitcookies file not generally used on bots.')

  checker = _GitCookiesChecker()
  checker.ensure_configured_gitcookies()

  print('Your .netrc and .gitcookies have credentials for these hosts:')
  checker.print_current_creds(include_netrc=True)

  if not checker.find_and_report_problems():
    print('\nNo problems detected in your .gitcookies file.')
    return 0
  return 1


@metrics.collector.collect_metrics('git cl baseurl')
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
    'unsent': Fore.YELLOW,
    'waiting': Fore.BLUE,
    'reply': Fore.YELLOW,
    'not lgtm': Fore.RED,
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
  if not changes:
    raise StopIteration()

  if not fine_grained:
    # Fast path which doesn't involve querying codereview servers.
    # Do not use get_approving_reviewers(), since it requires an HTTP request.
    for cl in changes:
      yield (cl, 'waiting' if cl.GetIssueURL() else 'error')
    return

  # First, sort out authentication issues.
  logging.debug('ensuring credentials exist')
  for cl in changes:
    cl.EnsureAuthenticated(force=False, refresh=True)

  def fetch(cl):
    try:
      return (cl, cl.GetStatus())
    except:
      # See http://crbug.com/629863.
      logging.exception('failed to fetch status for cl %s:', cl.GetIssue())
      raise

  threads_count = len(changes)
  if max_processes:
    threads_count = max(1, min(threads_count, max_processes))
  logging.debug('querying %d CLs using %d threads', len(changes), threads_count)

  pool = ThreadPool(threads_count)
  fetched_cls = set()
  try:
    it = pool.imap_unordered(fetch, changes).__iter__()
    while True:
      try:
        cl, status = it.next(timeout=5)
      except multiprocessing.TimeoutError:
        break
      fetched_cls.add(cl)
      yield cl, status
  finally:
    pool.close()

  # Add any branches that failed to fetch.
  for cl in set(changes) - fetched_cls:
    yield (cl, 'error')


def upload_branch_deps(cl, args):
  """Uploads CLs of local branches that are dependents of the current branch.

  If the local branch dependency tree looks like:

    test1 -> test2.1 -> test3.1
                     -> test3.2
          -> test2.2 -> test3.3

  and you run "git cl upload --dependencies" from test1 then "git cl upload" is
  run on the dependent branches in this order:
  test2.1, test3.1, test3.2, test2.2, test3.3

  Note: This function does not rebase your local dependent branches. Use it
        when you make a change to the parent branch that will not conflict
        with its dependent branches, and you would like their dependencies
        updated in Rietveld.
  """
  if git_common.is_dirty_git_tree('upload-branch-deps'):
    return 1

  root_branch = cl.GetBranch()
  if root_branch is None:
    DieWithError('Can\'t find dependent branches from detached HEAD state. '
                 'Get on a branch!')
  if not cl.GetIssue():
    DieWithError('Current branch does not have an uploaded CL. We cannot set '
                 'patchset dependencies without an uploaded CL.')

  branches = RunGit(['for-each-ref',
                     '--format=%(refname:short) %(upstream:short)',
                     'refs/heads'])
  if not branches:
    print('No local branches found.')
    return 0

  # Create a dictionary of all local branches to the branches that are
  # dependent on it.
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

  confirm_or_exit('This command will checkout all dependent branches and run '
                  '"git cl upload".', action='continue')

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
      except:  # pylint: disable=bare-except
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


@metrics.collector.collect_metrics('git cl archive')
def CMDarchive(parser, args):
  """Archives and deletes branches associated with closed changelists."""
  parser.add_option(
      '-j', '--maxjobs', action='store', type=int,
      help='The maximum number of jobs to use when retrieving review status.')
  parser.add_option(
      '-f', '--force', action='store_true',
      help='Bypasses the confirmation prompt.')
  parser.add_option(
      '-d', '--dry-run', action='store_true',
      help='Skip the branch tagging and removal steps.')
  parser.add_option(
      '-t', '--notags', action='store_true',
      help='Do not tag archived branches. '
           'Note: local commit history may be lost.')

  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported args: %s' % ' '.join(args))

  branches = RunGit(['for-each-ref', '--format=%(refname)', 'refs/heads'])
  if not branches:
    return 0

  print('Finding all branches associated with closed issues...')
  changes = [Changelist(branchref=b)
             for b in branches.splitlines()]
  alignment = max(5, max(len(c.GetBranch()) for c in changes))
  statuses = get_cl_statuses(changes,
                             fine_grained=True,
                             max_processes=options.maxjobs)
  proposal = [(cl.GetBranch(),
               'git-cl-archived-%s-%s' % (cl.GetIssue(), cl.GetBranch()))
              for cl, status in statuses
              if status in ('closed', 'rietveld-not-supported')]
  proposal.sort()

  if not proposal:
    print('No branches with closed codereview issues found.')
    return 0

  current_branch = GetCurrentBranch()

  print('\nBranches with closed issues that will be archived:\n')
  if options.notags:
    for next_item in proposal:
      print('  ' + next_item[0])
  else:
    print('%*s | %s' % (alignment, 'Branch name', 'Archival tag name'))
    for next_item in proposal:
      print('%*s   %s' % (alignment, next_item[0], next_item[1]))

  # Quit now on precondition failure or if instructed by the user, either
  # via an interactive prompt or by command line flags.
  if options.dry_run:
    print('\nNo changes were made (dry run).\n')
    return 0
  elif any(branch == current_branch for branch, _ in proposal):
    print('You are currently on a branch \'%s\' which is associated with a '
          'closed codereview issue, so archive cannot proceed. Please '
          'checkout another branch and run this command again.' %
          current_branch)
    return 1
  elif not options.force:
    answer = ask_for_data('\nProceed with deletion (Y/n)? ').lower()
    if answer not in ('y', ''):
      print('Aborted.')
      return 1

  for branch, tagname in proposal:
    if not options.notags:
      RunGit(['tag', tagname, branch])
    RunGit(['branch', '-D', branch])

  print('\nJob\'s done!')

  return 0


@metrics.collector.collect_metrics('git cl status')
def CMDstatus(parser, args):
  """Show status of changelists.

  Colors are used to tell the state of the CL unless --fast is used:
    - Blue     waiting for review
    - Yellow   waiting for you to reply to review, or not yet sent
    - Green    LGTM'ed
    - Red      'not LGTM'ed
    - Magenta  in the CQ
    - Cyan     was committed, branch can be deleted
    - White    error, or unknown status

  Also see 'git cl comments'.
  """
  parser.add_option(
      '--no-branch-color',
      action='store_true',
      help='Disable colorized branch names')
  parser.add_option('--field',
                    help='print only specific field (desc|id|patch|status|url)')
  parser.add_option('-f', '--fast', action='store_true',
                    help='Do not retrieve review status')
  parser.add_option(
      '-j', '--maxjobs', action='store', type=int,
      help='The maximum number of jobs to use when retrieving review status')

  _add_codereview_issue_select_options(
    parser, 'Must be in conjunction with --field.')
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  if args:
    parser.error('Unsupported args: %s' % args)

  if options.issue is not None and not options.field:
    parser.error('--field must be specified with --issue.')

  if options.field:
    cl = Changelist(issue=options.issue)
    if options.field.startswith('desc'):
      print(cl.GetDescription())
    elif options.field == 'id':
      issueid = cl.GetIssue()
      if issueid:
        print(issueid)
    elif options.field == 'patch':
      patchset = cl.GetMostRecentPatchset()
      if patchset:
        print(patchset)
    elif options.field == 'status':
      print(cl.GetStatus())
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
      Changelist(branchref=b)
      for b in branches.splitlines()]
  print('Branches associated with reviews:')
  output = get_cl_statuses(changes,
                           fine_grained=not options.fast,
                           max_processes=options.maxjobs)

  current_branch = GetCurrentBranch()

  def FormatBranchName(branch, colorize=False):
    """Simulates 'git branch' behavior. Colorizes and prefixes branch name with
    an asterisk when it is the current branch."""

    asterisk = ""
    color = Fore.RESET
    if branch == current_branch:
      asterisk = "* "
      color = Fore.GREEN
    branch_name = ShortBranchName(branch)

    if colorize:
      return asterisk + color + branch_name + Fore.RESET
    return asterisk + branch_name

  branch_statuses = {}

  alignment = max(5, max(len(FormatBranchName(c.GetBranch())) for c in changes))
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

    branch_display = FormatBranchName(branch)
    padding = ' ' * (alignment - len(branch_display))
    if not options.no_branch_color:
      branch_display = FormatBranchName(branch, colorize=True)

    print('  %s : %s%s %s%s' % (padding + branch_display, color, url,
                                status_str, reset))

  print()
  print('Current branch: %s' % current_branch)
  for cl in changes:
    if cl.GetBranch() == current_branch:
      break
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
        # Extract whitespace first and the leading '-'.
        indent = len(line) - len(line.lstrip(' ')) + 1
        return line[:indent] + getattr(Fore, color) + line[indent:] + Fore.RESET
    return line

  lines = CMDstatus.__doc__.splitlines()
  CMDstatus.__doc__ = '\n'.join(colorize_line(l) for l in lines)


def write_json(path, contents):
  if path == '-':
    json.dump(contents, sys.stdout)
  else:
    with open(path, 'w') as f:
      json.dump(contents, f)


@subcommand.usage('[issue_number]')
@metrics.collector.collect_metrics('git cl issue')
def CMDissue(parser, args):
  """Sets or displays the current code review issue number.

  Pass issue number 0 to clear the current issue.
  """
  parser.add_option('-r', '--reverse', action='store_true',
                    help='Lookup the branch(es) for the specified issues. If '
                         'no issues are specified, all branches with mapped '
                         'issues will be listed.')
  parser.add_option('--json',
                    help='Path to JSON output file, or "-" for stdout.')
  _add_codereview_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  if options.reverse:
    branches = RunGit(['for-each-ref', 'refs/heads',
                       '--format=%(refname)']).splitlines()
    # Reverse issue lookup.
    issue_branch_map = {}

    git_config = {}
    for config in RunGit(['config', '--get-regexp',
                          r'branch\..*issue']).splitlines():
      name, _space, val = config.partition(' ')
      git_config[name] = val

    for branch in branches:
      for cls in _CODEREVIEW_IMPLEMENTATIONS.values():
        config_key = _git_branch_config_key(ShortBranchName(branch),
                                            cls.IssueConfigKey())
        issue = git_config.get(config_key)
        if issue:
          issue_branch_map.setdefault(int(issue), []).append(branch)
    if not args:
      args = sorted(issue_branch_map.iterkeys())
    result = {}
    for issue in args:
      try:
        issue_num = int(issue)
      except ValueError:
        print('ERROR cannot parse issue number: %s' % issue, file=sys.stderr)
        continue
      result[issue_num] = issue_branch_map.get(issue_num)
      print('Branch for issue number %s: %s' % (
          issue, ', '.join(issue_branch_map.get(issue_num) or ('None',))))
    if options.json:
      write_json(options.json, result)
    return 0

  if len(args) > 0:
    issue = ParseIssueNumberArgument(args[0])
    if not issue.valid:
      DieWithError('Pass a url or number to set the issue, 0 to unset it, '
                   'or no argument to list it.\n'
                   'Maybe you want to run git cl status?')
    cl = Changelist()
    cl.SetIssue(issue.issue)
  else:
    cl = Changelist()
  print('Issue number: %s (%s)' % (cl.GetIssue(), cl.GetIssueURL()))
  if options.json:
    write_json(options.json, {
      'issue': cl.GetIssue(),
      'issue_url': cl.GetIssueURL(),
    })
  return 0


@metrics.collector.collect_metrics('git cl comments')
def CMDcomments(parser, args):
  """Shows or posts review comments for any changelist."""
  parser.add_option('-a', '--add-comment', dest='comment',
                    help='comment to add to an issue')
  parser.add_option('-p', '--publish', action='store_true',
                    help='marks CL as ready and sends comment to reviewers')
  parser.add_option('-i', '--issue', dest='issue',
                    help='review issue id (defaults to current issue).')
  parser.add_option('-m', '--machine-readable', dest='readable',
                    action='store_false', default=True,
                    help='output comments in a format compatible with '
                         'editor parsing')
  parser.add_option('-j', '--json-file',
                    help='File to write JSON summary to, or "-" for stdout')
  _add_codereview_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  issue = None
  if options.issue:
    try:
      issue = int(options.issue)
    except ValueError:
      DieWithError('A review issue ID is expected to be a number.')

  cl = Changelist(issue=issue)

  if options.comment:
    cl.AddComment(options.comment, options.publish)
    return 0

  summary = sorted(cl.GetCommentsSummary(readable=options.readable),
                   key=lambda c: c.date)
  for comment in summary:
    if comment.disapproval:
      color = Fore.RED
    elif comment.approval:
      color = Fore.GREEN
    elif comment.sender == cl.GetIssueOwner():
      color = Fore.MAGENTA
    elif comment.autogenerated:
      color = Fore.CYAN
    else:
      color = Fore.BLUE
    print('\n%s%s   %s%s\n%s' % (
      color,
      comment.date.strftime('%Y-%m-%d %H:%M:%S UTC'),
      comment.sender,
      Fore.RESET,
      '\n'.join('  ' + l for l in comment.message.strip().splitlines())))

  if options.json_file:
    def pre_serialize(c):
      dct = c.__dict__.copy()
      dct['date'] = dct['date'].strftime('%Y-%m-%d %H:%M:%S.%f')
      return dct
    write_json(options.json_file, map(pre_serialize, summary))
  return 0


@subcommand.usage('[codereview url or issue id]')
@metrics.collector.collect_metrics('git cl description')
def CMDdescription(parser, args):
  """Brings up the editor for the current CL's description."""
  parser.add_option('-d', '--display', action='store_true',
                    help='Display the description instead of opening an editor')
  parser.add_option('-n', '--new-description',
                    help='New description to set for this issue (- for stdin, '
                         '+ to load from local commit HEAD)')
  parser.add_option('-f', '--force', action='store_true',
                    help='Delete any unpublished Gerrit edits for this issue '
                         'without prompting')

  _add_codereview_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  target_issue_arg = None
  if len(args) > 0:
    target_issue_arg = ParseIssueNumberArgument(args[0])
    if not target_issue_arg.valid:
      parser.error('Invalid issue ID or URL.')

  kwargs = {}
  detected_codereview_from_url = False
  if target_issue_arg:
    kwargs['issue'] = target_issue_arg.issue
    kwargs['codereview_host'] = target_issue_arg.hostname
    if not args[0].isdigit() and not options.forced_codereview:
      detected_codereview_from_url = True

  cl = Changelist(**kwargs)
  if not cl.GetIssue():
    assert not detected_codereview_from_url
    DieWithError('This branch has no associated changelist.')

  if detected_codereview_from_url:
    logging.info('canonical issue/change URL: %s\n', cl.GetIssueURL())

  description = ChangeDescription(cl.GetDescription())

  if options.display:
    print(description.description)
    return 0

  if options.new_description:
    text = options.new_description
    if text == '-':
      text = '\n'.join(l.rstrip() for l in sys.stdin)
    elif text == '+':
      base_branch = cl.GetCommonAncestorWithUpstream()
      change = cl.GetChange(base_branch, None, local_description=True)
      text = change.FullDescriptionText()

    description.set_description(text)
  else:
    description.prompt()
  if cl.GetDescription().strip() != description.description:
    cl.UpdateDescription(description.description, force=options.force)
  return 0


@metrics.collector.collect_metrics('git cl lint')
def CMDlint(parser, args):
  """Runs cpplint on the current changelist."""
  parser.add_option('--filter', action='append', metavar='-x,+y',
                    help='Comma-separated list of cpplint\'s category-filters')
  options, args = parser.parse_args(args)

  # Access to a protected member _XX of a client class
  # pylint: disable=protected-access
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
    cl = Changelist()
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


@metrics.collector.collect_metrics('git cl presubmit')
def CMDpresubmit(parser, args):
  """Runs presubmit tests on the current changelist."""
  parser.add_option('-u', '--upload', action='store_true',
                    help='Run upload hook instead of the push hook')
  parser.add_option('-f', '--force', action='store_true',
                    help='Run checks even if tree is dirty')
  parser.add_option('--all', action='store_true',
                    help='Run checks against all files, not just modified ones')
  parser.add_option('--parallel', action='store_true',
                    help='Run all tests specified by input_api.RunTests in all '
                         'PRESUBMIT files in parallel.')
  options, args = parser.parse_args(args)

  if not options.force and git_common.is_dirty_git_tree('presubmit'):
    print('use --force to check even if tree is dirty.')
    return 1

  cl = Changelist()
  if args:
    base_branch = args[0]
  else:
    # Default to diffing against the common ancestor of the upstream branch.
    base_branch = cl.GetCommonAncestorWithUpstream()

  if options.all:
    base_change = cl.GetChange(base_branch, None)
    files = [('M', f) for f in base_change.AllFiles()]
    change = presubmit_support.GitChange(
        base_change.Name(),
        base_change.FullDescriptionText(),
        base_change.RepositoryRoot(),
        files,
        base_change.issue,
        base_change.patchset,
        base_change.author_email,
        base_change._upstream)
  else:
    change = cl.GetChange(base_branch, None)

  cl.RunHook(
      committing=not options.upload,
      may_prompt=False,
      verbose=options.verbose,
      change=change,
      parallel=options.parallel)
  return 0


def GenerateGerritChangeId(message):
  """Returns the Change ID footer value (Ixxxxxx...xxx).

  Works the same way as
  https://gerrit-review.googlesource.com/tools/hooks/commit-msg
  but can be called on demand on all platforms.

  The basic idea is to generate git hash of a state of the tree, original
  commit message, author/committer info and timestamps.
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
                           stdin=('\n'.join(lines)).encode())
  return 'I%s' % change_hash.strip()


def GetTargetRef(remote, remote_branch, target_branch):
  """Computes the remote branch ref to use for the CL.

  Args:
    remote (str): The git remote for the CL.
    remote_branch (str): The git remote branch for the CL.
    target_branch (str): The target branch specified by the user.
  """
  if not (remote and remote_branch):
    return None

  if target_branch:
    # Canonicalize branch references to the equivalent local full symbolic
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

  return remote_branch


def cleanup_list(l):
  """Fixes a list so that comma separated items are put as individual items.

  So that "--reviewers joe@c,john@c --reviewers joa@c" results in
  options.reviewers == sorted(['joe@c', 'john@c', 'joa@c']).
  """
  items = sum((i.split(',') for i in l), [])
  stripped_items = (i.strip() for i in items)
  return sorted(filter(None, stripped_items))


@subcommand.usage('[flags]')
@metrics.collector.collect_metrics('git cl upload')
def CMDupload(parser, args):
  """Uploads the current changelist to codereview.

  Can skip dependency patchset uploads for a branch by running:
    git config branch.branch_name.skip-deps-uploads True
  To unset, run:
    git config --unset branch.branch_name.skip-deps-uploads
  Can also set the above globally by using the --global flag.

  If the name of the checked out branch starts with "bug-" or "fix-" followed
  by a bug number, this bug number is automatically populated in the CL
  description.

  If subject contains text in square brackets or has "<text>: " prefix, such
  text(s) is treated as Gerrit hashtags. For example, CLs with subjects:
    [git-cl] add support for hashtags
    Foo bar: implement foo
  will be hashtagged with "git-cl" and "foo-bar" respectively.
  """
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('--bypass-watchlists', action='store_true',
                    dest='bypass_watchlists',
                    help='bypass watchlists auto CC-ing reviewers')
  parser.add_option('-f', '--force', action='store_true', dest='force',
                    help="force yes to questions (don't prompt)")
  parser.add_option('--message', '-m', dest='message',
                    help='message for patchset')
  parser.add_option('-b', '--bug',
                    help='pre-populate the bug number(s) for this issue. '
                         'If several, separate with commas')
  parser.add_option('--message-file', dest='message_file',
                    help='file which contains message for patchset')
  parser.add_option('--title', '-t', dest='title',
                    help='title for patchset')
  parser.add_option('-r', '--reviewers',
                    action='append', default=[],
                    help='reviewer email addresses')
  parser.add_option('--tbrs',
                    action='append', default=[],
                    help='TBR email addresses')
  parser.add_option('--cc',
                    action='append', default=[],
                    help='cc email addresses')
  parser.add_option('--hashtag', dest='hashtags',
                    action='append', default=[],
                    help=('Gerrit hashtag for new CL; '
                          'can be applied multiple times'))
  parser.add_option('-s', '--send-mail', action='store_true',
                    help='send email to reviewer(s) and cc(s) immediately')
  parser.add_option('--target_branch',
                    '--target-branch',
                    metavar='TARGET',
                    help='Apply CL to remote ref TARGET.  ' +
                         'Default: remote branch head, or master')
  parser.add_option('--squash', action='store_true',
                    help='Squash multiple commits into one')
  parser.add_option('--no-squash', action='store_false', dest='squash',
                    help='Don\'t squash multiple commits into one')
  parser.add_option('--topic', default=None,
                    help='Topic to specify when uploading')
  parser.add_option('--tbr-owners', dest='add_owners_to', action='store_const',
                    const='TBR', help='add a set of OWNERS to TBR')
  parser.add_option('--r-owners', dest='add_owners_to', action='store_const',
                    const='R', help='add a set of OWNERS to R')
  parser.add_option('-c', '--use-commit-queue', action='store_true',
                    help='tell the CQ to commit this patchset; '
                          'implies --send-mail')
  parser.add_option('-d', '--cq-dry-run', dest='cq_dry_run',
                    action='store_true',
                    help='Send the patchset to do a CQ dry run right after '
                         'upload.')
  parser.add_option('--preserve-tryjobs', action='store_true',
                    help='instruct the CQ to let tryjobs running even after '
                         'new patchsets are uploaded instead of canceling '
                         'prior patchset\' tryjobs')
  parser.add_option('--dependencies', action='store_true',
                    help='Uploads CLs of all the local branches that depend on '
                         'the current branch')
  parser.add_option('-a', '--enable-auto-submit', action='store_true',
                    help='Sends your change to the CQ after an approval. Only '
                         'works on repos that have the Auto-Submit label '
                         'enabled')
  parser.add_option('--parallel', action='store_true',
                    help='Run all tests specified by input_api.RunTests in all '
                         'PRESUBMIT files in parallel.')

  parser.add_option('--no-autocc', action='store_true',
                    help='Disables automatic addition of CC emails')
  parser.add_option('--private', action='store_true',
                    help='Set the review private. This implies --no-autocc.')

  orig_args = args
  _add_codereview_select_options(parser)
  (options, args) = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  if git_common.is_dirty_git_tree('upload'):
    return 1

  options.reviewers = cleanup_list(options.reviewers)
  options.tbrs = cleanup_list(options.tbrs)
  options.cc = cleanup_list(options.cc)

  if options.message_file:
    if options.message:
      parser.error('Only one of --message and --message-file allowed.')
    options.message = gclient_utils.FileRead(options.message_file)
    options.message_file = None

  if options.cq_dry_run and options.use_commit_queue:
    parser.error('Only one of --use-commit-queue and --cq-dry-run allowed.')

  if options.use_commit_queue:
    options.send_mail = True

  # For sanity of test expectations, do this otherwise lazy-loading *now*.
  settings.GetIsGerrit()

  cl = Changelist()

  return cl.CMDUpload(options, args, orig_args)


@subcommand.usage('--description=<description file>')
@metrics.collector.collect_metrics('git cl split')
def CMDsplit(parser, args):
  """Splits a branch into smaller branches and uploads CLs.

  Creates a branch and uploads a CL for each group of files modified in the
  current branch that share a common OWNERS file. In the CL description and
  comment, the string '$directory', is replaced with the directory containing
  the shared OWNERS file.
  """
  parser.add_option('-d', '--description', dest='description_file',
                    help='A text file containing a CL description in which '
                         '$directory will be replaced by each CL\'s directory.')
  parser.add_option('-c', '--comment', dest='comment_file',
                    help='A text file containing a CL comment.')
  parser.add_option('-n', '--dry-run', dest='dry_run', action='store_true',
                    default=False,
                    help='List the files and reviewers for each CL that would '
                         'be created, but don\'t create branches or CLs.')
  parser.add_option('--cq-dry-run', action='store_true',
                    help='If set, will do a cq dry run for each uploaded CL. '
                         'Please be careful when doing this; more than ~10 CLs '
                         'has the potential to overload our build '
                         'infrastructure. Try to upload these not during high '
                         'load times (usually 11-3 Mountain View time). Email '
                         'infra-dev@chromium.org with any questions.')
  parser.add_option('-a', '--enable-auto-submit', action='store_true',
                    default=True,
                    help='Sends your change to the CQ after an approval. Only '
                         'works on repos that have the Auto-Submit label '
                         'enabled')
  options, _ = parser.parse_args(args)

  if not options.description_file:
    parser.error('No --description flag specified.')

  def WrappedCMDupload(args):
    return CMDupload(OptionParser(), args)

  return split_cl.SplitCl(options.description_file, options.comment_file,
                          Changelist, WrappedCMDupload, options.dry_run,
                          options.cq_dry_run, options.enable_auto_submit)


@subcommand.usage('DEPRECATED')
@metrics.collector.collect_metrics('git cl commit')
def CMDdcommit(parser, args):
  """DEPRECATED: Used to commit the current changelist via git-svn."""
  message = ('git-cl no longer supports committing to SVN repositories via '
             'git-svn. You probably want to use `git cl land` instead.')
  print(message)
  return 1


# Two special branches used by git cl land.
MERGE_BRANCH = 'git-cl-commit'
CHERRY_PICK_BRANCH = 'git-cl-cherry-pick'


@subcommand.usage('[upstream branch to apply against]')
@metrics.collector.collect_metrics('git cl land')
def CMDland(parser, args):
  """Commits the current changelist via git.

  In case of Gerrit, uses Gerrit REST api to "submit" the issue, which pushes
  upstream and closes the issue automatically and atomically.
  """
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('-f', '--force', action='store_true', dest='force',
                    help="force yes to questions (don't prompt)")
  parser.add_option('--parallel', action='store_true',
                    help='Run all tests specified by input_api.RunTests in all '
                         'PRESUBMIT files in parallel.')
  (options, args) = parser.parse_args(args)

  cl = Changelist()

  if not cl.GetIssue():
    DieWithError('You must upload the change first to Gerrit.\n'
                 '  If you would rather have `git cl land` upload '
                 'automatically for you, see http://crbug.com/642759')
  return cl.CMDLand(options.force, options.bypass_hooks,
                                     options.verbose, options.parallel)


@subcommand.usage('<patch url or issue id or issue url>')
@metrics.collector.collect_metrics('git cl patch')
def CMDpatch(parser, args):
  """Patches in a code review."""
  parser.add_option('-b', dest='newbranch',
                    help='create a new branch off trunk for the patch')
  parser.add_option('-f', '--force', action='store_true',
                    help='overwrite state on the current or chosen branch')
  parser.add_option('-n', '--no-commit', action='store_true', dest='nocommit',
                    help='don\'t commit after patch applies.')

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

  _add_codereview_select_options(parser)
  (options, args) = parser.parse_args(args)
  _process_codereview_select_options(parser, options)

  if options.reapply:
    if options.newbranch:
      parser.error('--reapply works on the current branch only.')
    if len(args) > 0:
      parser.error('--reapply implies no additional arguments.')

    cl = Changelist()
    if not cl.GetIssue():
      parser.error('Current branch must have an associated issue.')

    upstream = cl.GetUpstreamBranch()
    if upstream is None:
      parser.error('No upstream branch specified. Cannot reset branch.')

    RunGit(['reset', '--hard', upstream])
    if options.pull:
      RunGit(['pull'])

    return cl.CMDPatchIssue(cl.GetIssue(), options.nocommit)

  if len(args) != 1 or not args[0]:
    parser.error('Must specify issue number or URL.')

  target_issue_arg = ParseIssueNumberArgument(args[0])
  if not target_issue_arg.valid:
    parser.error('Invalid issue ID or URL.')

  cl_kwargs = {
      'codereview_host': target_issue_arg.hostname,
  }
  detected_codereview_from_url = False
  if not args[0].isdigit() and not options.forced_codereview:
    detected_codereview_from_url = True
    cl_kwargs['issue'] = target_issue_arg.issue

  # We don't want uncommitted changes mixed up with the patch.
  if git_common.is_dirty_git_tree('patch'):
    return 1

  if options.newbranch:
    if options.force:
      RunGit(['branch', '-D', options.newbranch],
             stderr=subprocess2.PIPE, error_ok=True)
    RunGit(['new-branch', options.newbranch])

  cl = Changelist(**cl_kwargs)

  if detected_codereview_from_url:
    print('canonical issue/change URL: %s\n' % cl.GetIssueURL())

  return cl.CMDPatchWithParsedIssue(
      target_issue_arg, options.nocommit, options.force)


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


@metrics.collector.collect_metrics('git cl tree')
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


@metrics.collector.collect_metrics('git cl try')
def CMDtry(parser, args):
  """Triggers tryjobs using either Buildbucket or CQ dry run."""
  group = optparse.OptionGroup(parser, 'Tryjob options')
  group.add_option(
      '-b', '--bot', action='append',
      help=('IMPORTANT: specify ONE builder per --bot flag. Use it multiple '
            'times to specify multiple builders. ex: '
            '"-b win_rel -b win_layout". See '
            'the try server waterfall for the builders name and the tests '
            'available.'))
  group.add_option(
      '-B', '--bucket', default='',
      help=('Buildbucket bucket to send the try requests.'))
  group.add_option(
      '-r', '--revision',
      help='Revision to use for the tryjob; default: the revision will '
           'be determined by the try recipe that builder runs, which usually '
           'defaults to HEAD of origin/master')
  group.add_option(
      '-c', '--clobber', action='store_true', default=False,
      help='Force a clobber before building; that is don\'t do an '
           'incremental build')
  group.add_option(
      '--category', default='git_cl_try', help='Specify custom build category.')
  group.add_option(
      '--project',
      help='Override which project to use. Projects are defined '
           'in recipe to determine to which repository or directory to '
           'apply the patch')
  group.add_option(
      '-p', '--property', dest='properties', action='append', default=[],
      help='Specify generic properties in the form -p key1=value1 -p '
           'key2=value2 etc. The value will be treated as '
           'json if decodable, or as string otherwise. '
           'NOTE: using this may make your tryjob not usable for CQ, '
           'which will then schedule another tryjob with default properties')
  group.add_option(
      '--buildbucket-host', default='cr-buildbucket.appspot.com',
      help='Host of buildbucket. The default host is %default.')
  parser.add_option_group(group)
  auth.add_auth_options(parser)
  _add_codereview_issue_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  auth_config = auth.extract_auth_config_from_options(options)

  # Make sure that all properties are prop=value pairs.
  bad_params = [x for x in options.properties if '=' not in x]
  if bad_params:
    parser.error('Got properties with missing "=": %s' % bad_params)

  if args:
    parser.error('Unknown arguments: %s' % args)

  cl = Changelist(issue=options.issue)
  if not cl.GetIssue():
    parser.error('Need to upload first.')

  # HACK: warm up Gerrit change detail cache to save on RPCs.
  cl._GetChangeDetail(['DETAILED_ACCOUNTS', 'ALL_REVISIONS'])

  error_message = cl.CannotTriggerTryJobReason()
  if error_message:
    parser.error('Can\'t trigger tryjobs: %s' % error_message)

  buckets = _get_bucket_map(cl, options, parser)
  if buckets and any(b.startswith('master.') for b in buckets):
    print('ERROR: Buildbot masters are not supported.')
    return 1

  # If no bots are listed and we couldn't get a list based on PRESUBMIT files,
  # then we default to triggering a CQ dry run (see http://crbug.com/625697).
  if not buckets:
    if options.verbose:
      print('git cl try with no bots now defaults to CQ dry run.')
    print('Scheduling CQ dry run on: %s' % cl.GetIssueURL())
    return cl.SetCQState(_CQState.DRY_RUN)

  for builders in buckets.itervalues():
    if any('triggered' in b for b in builders):
      print('ERROR You are trying to send a job to a triggered bot. This type '
            'of bot requires an initial job from a parent (usually a builder). '
            'Instead send your job to the parent.\n'
            'Bot list: %s' % builders, file=sys.stderr)
      return 1

  patchset = cl.GetMostRecentPatchset()
  try:
    _trigger_try_jobs(auth_config, cl, buckets, options, patchset)
  except BuildbucketResponseException as ex:
    print('ERROR: %s' % ex)
    return 1
  return 0


@metrics.collector.collect_metrics('git cl try-results')
def CMDtry_results(parser, args):
  """Prints info about results for tryjobs associated with the current CL."""
  group = optparse.OptionGroup(parser, 'Tryjob results options')
  group.add_option(
      '-p', '--patchset', type=int, help='patchset number if not current.')
  group.add_option(
      '--print-master', action='store_true', help='print master name as well.')
  group.add_option(
      '--color', action='store_true', default=setup_color.IS_TTY,
      help='force color output, useful when piping output.')
  group.add_option(
      '--buildbucket-host', default='cr-buildbucket.appspot.com',
      help='Host of buildbucket. The default host is %default.')
  group.add_option(
      '--json', help=('Path of JSON output file to write tryjob results to,'
                      'or "-" for stdout.'))
  parser.add_option_group(group)
  auth.add_auth_options(parser)
  _add_codereview_issue_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  auth_config = auth.extract_auth_config_from_options(options)
  cl = Changelist(issue=options.issue)
  if not cl.GetIssue():
    parser.error('Need to upload first.')

  patchset = options.patchset
  if not patchset:
    patchset = cl.GetMostRecentPatchset()
    if not patchset:
      parser.error('Code review host doesn\'t know about issue %s. '
                   'No access to issue or wrong issue number?\n'
                   'Either upload first, or pass --patchset explicitly.' %
                   cl.GetIssue())

  try:
    jobs = fetch_try_jobs(auth_config, cl, options.buildbucket_host, patchset)
  except BuildbucketResponseException as ex:
    print('Buildbucket error: %s' % ex)
    return 1
  if options.json:
    write_try_results_json(options.json, jobs)
  else:
    print_try_jobs(options, jobs)
  return 0


@subcommand.usage('[new upstream branch]')
@metrics.collector.collect_metrics('git cl upstream')
def CMDupstream(parser, args):
  """Prints or sets the name of the upstream branch, if any."""
  _, args = parser.parse_args(args)
  if len(args) > 1:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  cl = Changelist()
  if args:
    # One arg means set upstream branch.
    branch = cl.GetBranch()
    RunGit(['branch', '--set-upstream-to', args[0], branch])
    cl = Changelist()
    print('Upstream branch set to %s' % (cl.GetUpstreamBranch(),))

    # Clear configured merge-base, if there is one.
    git_common.remove_merge_base(branch)
  else:
    print(cl.GetUpstreamBranch())
  return 0


@metrics.collector.collect_metrics('git cl web')
def CMDweb(parser, args):
  """Opens the current CL in the web browser."""
  _, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  issue_url = Changelist().GetIssueURL()
  if not issue_url:
    print('ERROR No issue to open', file=sys.stderr)
    return 1

  # Redirect I/O before invoking browser to hide its output. For example, this
  # allows us to hide the "Created new window in existing browser session."
  # message from Chrome. Based on https://stackoverflow.com/a/2323563.
  saved_stdout = os.dup(1)
  saved_stderr = os.dup(2)
  os.close(1)
  os.close(2)
  os.open(os.devnull, os.O_RDWR)
  try:
    webbrowser.open(issue_url)
  finally:
    os.dup2(saved_stdout, 1)
    os.dup2(saved_stderr, 2)
  return 0


@metrics.collector.collect_metrics('git cl set-commit')
def CMDset_commit(parser, args):
  """Sets the commit bit to trigger the CQ."""
  parser.add_option('-d', '--dry-run', action='store_true',
                    help='trigger in dry run mode')
  parser.add_option('-c', '--clear', action='store_true',
                    help='stop CQ run, if any')
  _add_codereview_issue_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))
  if options.dry_run and options.clear:
    parser.error('Only one of --dry-run and --clear are allowed.')

  cl = Changelist(issue=options.issue)
  if options.clear:
    state = _CQState.NONE
  elif options.dry_run:
    state = _CQState.DRY_RUN
  else:
    state = _CQState.COMMIT
  if not cl.GetIssue():
    parser.error('Must upload the issue first.')
  cl.SetCQState(state)
  return 0


@metrics.collector.collect_metrics('git cl set-close')
def CMDset_close(parser, args):
  """Closes the issue."""
  _add_codereview_issue_select_options(parser)
  options, args = parser.parse_args(args)
  _process_codereview_select_options(parser, options)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))
  cl = Changelist(issue=options.issue)
  # Ensure there actually is an issue to close.
  if not cl.GetIssue():
    DieWithError('ERROR: No issue to close.')
  cl.CloseIssue()
  return 0


@metrics.collector.collect_metrics('git cl diff')
def CMDdiff(parser, args):
  """Shows differences between local tree and last upload."""
  parser.add_option(
      '--stat',
      action='store_true',
      dest='stat',
      help='Generate a diffstat')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  cl = Changelist()
  issue = cl.GetIssue()
  branch = cl.GetBranch()
  if not issue:
    DieWithError('No issue found for current branch (%s)' % branch)

  base = cl._GitGetBranchConfigValue('last-upload-hash')
  if not base:
    base = cl._GitGetBranchConfigValue('gerritsquashhash')
  if not base:
    detail = cl._GetChangeDetail(['CURRENT_REVISION', 'CURRENT_COMMIT'])
    revision_info = detail['revisions'][detail['current_revision']]
    fetch_info = revision_info['fetch']['http']
    RunGit(['fetch', fetch_info['url'], fetch_info['ref']])
    base = 'FETCH_HEAD'

  cmd = ['git', 'diff']
  if options.stat:
    cmd.append('--stat')
  cmd.append(base)
  subprocess2.check_call(cmd)

  return 0


@metrics.collector.collect_metrics('git cl owners')
def CMDowners(parser, args):
  """Finds potential owners for reviewing."""
  parser.add_option(
      '--ignore-current',
      action='store_true',
      help='Ignore the CL\'s current reviewers and start from scratch.')
  parser.add_option(
      '--ignore-self',
      action='store_true',
      help='Do not consider CL\'s author as an owners.')
  parser.add_option(
      '--no-color',
      action='store_true',
      help='Use this option to disable color output')
  parser.add_option(
      '--batch',
      action='store_true',
      help='Do not run interactively, just suggest some')
  # TODO: Consider moving this to another command, since other
  #       git-cl owners commands deal with owners for a given CL.
  parser.add_option(
      '--show-all',
      action='store_true',
      help='Show all owners for a particular file')
  options, args = parser.parse_args(args)

  author = RunGit(['config', 'user.email']).strip() or None

  cl = Changelist()

  if options.show_all:
    for arg in args:
      base_branch = cl.GetCommonAncestorWithUpstream()
      change = cl.GetChange(base_branch, None)
      database = owners.Database(change.RepositoryRoot(), file, os.path)
      database.load_data_needed_for([arg])
      print('Owners for %s:' % arg)
      for owner in sorted(database.all_possible_owners([arg], None)):
        print(' - %s' % owner)
    return 0

  if args:
    if len(args) > 1:
      parser.error('Unknown args.')
    base_branch = args[0]
  else:
    # Default to diffing against the common ancestor of the upstream branch.
    base_branch = cl.GetCommonAncestorWithUpstream()

  change = cl.GetChange(base_branch, None)
  affected_files = [f.LocalPath() for f in change.AffectedFiles()]

  if options.batch:
    db = owners.Database(change.RepositoryRoot(), file, os.path)
    print('\n'.join(db.reviewers_for(affected_files, author)))
    return 0

  return owners_finder.OwnersFinder(
      affected_files,
      change.RepositoryRoot(),
      author,
      [] if options.ignore_current else cl.GetReviewers(),
      fopen=file, os_path=os.path,
      disable_color=options.no_color,
      override_files=change.OriginalOwnersFiles(),
      ignore_author=options.ignore_self).run()


def BuildGitDiffCmd(diff_type, upstream_commit, args, allow_prefix=False):
  """Generates a diff command."""
  # Generate diff for the current branch's changes.
  diff_cmd = ['-c', 'core.quotePath=false', 'diff', '--no-ext-diff']

  if allow_prefix:
    # explicitly setting --src-prefix and --dst-prefix is necessary in the
    # case that diff.noprefix is set in the user's git config.
    diff_cmd += ['--src-prefix=a/', '--dst-prefix=b/']
  else:
    diff_cmd += ['--no-prefix']

  diff_cmd += [diff_type, upstream_commit, '--']

  if args:
    for arg in args:
      if os.path.isdir(arg) or os.path.isfile(arg):
        diff_cmd.append(arg)
      else:
        DieWithError('Argument "%s" is not a file or a directory' % arg)

  return diff_cmd


def MatchingFileType(file_name, extensions):
  """Returns True if the file name ends with one of the given extensions."""
  return bool([ext for ext in extensions if file_name.lower().endswith(ext)])


@subcommand.usage('[files or directories to diff]')
@metrics.collector.collect_metrics('git cl format')
def CMDformat(parser, args):
  """Runs auto-formatting tools (clang-format etc.) on the diff."""
  CLANG_EXTS = ['.cc', '.cpp', '.h', '.m', '.mm', '.proto', '.java']
  GN_EXTS = ['.gn', '.gni', '.typemap']
  parser.add_option('--full', action='store_true',
                    help='Reformat the full content of all touched files')
  parser.add_option('--dry-run', action='store_true',
                    help='Don\'t modify any file on disk.')
  parser.add_option(
      '--python',
      action='store_true',
      default=None,
      help='Enables python formatting on all python files.')
  parser.add_option(
      '--no-python',
      action='store_true',
      dest='python',
      help='Disables python formatting on all python files. '
      'Takes precedence over --python. '
      'If neither --python or --no-python are set, python '
      'files that have a .style.yapf file in an ancestor '
      'directory will be formatted.')
  parser.add_option('--js', action='store_true',
                    help='Format javascript code with clang-format.')
  parser.add_option('--diff', action='store_true',
                    help='Print diff to stdout rather than modifying files.')
  parser.add_option('--presubmit', action='store_true',
                    help='Used when running the script from a presubmit.')
  opts, args = parser.parse_args(args)

  # Normalize any remaining args against the current path, so paths relative to
  # the current directory are still resolved as expected.
  args = [os.path.join(os.getcwd(), arg) for arg in args]

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

  if opts.js:
    CLANG_EXTS.extend(['.js', '.ts'])

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
  py_explicitly_disabled = opts.python is not None and not opts.python
  if python_diff_files and not py_explicitly_disabled:
    depot_tools_path = os.path.dirname(os.path.abspath(__file__))
    yapf_tool = os.path.join(depot_tools_path, 'yapf')
    if sys.platform.startswith('win'):
      yapf_tool += '.bat'

    # If we couldn't find a yapf file we'll default to the chromium style
    # specified in depot_tools.
    chromium_default_yapf_style = os.path.join(depot_tools_path,
                                               YAPF_CONFIG_FILENAME)
    # Used for caching.
    yapf_configs = {}
    for f in python_diff_files:
      # Find the yapf style config for the current file, defaults to depot
      # tools default.
      _FindYapfConfigFile(f, yapf_configs, top_dir)

    # Turn on python formatting by default if a yapf config is specified.
    # This breaks in the case of this repo though since the specified
    # style file is also the global default.
    if opts.python is None:
      filtered_py_files = []
      for f in python_diff_files:
        if _FindYapfConfigFile(f, yapf_configs, top_dir) is not None:
          filtered_py_files.append(f)
    else:
      filtered_py_files = python_diff_files

    # Note: yapf still seems to fix indentation of the entire file
    # even if line ranges are specified.
    # See https://github.com/google/yapf/issues/499
    if not opts.full and filtered_py_files:
      py_line_diffs = _ComputeDiffLineRanges(filtered_py_files, upstream_commit)

    for f in filtered_py_files:
      yapf_config = _FindYapfConfigFile(f, yapf_configs, top_dir)
      if yapf_config is None:
        yapf_config = chromium_default_yapf_style

      cmd = [yapf_tool, '--style', yapf_config, f]

      has_formattable_lines = False
      if not opts.full:
        # Only run yapf over changed line ranges.
        for diff_start, diff_len in py_line_diffs[f]:
          diff_end = diff_start + diff_len - 1
          # Yapf errors out if diff_end < diff_start but this
          # is a valid line range diff for a removal.
          if diff_end >= diff_start:
            has_formattable_lines = True
            cmd += ['-l', '{}-{}'.format(diff_start, diff_end)]
        # If all line diffs were removals we have nothing to format.
        if not has_formattable_lines:
          continue

      if opts.diff or opts.dry_run:
        cmd += ['--diff']
        # Will return non-zero exit code if non-empty diff.
        stdout = RunCommand(cmd, error_ok=True, cwd=top_dir)
        if opts.diff:
          sys.stdout.write(stdout)
        elif len(stdout) > 0:
          return_value = 2
      else:
        cmd += ['-i']
        RunCommand(cmd, cwd=top_dir)

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
    if opts.dry_run or opts.diff:
      cmd.append('--dry-run')
    for gn_diff_file in gn_diff_files:
      gn_ret = subprocess2.call(cmd + [gn_diff_file],
                                shell=sys.platform == 'win32',
                                cwd=top_dir)
      if opts.dry_run and gn_ret == 2:
        return_value = 2  # Not formatted.
      elif opts.diff and gn_ret == 2:
        # TODO this should compute and print the actual diff.
        print('This change has GN build file diff for ' + gn_diff_file)
      elif gn_ret != 0:
        # For non-dry run cases (and non-2 return values for dry-run), a
        # nonzero error code indicates a failure, probably because the file
        # doesn't parse.
        DieWithError('gn format failed on ' + gn_diff_file +
                     '\nTry running `gn format` on this file manually.')

  # Skip the metrics formatting from the global presubmit hook. These files have
  # a separate presubmit hook that issues an error if the files need formatting,
  # whereas the top-level presubmit script merely issues a warning. Formatting
  # these files is somewhat slow, so it's important not to duplicate the work.
  if not opts.presubmit:
    for xml_dir in GetDirtyMetricsDirs(diff_files):
      tool_dir = os.path.join(top_dir, xml_dir)
      cmd = [os.path.join(tool_dir, 'pretty_print.py'), '--non-interactive']
      if opts.dry_run or opts.diff:
        cmd.append('--diff')
      stdout = RunCommand(cmd, cwd=top_dir)
      if opts.diff:
        sys.stdout.write(stdout)
      if opts.dry_run and stdout:
        return_value = 2  # Not formatted.

  return return_value


def GetDirtyMetricsDirs(diff_files):
  xml_diff_files = [x for x in diff_files if MatchingFileType(x, ['.xml'])]
  metrics_xml_dirs = [
    os.path.join('tools', 'metrics', 'actions'),
    os.path.join('tools', 'metrics', 'histograms'),
    os.path.join('tools', 'metrics', 'rappor'),
    os.path.join('tools', 'metrics', 'ukm'),
  ]
  for xml_dir in metrics_xml_dirs:
    if any(file.startswith(xml_dir) for file in xml_diff_files):
      yield xml_dir


@subcommand.usage('<codereview url or issue id>')
@metrics.collector.collect_metrics('git cl checkout')
def CMDcheckout(parser, args):
  """Checks out a branch associated with a given Gerrit issue."""
  _, args = parser.parse_args(args)

  if len(args) != 1:
    parser.print_help()
    return 1

  issue_arg = ParseIssueNumberArgument(args[0])
  if not issue_arg.valid:
    parser.error('Invalid issue ID or URL.')

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
    branches.extend(find_issues(cls.IssueConfigKey()))
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

  def parse_args(self, args=None, _values=None):
    try:
      return self._parse_args(args)
    finally:
      # Regardless of success or failure of args parsing, we want to report
      # metrics, but only after logging has been initialized (if parsing
      # succeeded).
      global settings
      settings = Settings()

      if not metrics.DISABLE_METRICS_COLLECTION:
        # GetViewVCUrl ultimately calls logging method.
        project_url = settings.GetViewVCUrl().strip('/+')
        if project_url in metrics_utils.KNOWN_PROJECT_URLS:
          metrics.collector.add('project_urls', [project_url])

  def _parse_args(self, args=None):
    # Create an optparse.Values object that will store only the actual passed
    # options, without the defaults.
    actual_options = optparse.Values()
    _, args = optparse.OptionParser.parse_args(self, args, actual_options)
    # Create an optparse.Values object with the default options.
    options = optparse.Values(self.get_default_values().__dict__)
    # Update it with the options passed by the user.
    options._update_careful(actual_options.__dict__)
    # Store the options passed by the user in an _actual_options attribute.
    # We store only the keys, and not the values, since the values can contain
    # arbitrary information, which might be PII.
    metrics.collector.add('arguments', actual_options.__dict__.keys())

    levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(
        level=levels[min(options.verbose, len(levels) - 1)],
        format='[%(levelname).1s%(asctime)s %(process)d %(thread)d '
               '%(filename)s] %(message)s')

    return options, args


def main(argv):
  if sys.hexversion < 0x02060000:
    print('\nYour Python version %s is unsupported, please upgrade.\n' %
          (sys.version.split(' ', 1)[0],), file=sys.stderr)
    return 2

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
        ('App Engine is misbehaving and returned HTTP %d, again. Keep faith '
         'and retry or visit go/isgaeup.\n%s') % (e.code, str(e)))
  return 0


if __name__ == '__main__':
  # These affect sys.stdout, so do it outside of main() to simplify mocks in
  # the unit tests.
  fix_encoding.fix_encoding()
  setup_color.init()
  with metrics.collector.print_notice_and_exit():
    sys.exit(main(sys.argv[1:]))
