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

LLVM and other projects are checked-in to the NaCl repository, but their
head isn't necessarily the one that we currently use in PNaCl. The
TOOL_REVISIONS file points at subversion revisions to use for tools such
as LLVM. Our build process then downloads pre-built tool tarballs from
the toolchain build waterfall.

git repository before running this script:
         ______________________
         |                    |
         v                    |
  ...----A------B------C------D------ NaCl HEAD
         ^      ^      ^      ^
         |      |      |      |__ Latest TOOL_REVISIONS update.
         |      |      |
         |      |      |__ A newer LLVM change (LLVM repository HEAD).
         |      |
         |      |__ Oldest LLVM change since this PNaCl version.
         |
         |__ TOOL_REVISIONS points at an older LLVM change.

git repository after running this script:
                       _______________
                       |             |
                       v             |
  ...----A------B------C------D------E------ NaCl HEAD

Note that there could be any number of non-PNaCl changes between each of
these changelists, and that the user can also decide to update the
pointer to B instead of C.

There is further complication when toolchain builds are merged.
""")
  parser.add_argument('--email', metavar='ADDRESS', type=str,
                      default=getpass.getuser()+'@chromium.org',
                      help="Email address to send errors to.")
  parser.add_argument('--commits-since', metavar='N', type=int, default=0,
                      help="Commits to wait between TOOL_REVISIONS updates.")
  parser.add_argument('--svn-id', metavar='SVN_ID', type=int, default=0,
                      help="Update to a specific SVN ID instead of the most "
                      "recent SVN ID with a PNaCl change. This value must "
                      "be more recent than the one in the current "
                      "TOOL_REVISIONS. This option is useful when multiple "
                      "changelists' toolchain builds were merged, or when "
                      "too many PNaCl changes would be pulled in at the "
                      "same time.")
  parser.add_argument('--dry-run', default=False, action='store_true',
                      help="Print the changelist that would be sent, but "
                      "don't actually send anything to review.")
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


def GitChangesPath(commit_hash, path):
  """Returns True if the commit changes a file under the given path."""
  return any([
      re.search('^' + path, f.strip()) for f in
      GitFilesChanged(commit_hash)])


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
      ['reviewers tbr', '\s*TBR=([^\n]+)',                 ''],
      ['reviewers',     '\s*R=([^\n]+)',                   ''],
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


def DryRun(out):
  sys.stdout.write("DRY RUN: " + out + "\n")


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
        ('reviewers tbr', None),
        ('reviewers', None),
        ('body', None),
    ])
  def __getitem__(self, key):
    return self._vals[key]
  def __setitem__(self, key, val):
    assert key in self._vals.keys()
    self._vals[key] = str(val)
  def __str__(self):
    """Changelist to string.

    A short description of the change, e.g.:
      r12345: (tom@example.com) Subject of the change.

    If the change is itself pulling in other changes from
    sub-repositories then take its relevant description and append it to
    the string. These sub-directory updates are also script-generated
    and therefore have a predictable format. e.g.:
      r12345: (tom@example.com) Subject of the change.
        | dead123: (dick@example.com) Other change in another repository.
        | beef456: (harry@example.com) Yet another cross-repository change.
    """
    desc = ('  r' + self._vals['git svn id'] + ': (' +
            self._vals['author'] + ') ' +
            self._vals['subject'])
    if GitChangesPath(self._vals['hash'], 'pnacl/COMPONENT_REVISIONS'):
      git_hash_abbrev = '[0-9a-fA-F]{7}'
      email = '[^@)]+@[^)]+\.[^)]+'
      desc = '\n'.join([desc] + [
          '    | ' + line for line in self._vals['body'].split('\n') if
          re.match('^ *%s: \(%s\) .*$' % (git_hash_abbrev, email), line)])
    return desc


def FmtOut(tr_recent, tr_points_at, pnacl_changes, err=[], msg=[]):
  assert isinstance(err, list)
  assert isinstance(msg, list)
  old_svn_id = tr_points_at['git svn id']
  new_svn_id = pnacl_changes[-1]['git svn id'] if pnacl_changes else '?'
  changes = '\n'.join([str(cl) for cl in pnacl_changes])
  bugs = '\n'.join(list(set(
      ['BUG= ' + cl['bug'].strip() if cl['bug'] else '' for
       cl in pnacl_changes]) - set([''])))
  reviewers = ', '.join(list(set(
      [r.strip() for r in
       (','.join([
           cl['author'] + ',' + cl['reviewers tbr'] + ',' + cl['reviewers']
           for cl in pnacl_changes])).split(',')]) - set([''])))
  return (('*** ERROR ***\n' if err else '') +
          '\n\n'.join(err) +
          '\n\n'.join(msg) +
          ('\n\n' if err or msg else '') +
          ('Update TOOL_REVISIONS for PNaCl r%s->r%s\n\n'
           'Pull the following PNaCl changes into NaCl:\n%s\n\n'
           '%s\n'
           'R= %s\n'
           'TEST=git try\n'
           'NOTRY=true\n'
           '(Please LGTM this change and tick the "commit" box)\n' %
           (old_svn_id, new_svn_id, changes, bugs, reviewers)))


def Main():
  args = ParseArgs(sys.argv[1:])

  # Updating TOOL_REVISIONS involves looking at the 3 following changelists.
  tr_recent = CLInfo('Most recent TOOL_REVISIONS update')
  tr_points_at = CLInfo('TOOL_REVISIONS update points at PNaCl version')
  pnacl_changes = []
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
    if len(recent_commits) - 1 < args.commits_since:
      # This check is conservative: the TOOL_REVISIONS update might not
      # have changed PNACL_VERSION. We might wait longer than we
      # actually need to.
      # TODO(jfb) Look at the previous commit, and if args.commit_since is
      #           respected (none of the previous commits actually change
      #           PNACL_VERSION) then do the update.
      Done(FmtOut(tr_recent, tr_points_at, pnacl_changes,
                  msg=['Nothing to do: not enough commits since last '
                       'TOOL_REVISIONS update, need at least %s commits to '
                       'ensure that we get enough performance runs.' %
                       args.commits_since]))

    # The current TOOL_REVISIONS points at a specific PNaCl LLVM
    # version. LLVM is checked-in to the NaCl repository, but its head
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

    if args.svn_id and args.svn_id <= int(tr_points_at['git svn id']):
      Done(FmtOut(tr_recent, tr_points_at, pnacl_changes,
                  err=["Can't update to SVN ID r%s, the current "
                       "TOOL_REVISIONS' SVN ID (r%s) is more recent." %
                       (args.svn_id, tr_points_at['git svn id'])]))

    # Find the commits changing PNaCl files that follow the previous
    # TOOL_REVISIONS PNaCl pointer.
    pnacl_pathes = ['pnacl/', 'toolchain_build/']
    pnacl_hashes = list(set(reduce(
        lambda acc, lst: acc + lst,
        [[cl for cl in recent_commits[:-1] if
          GitChangesPath(cl, path)] for
         path in pnacl_pathes])))
    for hash in pnacl_hashes:
      cl = CLInfo('PNaCl change ' + hash)
      cl['hash'] = hash
      for i in ['author', 'date', 'subject']:
        cl[i] = GitCommitInfo(info=i, obj=hash, num=1)
      for k,v in CommitMessageToCleanDict(
          GitCommitInfo(info='body', obj=hash, num=1)).iteritems():
        cl[k] = v
      pnacl_changes.append(cl)

    # The PNaCl hashes weren't ordered chronologically, make sure the
    # changes are.
    pnacl_changes.sort(key=lambda x: int(x['git svn id']))

    if args.svn_id:
      pnacl_changes = [cl for cl in pnacl_changes if
                       int(cl['git svn id']) <= args.svn_id]

    if len(pnacl_changes) == 0:
      Done(FmtOut(tr_recent, tr_points_at, pnacl_changes,
                  msg=['No PNaCl change since r%s.' %
                       tr_points_at['git svn id']]))

    current_versions['PNACL_VERSION'] = pnacl_changes[-1]['git svn id']

    new_branch_name = ('TOOL_REVISIONS-pnacl-update-to-%s' %
                       current_versions['PNACL_VERSION'])
    if GitBranchExists(new_branch_name):
      # TODO(jfb) Figure out if git-try succeeded, checkout the branch
      #           and dcommit.
      raise Exception("Branch %s already exists, the change hasn't "
                      "landed yet.\nPlease check trybots and dcommit it "
                      "manually." % new_branch_name)
    if args.dry_run:
      DryRun("Would check out branch: " + new_branch_name)
    else:
      GitCheckoutNewBranch(new_branch_name)

    # Change the value of PNACL_VERSION in the TOOL_REVISIONS file to
    # the desired git svn id. The hashes are updated below.
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
      if not args.dry_run:
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
          (e.url, e.code, e.reason, current_versions['PNACL_VERSION']))

    if args.dry_run:
      DryRun("Would update TOOL_REVISIONS to:\n%s" %
             '\n'.join(['  ' + u[0] + '=' + u[2] for u in updates]))
    else:
      # Change the TOOL_REVISIONS file in the local git repository.
      with open('TOOL_REVISIONS', 'r+') as tool_revisions:
        content = tool_revisions.read()
        for keyname, old_value, new_value in updates:
          old = '%s=%s' % (keyname, old_value)
          new = '%s=%s' % (keyname, new_value)
          assert content.find(old) != -1, (
              "TOOL_REVISIONS doesn't contain %s" % old)
          content = content.replace(old, new)
        tool_revisions.seek(0)
        tool_revisions.truncate()
        tool_revisions.write(content)
      GitAdd('TOOL_REVISIONS')
      GitCommit(FmtOut(tr_recent, tr_points_at, pnacl_changes))

      upload_res = UploadChanges()
      msg += ['Upload result:\n%s' % upload_res]
      try_res = GitTry()
      msg += ['Try result:\n%s' % try_res]

      GitCheckout('master', force=False)

    Done(FmtOut(tr_recent, tr_points_at, pnacl_changes, msg=msg))

  except SystemExit as e:
    # Normal exit.
    raise

  except (BaseException, Exception) as e:
    # Leave the branch around, if any was created: it'll prevent next
    # runs of the cronjob from succeeding until the failure is fixed.
    out = FmtOut(tr_recent, tr_points_at, pnacl_changes, msg=msg,
                 err=['Failed at %s: %s' % (datetime.datetime.now(), e)])
    sys.stderr.write(out)
    if not args.dry_run:
      SendEmail(args.email, out)
      GitCheckout('master', force=True)
    raise


if __name__ == '__main__':
  Main()
