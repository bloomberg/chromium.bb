# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Should be run on a GCE instance to set up the build environment."""

from __future__ import print_function

import getpass
import os

from chromite.compute import compute_configs
from chromite.compute import bot_constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils


# Make the script more readable.
RunCommand = cros_build_lib.RunCommand
SudoRunCommand = cros_build_lib.SudoRunCommand


BOT_CREDS_PATH = bot_constants.BOT_CREDS_TMP_PATH
# Most credentials are stored in the home directory.
HOME_DIR = osutils.ExpandPath('~')


def SetupPrerequisites():
  """Installs packages required for Chrome OS build."""
  SudoRunCommand(['apt-get', 'update'])
  SudoRunCommand(['apt-get', '-y', '--force-yes', 'upgrade'])
  # Chrome OS pre-requisite packages.
  packages = ['git', 'curl', 'pbzip2', 'gawk', 'gitk', 'subversion']
  # Required for CIDB.
  packages += ['python-sqlalchemy', 'python-mysqldb']
  # Required for payload generation outside of the chroot.
  packages += ['python-protobuf']

  # Packages to monitor system performance and usage.
  packages += ['sysstat']

  SudoRunCommand(['apt-get', '-y', 'install'] + packages)


def InstallChromeDependencies():
  """Installs packages required to build Chromium."""
  # The install-build-deps.sh relies on some packages that are not in
  # the base image. Install them first before invoking the script.
  SudoRunCommand(['apt-get', '-y', 'install',
                  'gcc-arm-linux-gnueabihf',
                  'g++-4.8-multilib-arm-linux-gnueabihf',
                  'gcc-4.8-multilib-arm-linux-gnueabihf',
                  'realpath'])

  with osutils.TempDir(prefix='tmp-chrome-deps') as tempdir:
    RunCommand(['git', 'clone', bot_constants.CHROMIUM_BUILD_URL], cwd=tempdir)
    RunCommand([os.path.join(tempdir, 'build', 'install-build-deps.sh'),
                '--syms', '--no-prompt'])


def SetMountCount():
  """Sets mount count to a large number."""
  for drive in compute_configs.DRIVES:
    SudoRunCommand(['tune2fs', '-c', '150', os.path.join('dev', drive)],
                   redirect_stdout=True)


def _SetupSVN():
  """Sets up the chromium svn username/password."""
  # Create a ~/.subversion directory.
  RunCommand(['svn', 'ls', 'http://src.chromium.org/svn'], redirect_stdout=True)
  # Change the setting to store the svn password.
  sed_str = ('s/# store-plaintext-passwords = '
             'no/store-plaintext-passwords = yes/g')
  RunCommand(['sed', '-i', '-e', sed_str,
              osutils.ExpandPath(os.path.join('~', '.subversion', 'servers'))])

  password_path = osutils.ExpandPath(
      os.path.join(BOT_CREDS_PATH, bot_constants.SVN_PASSWORD_FILE))
  password = osutils.ReadFile(password_path).strip()
  # `svn ls` each repository to store the password in ~/.subversion.
  for svn_host in bot_constants.CHROMIUM_SVN_HOSTS:
    for svn_repo in bot_constants.CHROMIUM_SVN_REPOS:
      RunCommand(['svn', 'ls', '--username', bot_constants.BUILDBOT_SVN_USER,
                  '--password', password, 'svn://%s/%s' % (svn_host, svn_repo)],
                 redirect_stdout=True)

def _SetupGoB():
  """Sets up GoB credentials."""
  RunCommand(['git', 'config', '--global', 'user.email',
              bot_constants.GIT_USER_EMAIL])
  RunCommand(['git', 'config', '--global', 'user.name',
              bot_constants.GIT_USER_NAME])

  RunCommand(['git', 'clone', bot_constants.GCOMPUTE_TOOLS_URL],
             cwd=HOME_DIR, redirect_stdout=True)

  # Run git-cookie-authdaemon at boot time by adding it to
  # /etc/rc.local
  rc_local_path = os.path.join(os.path.sep, 'etc', 'rc.local')
  daemon_path = os.path.join(HOME_DIR, 'gcompute-tools',
                             'git-cookie-authdaemon')
  daemon_cmd = 'su %s -c %s\n' % (bot_constants.BUILDBOT_USER,
                                  daemon_path)
  content = osutils.ReadFile(rc_local_path).replace('exit 0', '')
  content += daemon_cmd
  content += 'exit 0\n'

  with osutils.TempDir() as tempdir:
    tmp_file = os.path.join(tempdir, 'rc.local')
    osutils.WriteFile(tmp_file, content)
    os.chmod(tmp_file, 755)
    SudoRunCommand(['mv', tmp_file, rc_local_path])


def _SetupCIDB():
  """Copies cidb credentials."""
  RunCommand(
      ['cp', '-r', os.path.join(BOT_CREDS_PATH, bot_constants.CIDB_CREDS_DIR),
       HOME_DIR])


def _SetupTreeStatus():
  """Copies credentials for updating tree status."""
  RunCommand(
      ['cp',
       os.path.join(BOT_CREDS_PATH, bot_constants.TREE_STATUS_PASSWORD_FILE),
       HOME_DIR])


def SetupCredentials():
  """Sets up various credentials."""
  _SetupSVN()
  _SetupGoB()
  _SetupCIDB()
  _SetupTreeStatus()


def SetupBuildbotEnvironment():
  """Sets up the buildbot environment."""

  # Append host entries to /etc/hosts. This includes the buildbot
  # master IP address.
  host_entries = RunCommand(
      ['cat', os.path.join(BOT_CREDS_PATH, bot_constants.HOST_ENTRIES)],
      capture_output=True).output
  SudoRunCommand(['tee', '-a', '/etc/hosts'], input=host_entries)

  # Create the buildbot directory.
  SudoRunCommand(['mkdir', '-p', bot_constants.BUILDBOT_DIR])
  SudoRunCommand(['chown', '-R', '%s:%s' % (bot_constants.BUILDBOT_USER,
                                            bot_constants.BUILDBOT_USER),
                  bot_constants.BUILDBOT_DIR])

  with osutils.TempDir() as tempdir:
    # Download depot tools to a temp directory to bootstrap. `gclient
    # sync` will create depot_tools in BUILDBOT_DIR later.
    tmp_depot_tools_path = os.path.join(tempdir, 'depot_tools')
    RunCommand(['git', 'clone', bot_constants.DEPOT_TOOLS_URL],
               cwd=tempdir, redirect_stdout=True)
    # `gclient` relies on depot_tools in $PATH, pass the extra
    # envinornment variable.
    path_env = '%s:%s' % (os.getenv('PATH'), tmp_depot_tools_path)
    RunCommand(['gclient', 'config', bot_constants.BUILDBOT_SVN_REPO],
               cwd=bot_constants.BUILDBOT_DIR, extra_env={'PATH': path_env})
    RunCommand(['gclient', 'sync', '--jobs', '5'],
               cwd=bot_constants.BUILDBOT_DIR,
               redirect_stdout=True, extra_env={'PATH': path_env})

  # Set up buildbot password.
  config_dir = os.path.join(bot_constants.BUILDBOT_DIR, 'build', 'site_config')
  RunCommand(['cp', os.path.join(BOT_CREDS_PATH,
                                 bot_constants.BUILDBOT_PASSWORD_FILE),
              os.path.join(config_dir,
                           bot_constants.BUILDBOT_PASSWORD_FILE)])

  # Update the environment variable.
  depot_tools_path = os.path.join(bot_constants.BUILDBOT_DIR, 'depot_tools')
  RunCommand(['bash', '-c', r'echo export PATH=\$PATH:%s >> ~/.bashrc'
              % depot_tools_path])

  # Start buildbot slave at startup.
  crontab_content = ''
  result = RunCommand(
      ['crontab', '-l'], capture_output=True, error_code_ok=True)
  crontab_content = result.output if result.returncode == 0 else ''
  crontab_content += ('SHELL=/bin/bash\nUSER=chrome-bot\n'
                      '@reboot cd /b/build/slave && make start\n')
  RunCommand(['crontab', '-'], input=crontab_content)


def TuneSystemSettings():
  """Tune the system settings for our build environment."""
  # Increase the user-level file descriptor limits.
  entries = ('*       soft    nofile  65536\n'
             '*       hard    nofile  65536\n')
  SudoRunCommand(['tee', '-a', '/etc/security/limits.conf'], input=entries)


def main(_argv):
  assert getpass.getuser() == bot_constants.BUILDBOT_USER, (
      'This script should be run by %s instead of %s!' % (
          bot_constants.BUILDBOT_USER, getpass.getuser()))
  SetupPrerequisites()
  InstallChromeDependencies()
  SetupCredentials()
  SetupBuildbotEnvironment()
  TuneSystemSettings()
