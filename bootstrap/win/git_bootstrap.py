# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
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


def _check_call(argv, **kwargs):
  """Wrapper for subprocess.check_call that adds logging."""
  logging.info('running %r', argv)
  subprocess.check_call(argv, **kwargs)


def get_os_bitness():
  """Returns bitness of operating system as int."""
  return 64 if platform.machine().endswith('64') else 32


def get_target_git_version():
  """Returns git version that should be installed."""
  if os.path.exists(os.path.join(ROOT_DIR, '.git_bleeding_edge')):
    git_version_file = 'git_version_bleeding_edge.txt'
  else:
    git_version_file = 'git_version.txt'
  with open(os.path.join(THIS_DIR, git_version_file)) as f:
    return f.read().strip()


def clean_up_old_git_installations(git_directory):
  """Removes git installations other than |git_directory|."""
  for entry in fnmatch.filter(os.listdir(ROOT_DIR), 'git-*_bin'):
    full_entry = os.path.join(ROOT_DIR, entry)
    if full_entry != git_directory:
      logging.info('Cleaning up old git installation %r', entry)
      shutil.rmtree(full_entry, ignore_errors=True)


def cipd_install(args, dest_directory, package, version):
  """Installs CIPD |package| at |version| into |dest_directory|."""
  manifest_file = tempfile.mktemp()
  try:
    with open(manifest_file, 'w') as f:
      f.write('%s %s\n' % (package, version))

    cipd_args = [
      args.cipd_client,
      'ensure',
      '-list', manifest_file,
      '-root', dest_directory,
    ]
    if args.cipd_cache_directory:
      cipd_args.extend(['-cache-dir', args.cipd_cache_directory])
    if args.verbose:
      cipd_args.append('-verbose')
    _check_call(cipd_args)
  finally:
    os.remove(manifest_file)


def need_to_install_git(args, git_directory):
  """Returns True if git needs to be installed."""
  if args.force:
    return True
  git_exe_path = os.path.join(git_directory, 'bin', 'git.exe')
  if not os.path.exists(git_exe_path):
    return True
  if subprocess.call(
      [git_exe_path, '--version'],
      stdout=DEVNULL, stderr=DEVNULL) != 0:
    return True
  for script in ('git.bat', 'gitk.bat', 'ssh.bat', 'ssh-keygen.bat',
                 'git-bash'):
    full_path = os.path.join(ROOT_DIR, script)
    if not os.path.exists(full_path):
      return True
    with open(full_path) as f:
      if os.path.relpath(git_directory, ROOT_DIR) not in f.read():
        return True
  if not os.path.exists(os.path.join(
      git_directory, 'etc', 'profile.d', 'python.sh')):
    return True
  return False


def install_git(args, git_version, git_directory):
  """Installs |git_version| into |git_directory|."""
  cipd_platform = 'windows-%s' % ('amd64' if args.bits == 64 else '386')
  temp_dir = tempfile.mkdtemp()
  try:
    cipd_install(args,
                 temp_dir,
                 'infra/depot_tools/git_installer/%s' % cipd_platform,
                 'v' + git_version.replace('.', '_'))

    if not os.path.exists(git_directory):
      os.makedirs(git_directory)

    # 7-zip has weird expectations for command-line syntax. Pass it as a string
    # to avoid subprocess module quoting breaking it. Also double-escape
    # backslashes in paths.
    _check_call(' '.join([
      os.path.join(temp_dir, 'git-installer.exe'),
      '-y',
      '-InstallPath="%s"' % git_directory.replace('\\', '\\\\'),
      '-Directory="%s"' % git_directory.replace('\\', '\\\\'),
    ]))
  finally:
    shutil.rmtree(temp_dir, ignore_errors=True)

  with open(os.path.join(THIS_DIR, 'git.template.bat')) as f:
    git_template = f.read()
  git_template = git_template.replace(
      'GIT_BIN_DIR', os.path.relpath(git_directory, ROOT_DIR))
  scripts = (
      ('git.bat', 'cmd\\git.exe'),
      ('gitk.bat', 'cmd\\gitk.exe'),
      ('ssh.bat', 'usr\\bin\\ssh.exe'),
      ('ssh-keygen.bat', 'usr\\bin\\ssh-keygen.exe'),
  )
  for script in scripts:
    with open(os.path.join(ROOT_DIR, script[0]), 'w') as f:
      f.write(git_template.replace('GIT_PROGRAM', script[1]))
  with open(os.path.join(THIS_DIR, 'git-bash.template.sh')) as f:
    git_bash_template = f.read()
  with open(os.path.join(ROOT_DIR, 'git-bash'), 'w') as f:
    f.write(git_bash_template.replace(
        'GIT_BIN_DIR', os.path.relpath(git_directory, ROOT_DIR)))
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
  parser.add_argument('--force', action='store_true',
                      help='Always re-install git.')
  parser.add_argument('--verbose', action='store_true')
  args = parser.parse_args(argv)

  if os.environ.get('WIN_TOOLS_FORCE') == '1':
    args.force = True

  logging.basicConfig(level=logging.INFO if args.verbose else logging.WARN)

  git_version = get_target_git_version()
  git_directory = os.path.join(
      ROOT_DIR, 'git-%s-%s_bin' % (git_version, args.bits))
  git_docs_dir = os.path.join(
      git_directory, 'mingw%s' % args.bits, 'share', 'doc', 'git-doc')

  clean_up_old_git_installations(git_directory)

  if need_to_install_git(args, git_directory):
    install_git(args, git_version, git_directory)

  # Update depot_tools files for "git help <command>"
  docsrc = os.path.join(ROOT_DIR, 'man', 'html')
  for name in os.listdir(docsrc):
    shutil.copy2(os.path.join(docsrc, name), os.path.join(git_docs_dir, name))

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
