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

BUILD_DIR = os.path.dirname(__file__)
NACL_DIR = os.path.dirname(BUILD_DIR)
TOOLCHAIN_REV_DIR = os.path.join(NACL_DIR, 'toolchain_revisions')
PKG_VER = os.path.join(BUILD_DIR, 'package_version', 'package_version.py')

PKGS = ['pnacl_newlib', 'pnacl_translator']
REV_FILES = [os.path.join(TOOLCHAIN_REV_DIR, '%s.json' % package)
             for package in PKGS]


def ParseArgs(args):
  parser = argparse.ArgumentParser(
      formatter_class=argparse.RawDescriptionHelpFormatter,
      description="""Update pnacl_newlib.json PNaCl version.

LLVM and other projects are checked-in to the NaCl repository, but their
head isn't necessarily the one that we currently use in PNaCl. The
pnacl_newlib.json and pnacl_translator.json files point at subversion
revisions to use for tools such as LLVM. Our build process then
downloads pre-built tool tarballs from the toolchain build waterfall.

git repository before running this script:
         ______________________
         |                    |
         v                    |
  ...----A------B------C------D------ NaCl HEAD
         ^      ^      ^      ^
         |      |      |      |__ Latest pnacl_{newlib,translator}.json update.
         |      |      |
         |      |      |__ A newer LLVM change (LLVM repository HEAD).
         |      |
         |      |__ Oldest LLVM change since this PNaCl version.
         |
         |__ pnacl_{newlib,translator}.json points at an older LLVM change.

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
  parser.add_argument('--svn-id', metavar='SVN_ID', type=int, default=0,
                      help="Update to a specific SVN ID instead of the most "
                      "recent SVN ID with a PNaCl change. This value must "
                      "be more recent than the one in the current "
                      "pnacl_newlib.json. This option is useful when multiple "
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
  return parser.parse_args()


def ExecCommand(command):
  try:
    return subprocess.check_output(command, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    sys.stderr.write('\nRunning `%s` returned %i, got:\n%s\n' %
                     (' '.join(e.cmd), e.returncode, e.output))
    raise


def GetCurrentRevision():
  return [ExecCommand([sys.executable, PKG_VER,
                       'getrevision',
                       '--revision-package', package]).strip()
          for package in PKGS]


def SetCurrentRevision(revision_num):
  for package in PKGS:
    ExecCommand([sys.executable, PKG_VER,
                 'setrevision',
                 '--revision-package', package,
                 '--revision', str(revision_num)])


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
  return ExecCommand(['git', 'cl', 'try'])


def FindCommitWithGitSvnId(git_svn_id):
  while True:
    # This command needs to retry because git-svn partially rebuild its
    # revision map for every commit. Asking it a second time fixes the
    # issue.
    out = ExecCommand(['git', 'svn', 'find-rev', 'r' + git_svn_id]).strip()
    if not re.match('^Partial-rebuilding ', out):
      break
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
    msg['Subject'] = '[PNaCl revision updater] failure!'
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


def FmtOut(tr_points_at, pnacl_changes, new_svn_id, err=[], msg=[]):
  assert isinstance(err, list)
  assert isinstance(msg, list)
  old_svn_id = tr_points_at['git svn id']
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
          ('Update revision for PNaCl r%s->r%s\n\n'
           'Pull the following PNaCl changes into NaCl:\n%s\n\n'
           '%s\n'
           'R= %s\n'
           'TEST=git try\n'
           'NOTRY=true\n'
           '(Please LGTM this change and tick the "commit" box)\n' %
           (old_svn_id, new_svn_id, changes, bugs, reviewers)))


def Main():
  args = ParseArgs(sys.argv[1:])

  tr_points_at = CLInfo('revision update points at PNaCl version')
  pnacl_changes = []
  msg = []

  branch = GitCurrentBranch()
  assert branch == 'master', ('Must be on branch master, currently on %s' %
                              branch)

  try:
    status = GitStatus()
    assert len(status) == 0, ("Repository isn't clean:\n  %s" %
                              '\n  '.join(status))
    SyncSources()

    # The current revision file points at a specific PNaCl LLVM
    # version. LLVM is checked-in to the NaCl repository, but its head
    # isn't necessarily the one that we currently use in PNaCl.
    (pnacl_revision, translator_revision) = GetCurrentRevision()
    tr_points_at['git svn id'] = pnacl_revision
    tr_points_at['hash'] = FindCommitWithGitSvnId(tr_points_at['git svn id'])
    tr_points_at['date'] = GitCommitInfo(
        info='date', obj=tr_points_at['hash'], num=1)
    tr_points_at['subject'] = GitCommitInfo(
        info='subject', obj=tr_points_at['hash'], num=1)
    recent_commits = GitCommitsSince(tr_points_at['date'])
    tr_points_at['commits since'] = len(recent_commits)
    assert len(recent_commits) > 1

    if args.svn_id and args.svn_id <= int(tr_points_at['git svn id']):
      Done(FmtOut(tr_points_at, pnacl_changes, args.svn_id,
                  err=["Can't update to SVN ID r%s, the current "
                       "PNaCl revision's SVN ID (r%s) is more recent." %
                       (args.svn_id, tr_points_at['git svn id'])]))

    # Find the commits changing PNaCl files that follow the previous
    # PNaCl revision pointer.
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
      Done(FmtOut(tr_points_at, pnacl_changes, pnacl_revision,
                  msg=['No PNaCl change since r%s.' %
                       tr_points_at['git svn id']]))

    new_pnacl_revision = args.svn_id or pnacl_changes[-1]['git svn id']

    new_branch_name = ('pnacl-revision-update-to-%s' %
                       new_pnacl_revision)
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

    if args.dry_run:
      DryRun("Would update PNaCl revision to: %s" % new_pnacl_revision)
    else:
      SetCurrentRevision(new_pnacl_revision)
      for f in REV_FILES:
        GitAdd(f)
      GitCommit(FmtOut(tr_points_at, pnacl_changes, new_pnacl_revision))

      upload_res = UploadChanges()
      msg += ['Upload result:\n%s' % upload_res]
      try_res = GitTry()
      msg += ['Try result:\n%s' % try_res]

      GitCheckout('master', force=False)

    Done(FmtOut(tr_points_at, pnacl_changes, new_pnacl_revision, msg=msg))

  except SystemExit as e:
    # Normal exit.
    raise

  except (BaseException, Exception) as e:
    # Leave the branch around, if any was created: it'll prevent next
    # runs of the cronjob from succeeding until the failure is fixed.
    out = FmtOut(tr_points_at, pnacl_changes, new_pnacl_revision, msg=msg,
                 err=['Failed at %s: %s' % (datetime.datetime.now(), e)])
    sys.stderr.write(out)
    if not args.dry_run:
      SendEmail(args.email, out)
      GitCheckout('master', force=True)
    raise


if __name__ == '__main__':
  Main()
