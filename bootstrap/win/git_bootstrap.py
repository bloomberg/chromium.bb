# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import fnmatch
import logging
import os
import platform
import shutil
import subprocess
import sys
import tempfile


THIS_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..'))

DEVNULL = open(os.devnull, 'w')

BAT_EXT = '.bat' if sys.platform.startswith('win') else ''


# Top-level stubs to generate that fall through to executables within the Git
# directory.
STUBS = {
  'git.bat': 'cmd\\git.exe',
  'gitk.bat': 'cmd\\gitk.exe',
  'ssh.bat': 'usr\\bin\\ssh.exe',
  'ssh-keygen.bat': 'usr\\bin\\ssh-keygen.exe',
}


def _check_call(argv, input=None, **kwargs):
  """Wrapper for subprocess.check_call that adds logging."""
  logging.info('running %r', argv)
  if input is not None:
    kwargs['stdin'] = subprocess.PIPE
  proc = subprocess.Popen(argv, **kwargs)
  proc.communicate(input=input)
  if proc.returncode:
    raise subprocess.CalledProcessError(proc.returncode, argv, None)


def _safe_rmtree(path):
  if not os.path.exists(path):
    return

  def _make_writable_and_remove(path):
    st = os.stat(path)
    new_mode = st.st_mode | 0200
    if st.st_mode == new_mode:
      return False
    try:
      os.chmod(path, new_mode)
      os.remove(path)
      return True
    except Exception:
      return False

  def _on_error(function, path, excinfo):
    if not _make_writable_and_remove(path):
      logging.warning('Failed to %s: %s (%s)', function, path, excinfo)

  shutil.rmtree(path, onerror=_on_error)


@contextlib.contextmanager
def _tempdir():
  tdir = None
  try:
    tdir = tempfile.mkdtemp()
    yield tdir
  finally:
    _safe_rmtree(tdir)


def get_os_bitness():
  """Returns bitness of operating system as int."""
  return 64 if platform.machine().endswith('64') else 32


def get_target_git_version(args):
  """Returns git version that should be installed."""
  if (args.bleeding_edge or
      os.path.exists(os.path.join(ROOT_DIR, '.git_bleeding_edge'))):
    git_version_file = 'git_version_bleeding_edge.txt'
  else:
    git_version_file = 'git_version.txt'
  with open(os.path.join(THIS_DIR, git_version_file)) as f:
    return f.read().strip()


def clean_up_old_git_installations(git_directory, force):
  """Removes git installations other than |git_directory|."""
  for entry in fnmatch.filter(os.listdir(ROOT_DIR), 'git-*_bin'):
    full_entry = os.path.join(ROOT_DIR, entry)
    if force or full_entry != git_directory:
      logging.info('Cleaning up old git installation %r', entry)
      _safe_rmtree(full_entry)


def cipd_install(args, dest_directory, package, version):
  """Installs CIPD |package| at |version| into |dest_directory|."""
  logging.info('Installing CIPD package %r @ %r', package, version)
  manifest = '%s %s\n' % (package, version)
  cipd_args = [
    args.cipd_client,
    'ensure',
    '-ensure-file', '-',
    '-root', dest_directory,
  ]
  if args.cipd_cache_directory:
    cipd_args.extend(['-cache-dir', args.cipd_cache_directory])
  if args.verbose:
    cipd_args.append('-verbose')
  _check_call(cipd_args, input=manifest)


def need_to_install_git(args, git_directory, legacy):
  """Returns True if git needs to be installed."""
  if args.force:
    return True

  is_cipd_managed = os.path.exists(os.path.join(git_directory, '.cipd'))
  if legacy:
    if is_cipd_managed:
      # Converting from non-legacy to legacy, need reinstall.
      return True
    if not os.path.exists(os.path.join(
        git_directory, 'etc', 'profile.d', 'python.sh')):
      return True
  elif not is_cipd_managed:
    # Converting from legacy to CIPD, need reinstall.
    return True

  git_exe_path = os.path.join(git_directory, 'bin', 'git.exe')
  if not os.path.exists(git_exe_path):
    return True
  if subprocess.call(
      [git_exe_path, '--version'],
      stdout=DEVNULL, stderr=DEVNULL) != 0:
    return True

  gen_stubs = STUBS.keys()
  gen_stubs.append('git-bash')
  for stub in gen_stubs:
    full_path = os.path.join(ROOT_DIR, stub)
    if not os.path.exists(full_path):
      return True
    with open(full_path) as f:
      if os.path.relpath(git_directory, ROOT_DIR) not in f.read():
        return True

  return False


def install_git_legacy(args, git_version, git_directory, cipd_platform):
  _safe_rmtree(git_directory)
  with _tempdir() as temp_dir:
    cipd_install(args,
                 temp_dir,
                 'infra/depot_tools/git_installer/%s' % cipd_platform,
                 'v' + git_version.replace('.', '_'))

    # 7-zip has weird expectations for command-line syntax. Pass it as a string
    # to avoid subprocess module quoting breaking it. Also double-escape
    # backslashes in paths.
    _check_call(' '.join([
      os.path.join(temp_dir, 'git-installer.exe'),
      '-y',
      '-InstallPath="%s"' % git_directory.replace('\\', '\\\\'),
      '-Directory="%s"' % git_directory.replace('\\', '\\\\'),
    ]))


def install_git(args, git_version, git_directory, legacy):
  """Installs |git_version| into |git_directory|."""
  # TODO: Remove legacy version once everyone is on bundled Git.
  cipd_platform = 'windows-%s' % ('amd64' if args.bits == 64 else '386')
  if legacy:
    install_git_legacy(args, git_version, git_directory, cipd_platform)
  else:
    # When migrating from legacy, we want to nuke this directory. In other
    # cases, CIPD will handle the cleanup.
    if not os.path.isdir(os.path.join(git_directory, '.cipd')):
      logging.info('Deleting legacy Git directory: %s', git_directory)
      _safe_rmtree(git_directory)

    cipd_install(
        args,
        git_directory,
        'infra/git/%s' % (cipd_platform,),
        git_version)

  # Create Git templates and configure its base layout.
  with open(os.path.join(THIS_DIR, 'git.template.bat')) as f:
    git_template = f.read()
  git_template = git_template.replace(
      'GIT_BIN_DIR', os.path.relpath(git_directory, ROOT_DIR))

  for stub_name, relpath in STUBS.iteritems():
    with open(os.path.join(ROOT_DIR, stub_name), 'w') as f:
      f.write(git_template.replace('GIT_PROGRAM', relpath))
  with open(os.path.join(THIS_DIR, 'git-bash.template.sh')) as f:
    git_bash_template = f.read()
  with open(os.path.join(ROOT_DIR, 'git-bash'), 'w') as f:
    f.write(git_bash_template.replace(
        'GIT_BIN_DIR', os.path.relpath(git_directory, ROOT_DIR)))

  if legacy:
    # The non-legacy Git bundle includes "python.sh".
    #
    # TODO: Delete "profile.d.python.sh" after legacy mode is removed.
    shutil.copyfile(
        os.path.join(THIS_DIR, 'profile.d.python.sh'),
        os.path.join(git_directory, 'etc', 'profile.d', 'python.sh'))

  git_bat_path = os.path.join(ROOT_DIR, 'git.bat')
  _check_call([git_bat_path, 'config', '--system', 'core.autocrlf', 'false'])
  _check_call([git_bat_path, 'config', '--system', 'core.filemode', 'false'])
  _check_call([git_bat_path, 'config', '--system', 'core.preloadindex', 'true'])
  _check_call([git_bat_path, 'config', '--system', 'core.fscache', 'true'])


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--bits', type=int, choices=(32,64),
                      help='Bitness of the client to install. Default on this'
                      ' system: %(default)s', default=get_os_bitness())
  parser.add_argument('--cipd-client',
                      help='Path to CIPD client binary. default: %(default)s',
                      default=os.path.join(ROOT_DIR, 'cipd'+BAT_EXT))
  parser.add_argument('--cipd-cache-directory',
                      help='Path to CIPD cache directory.')
  parser.add_argument('--bleeding-edge', action='store_true',
                      help='Force bleeding edge Git.')
  parser.add_argument('--force', action='store_true',
                      help='Always re-install git.')
  parser.add_argument('--verbose', action='store_true')
  args = parser.parse_args(argv)

  if os.environ.get('WIN_TOOLS_FORCE') == '1':
    args.force = True

  logging.basicConfig(level=logging.INFO if args.verbose else logging.WARN)

  git_version = get_target_git_version(args)

  git_directory_tag = git_version.split(':')
  git_directory = os.path.join(
      ROOT_DIR, 'git-%s-%s_bin' % (git_directory_tag[-1], args.bits))
  git_docs_dir = os.path.join(
      git_directory, 'mingw%s' % args.bits, 'share', 'doc', 'git-doc')

  clean_up_old_git_installations(git_directory, args.force)

  # Modern Git versions use CIPD tags beginning with "version:". If the tag
  # does not begin with that, use the legacy installer.
  legacy = not git_version.startswith('version:')
  if need_to_install_git(args, git_directory, legacy):
    install_git(args, git_version, git_directory, legacy)

  # Update depot_tools files for "git help <command>"
  docsrc = os.path.join(ROOT_DIR, 'man', 'html')
  for name in os.listdir(docsrc):
    shutil.copy2(os.path.join(docsrc, name), os.path.join(git_docs_dir, name))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
