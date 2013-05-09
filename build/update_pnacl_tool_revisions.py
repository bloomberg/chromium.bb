#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import datetime
import email.mime.text
import getpass
import os
import re
import smtplib
import subprocess
import sys
import tempfile
import urllib2

import download_toolchains
import toolchainbinaries


def ParseArgs(args):
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description="""Update TOOL_REVISIONS' PNaCl version.

LLVM is checked-in to the NaCl repository, but its head isn't
necessarily the one that we currently use in PNaCl. The TOOL_REVISIONS
file points at subversion revisions to use for tools such as LLVM. Our
build process then downloads pre-built tool tarballs from the toolchain
build waterfall.

git repository before running this script:
         ______________________
         |                    |
         v                    |
  ...----a------b------c------d------ NaCl HEAD
         ^      ^      ^      ^
         |      |      |      |__ Latest TOOL_REVISIONS update.
         |      |      |
         |      |      |__ A newer LLVM change (LLVM repository HEAD).
         |      |
         |      |__ Oldest LLVM change since this PNaCl version.
         |
         |__ TOOL_REVISIONS points at an older LLVM change.

git repository after running this script:
                ______________________
                |                    |
                v                    |
  ...----a------b------c------d------e------ NaCl HEAD

Note that there could be any number of non-LLVM changes between each of
these changelists.

Performance measurements are done on the current NaCl tree, so we want
to pull in changes under ./pnacl/ individually and let the performance
bots measure their effect before updating to the next TOOL_REVISIONS.

There is further complication when toolchain builds are merged.
""")
  parser.add_argument('--email', metavar='ADDRESS', type=str,
                      default=getpass.getuser()+'@chromium.org',
                      help="Email address to send errors to.")
  parser.add_argument('--commits_since', metavar='N', type=int, default=3,
                      help="Commits to wait between TOOL_REVISIONS updates.")
  parser.add_argument('--overwrite_pnacl_svn_id', metavar='SVN_ID', type=int,
                      help="Don't update to the oldest PNaCl change, instead "
                      "update to the given svn id. Note that this value isn't "
                      "checked and is assumed to be correct.")
  # TODO(jfb) The following options come from download_toolchain.py and
  #           should be shared in some way.
  parser.add_argument('--filter_out_predicates', default=[],
                      help="Toolchains to filter out.")
  parser.add_argument('-b', '--base-url', dest='base_url',
                      default=toolchainbinaries.BASE_DOWNLOAD_URL,
                      help="Base url to download from.")
  parser.add_argument('--base-once-url', dest='base_once_url',
                      default=toolchainbinaries.BASE_ONCE_DOWNLOAD_URL,
                      help="Base url to download new toolchain "
                      "artifacts from.")
  return parser.parse_args()


def ExecCommand(command):
  try:
    return subprocess.check_output(command, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    sys.stderr.write('\nRunning `%s` returned %i, got:\n%s\n' %
                     (' '.join(e.cmd), e.returncode, e.output))
    raise


def GitCurrentBranch():
  return ExecCommand(['git', 'symbolic-ref', 'HEAD', '--short']).strip()


def GitStatus():
  """List of statuses, one per path, of paths in the current git branch.
  Ignores untracked paths."""
  out = ExecCommand(['git', 'status', '--porcelain']).strip().split('\n')
  return [f.strip() for f in out if not re.match('^\?\? (.*)$', f.strip())]


def SyncSources():
  """Assumes a git-svn checkout of NaCl. See:
  www.chromium.org/nativeclient/how-tos/how-to-use-git-svn-with-native-client
  """
  ExecCommand(['gclient', 'sync'])


def GitCommitInfo(info='', obj=None, num=None, extra=[]):
  """Commit information, where info is one of the shorthands in git_formats.
  obj can be a path or a hash.
  num is the number of results to return.
  extra is a list of optional extra arguments."""
  # Shorthands for git's pretty formats.
  # See PRETTY FORMATS format:<string> in `git help log`.
  git_formats = {
      '':        '',
      'hash':    '%H',
      'date':    '%ci',
      'author':  '%aN',
      'subject': '%s',
      'body':    '%b',
  }
  cmd = ['git', 'log', '--format=format:%s' % git_formats[info]] + extra
  if num: cmd += ['-n'+str(num)]
  if obj: cmd += [obj]
  return ExecCommand(cmd).strip()


def GitCommitsSince(date):
  """List of commit hashes since a particular date,
  in reverse chronological order."""
  return GitCommitInfo(info='hash',
                       extra=['--since="%s"' % date]).split('\n')


def GitFilesChanged(commit_hash):
  """List of files changed in a commit."""
  return GitCommitInfo(obj=commit_hash, num=1,
                       extra=['--name-only']).split('\n')


def GitOldestCommitChangingPath(commit_hashes, path):
  """Returns the oldest commit changing a file under path.
  Assumes the commit hashes are in reverse-chronological order."""
  for commit_hash in reversed(commit_hashes):
    files = GitFilesChanged(commit_hash)
    for file in files:
      if re.search('^' + path, file.strip()):
        return commit_hash
  return None


def GitBranchExists(name):
  return len(ExecCommand(['git', 'branch', '--list', name]).strip()) != 0


def GitCheckout(branch, force=False):
  """Checkout an existing branch.
  force throws away local changes."""
  ExecCommand(['git', 'checkout'] +
              (['--force'] if force else []) +
              [branch])


def GitCheckoutNewBranch(branch):
  """Create and checkout a new git branch."""
  ExecCommand(['git', 'checkout', '-b', branch])


def GitDeleteBranch(branch, force=False):
  """Force-delete a branch."""
  ExecCommand(['git', 'branch', '-D' if force else '-d', branch])


def GitAdd(file):
  ExecCommand(['git', 'add', file])


def GitCommit(message):
  with tempfile.NamedTemporaryFile() as tmp:
    tmp.write(message)
    tmp.flush()
    ExecCommand(['git', 'commit', '--file=%s' % tmp.name])


def UploadChanges():
  """Upload changes, don't prompt."""
  # TODO(jfb) Using the commit queue and avoiding git try + manual commit
  #           would be much nicer. See '--use-commit-queue'
  return ExecCommand(['git', 'cl', 'upload', '--send-mail', '-f'])


def GitTry():
  return ExecCommand(['git', 'try'])


def FindCommitWithGitSvnId(git_svn_id):
  out = ExecCommand(['git', 'svn', 'find-rev', 'r' + git_svn_id]).strip()
  assert out
  return out


def CommitMessageToCleanDict(commit_message):
  """Extract and clean commit message fields that follow the NaCl commit
  message convention. Don't repeat them as-is, to avoid confusing our
  infrastructure."""
  res = {}
  fields = [
      ['git svn id',    ('\s*git-svn-id: '
                        'svn://[^@]+@([0-9]+) [a-f0-9\-]+'), '<none>'],
      ['reviewers',     '\s*R=([^\n]+)',                   ''],
      ['reviewers tbr', '\s*TBR=([^\n]+)',                 ''],
      ['review url',    '\s*Review URL: *([^\n]+)',        '<none>'],
      ['bug',           '\s*BUG=([^\n]+)',                 '<none>'],
      ['test',          '\s*TEST=([^\n]+)',                '<none>'],
  ]
  for key, regex, none in fields:
    found = re.search(regex, commit_message)
    if found:
      commit_message = commit_message.replace(found.group(0), '')
      res[key] = found.group(1).strip()
    else:
      res[key] = none
  res['body'] = commit_message.strip()
  return res


def SendEmail(user_email, out):
  if user_email:
    sys.stderr.write('\nSending email to %s.\n' % user_email)
    msg = email.mime.text.MIMEText(out)
    msg['Subject'] = '[TOOL_REVISIONS updater] failure!'
    msg['From'] = 'tool_revisions-bot@chromium.org'
    msg['To'] = user_email
    s = smtplib.SMTP('localhost')
    s.sendmail(msg['From'], [msg['To']], msg.as_string())
    s.quit()
  else:
    sys.stderr.write('\nNo email address specified.')


def Done(out):
  sys.stdout.write(out)
  sys.exit(0)


class CLInfo:
  """Changelist information: sorted dictionary of NaCl-standard fields."""
  def __init__(self, desc):
    self._desc = desc
    self._vals = collections.OrderedDict([
        ('git svn id', None),
        ('hash', None),
        ('author', None),
        ('date', None),
        ('subject', None),
        ('commits since', None),
        ('bug', None),
        ('test', None),
        ('review url', None),
        ('reviewers', None),
        ('reviewers tbr', None),
        ('body', None),
    ])
  def __getitem__(self, key):
    return self._vals[key]
  def __setitem__(self, key, val):
    assert key in self._vals.keys()
    self._vals[key] = str(val)
  def __str__(self):
    if len([v for v in self._vals.values() if v]) == 0:
      return ''
    return '\n'.join([self._desc + ':'] +
                     [('  ' + k + ':' + (' ' * (17 - len(k))) + v)
                      for (k,v) in self._vals.items() if v]) + '\n\n'

def FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers, err=[], msg=[]):
  assert isinstance(err, list)
  assert isinstance(msg, list)
  return (('*** ERROR ***\n' if err else '') +
          '\n\n'.join(err) +
          '\n\n'.join(msg) +
          ('\n\n' if err or msg else '') +
          ('Update TOOL_REVISIONS for PNaCl %s->%s\n\n' %
           (tr_points_at['git svn id'], oldest_pnacl['git svn id'])) +
          str(oldest_pnacl) + str(tr_points_at) + str(tr_recent) +
          ('\n\nR= %s\n' % reviewers if reviewers else ''))


def Main():
  args = ParseArgs(sys.argv[1:])

  # Updating TOOL_REVISIONS involves looking at the 3 following changelists.
  tr_recent = CLInfo('Most recent TOOL_REVISIONS update')
  tr_points_at = CLInfo('TOOL_REVISIONS update points at PNaCl version')
  oldest_pnacl = CLInfo(
      'Command-line specified PNaCl change' if args.overwrite_pnacl_svn_id
      else 'Oldest PNaCl change since this PNaCl version')
  reviewers = None
  msg = []

  try:
    branch = GitCurrentBranch()
    assert branch == 'master', ('Must be on branch master, currently on %s' %
                                branch)
    status = GitStatus()
    assert len(status) == 0, ("Repository isn't clean:\n  %s" %
                              '\n  '.join(status))
    SyncSources()

    # Information on the latest TOOL_REVISIONS modification.
    for i in ['author', 'date', 'hash', 'subject']:
      tr_recent[i] = GitCommitInfo(info=i, obj='TOOL_REVISIONS', num=1)
    recent_commits = GitCommitsSince(tr_recent['date'])
    tr_recent['commits since'] = len(recent_commits)
    assert len(recent_commits) > 1
    if len(recent_commits) - 1 < args.commits_since:
      # This check is concervative: the TOOL_REVISIONS update might not
      # have changed PNACL_VERSION. We might wait longer than we
      # actually need to.
      # TODO(jfb) Look at the previous commit, and if args.commit_since is
      #           respected (none of the previous commits actually change
      #           PNACL_VERSION) then do the update.
      Done(FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers,
                  msg=['Nothing to do: not enough commits since last '
                       'TOOL_REVISIONS update, need at least %s commits to '
                       'ensure that we get enough performance runs.' %
                       args.commits_since]))

    # The current TOOL_REVISIONS points at a specific PNaCl LLVM
    # version.  LLVM is checked-in to the NaCl repository, but its head
    # isn't necessarily the one that we currently use in PNaCl.
    current_versions = download_toolchains.LoadVersions('TOOL_REVISIONS')
    tr_points_at['git svn id'] = current_versions['PNACL_VERSION']
    tr_points_at['hash'] = FindCommitWithGitSvnId(tr_points_at['git svn id'])
    tr_points_at['date'] = GitCommitInfo(
        info='date', obj=tr_points_at['hash'], num=1)
    tr_points_at['subject'] = GitCommitInfo(
        info='subject', obj=tr_points_at['hash'], num=1)
    recent_commits = GitCommitsSince(tr_points_at['date'])
    tr_points_at['commits since'] = len(recent_commits)
    assert len(recent_commits) > 1

    if args.overwrite_pnacl_svn_id:
      # The user doesn't want to update TOOL_REVISIONS to point at the
      # oldest PNaCl change that follows the previous TOOL_REVISIONS
      # pointer.  This could happens when it makes sense to skip
      # innocuous changes, or when moving the pointer backwards which
      # only a user is qualified to decide to do.
      oldest_pnacl['hash'] = FindCommitWithGitSvnId(
          str(args.overwrite_pnacl_svn_id))
      if not oldest_pnacl['hash']:
        Done(FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers,
                    err=[('Could not find git svn id %s provided at '
                          'the command line.') %
                         (args.overwrite_pnacl_svn_id)]))
    else:
      # Find the oldest PNaCl change that follows the previous
      # TOOL_REVISIONS PNaCl pointer. The default only updates to the
      # next such change to isolate each LLVM commit: it simplifies bug
      # and regression finding.
      oldest_pnacl['hash'] = GitOldestCommitChangingPath(
          recent_commits[:-1], 'pnacl/')
      if not oldest_pnacl['hash']:
        Done(FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers,
                    msg=['No PNaCl change since.']))

    for i in ['author', 'date', 'subject']:
      oldest_pnacl[i] = GitCommitInfo(info=i, obj=oldest_pnacl['hash'], num=1)
    for k,v in CommitMessageToCleanDict(
        GitCommitInfo(info='body',
                        obj=oldest_pnacl['hash'], num=1)).iteritems():
      oldest_pnacl[k] = v
    oldest_pnacl['body'] = (
        '\n  ' + '='*76 + '\n  ' +
        oldest_pnacl['body'].replace('\n', '\n  ') +
        '\n  ' + '='*76)
    reviewers = ', '.join(set(
        [r.strip() for r in ([oldest_pnacl['author']] +
                             oldest_pnacl['reviewers'].split(',') +
                             oldest_pnacl['reviewers tbr'].split(',')
                             ) if r.strip()]))

    new_branch_name = ('TOOL_REVISIONS-pnacl-update-to-%s' %
                       oldest_pnacl['git svn id'])
    if GitBranchExists(new_branch_name):
      # TODO(jfb) Figure out if git-try succeeded, checkout the branch
      #           and dcommit.
      raise Exception("Branch %s already exists, the change hasn't "
                      "landed yet.\nPlease check trybots and dcommit it "
                      "manually." % new_branch_name)
    GitCheckoutNewBranch(new_branch_name)

    # Change the value of PNACL_VERSION in the TOOL_REVISIONS file to
    # the desired git svn id. The hashes are updated below.
    current_versions['PNACL_VERSION'] = oldest_pnacl['git svn id']
    updates = [['PNACL_VERSION',
                tr_points_at['git svn id'],
                current_versions['PNACL_VERSION']]]
    try:
      # Update the hashes.
      # This will download toolchain tarballs that were pre-built by the
      # toolchain waterfall. This may fail if that toolchain revision
      # was never built. Such a failure usually occurs because multiple
      # toolchain builds got kicked-off close to each other and were
      # merged by the waterfall.
      updated_deps = download_toolchains.GetUpdatedDEPS(args, current_versions)
      for flavor, new_value in sorted(updated_deps.iteritems()):
        keyname = download_toolchains.HashKey(flavor)
        old_value = current_versions[keyname]
        updates.append([keyname, old_value, new_value])
    except urllib2.HTTPError as e:
      GitCheckout('master', force=True)
      GitDeleteBranch(new_branch_name, force=True)
      # TODO(jfb) Automatically split the commits.
      #           Before doing this, we should try the next versions and see
      #           if they have a toolchain and don't touch PNaCl.
      raise Exception(
          'Failed fetching %s with code %s (%s), '
          'please split up manually:\n'
          '  Go to the internal version of '
          'http://build.chromium.org/p/client.nacl.toolchain/console'
          '\n'
          '  On the top PNaCl row, click the missing bots '
          '(merged toolchain build).\n'
          '  Force build with your name, reason, no branch, '
          'and revision %s.' %
          (e.url, e.code, e.reason, oldest_pnacl['git svn id']))

    # Change the TOOL_REVISIONS file in the local git repository.
    with open('TOOL_REVISIONS', 'r+') as tool_revisions:
      content = tool_revisions.read()
      for keyname, old_value, new_value in updates:
        old = '%s=%s' % (keyname, old_value)
        new = '%s=%s' % (keyname, new_value)
        assert content.find(old) != -1, ("TOOL_REVISIONS doesn't contain %s" %
                                         old)
        content = content.replace(old, new)
      tool_revisions.seek(0)
      tool_revisions.truncate()
      tool_revisions.write(content)
    GitAdd('TOOL_REVISIONS')
    GitCommit(FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers))

    upload_res = UploadChanges()
    msg += ['Upload result:\n%s' % upload_res]
    try_res = GitTry()
    msg += ['Try result:\n%s' % try_res]

    GitCheckout('master', force=False)

    Done(FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers, msg=msg))

  except SystemExit as e:
    # Normal exit.
    raise

  except (BaseException, Exception) as e:
    # Leave the branch around, if any was created: it'll prevent next
    # runs of the cronjob from succeeding until the failure is fixed.
    out = FmtOut(tr_recent, tr_points_at, oldest_pnacl, reviewers, msg=msg,
                 err=['Failed at %s: %s' % (datetime.datetime.now(), e)])
    sys.stderr.write(out)
    SendEmail(args.email, out)
    GitCheckout('master', force=True)
    raise


if __name__ == '__main__':
  Main()
