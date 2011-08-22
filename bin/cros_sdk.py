#!/usr/bin/env python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script fetches and prepares an SDK chroot.
"""

import optparse
import os
import subprocess
import sys
import urllib
import urlparse


sys.path.insert(0, os.path.abspath(__file__ + '/../..'))
import lib.cros_build_lib as cros_build_lib
import buildbot.constants as constants


DEFAULT_CHROOT_DIR = 'chroot'
DEFAULT_URL = 'http://commondatastorage.googleapis.com/chromiumos-sdk/'
SRC_ROOT = os.path.realpath(constants.SOURCE_ROOT)
SDK_DIR = os.path.join(SRC_ROOT, 'sdks')
OVERLAY_DIR = os.path.join(SRC_ROOT, 'src/third_party/chromiumos-overlay')
SDK_VERSION_FILE = os.path.join(OVERLAY_DIR,
                                'chromeos/binhost/host/sdk_version.conf')

# TODO(zbehan): Remove the dependency on these, reimplement them in python
MAKE_CHROOT = [os.path.join(OVERLAY_DIR, 'chromeos/scripts/make_chroot'),
               '--cros_sdk']
ENTER_CHROOT = [os.path.join(SRC_ROOT, 'src/scripts/sdk_lib/enter_chroot.sh')]

# We need these tools to run. Very common tools (tar,..) are ommited.
NEEDED_TOOLS = ['curl']

def GetHostArch():
  """Returns a string for the host architecture"""
  out = cros_build_lib.RunCommand(['uname', '-m'],
      redirect_stdout=True, print_cmd=False).output
  return out.rstrip('\n')

def CheckPrerequisites(needed_tools):
  """Verifies that the required tools are present on the system.

  This is especially important as this script is intended to run
  outside the chroot.

  Arguments:
    needed_tools: an array of string specified binaries to look for.

  Returns:
    True if all needed tools were found.
  """
  for tool in needed_tools:
    cmd = ['which', tool]
    try:
      cros_build_lib.RunCommand(cmd, print_cmd=False, redirect_stdout=True,
                                combine_stdout_stderr=True)
    except cros_build_lib.RunCommandError:
      print 'The tool \'' + tool + '\' not found.'
      print 'Please install the appropriate package in your host.'
      print 'Example(ubuntu):'
      print '  sudo apt-get install <packagename>'
      return False
  return True

def GetLatestVersion():
  """Extracts latest version from chromiumos-overlay."""
  sdk_file = open(SDK_VERSION_FILE)
  buf = sdk_file.readline().rstrip('\n').split('=')
  if buf[0] != 'SDK_LATEST_VERSION':
    raise Exception('Malformed version file')
  return buf[1].strip('"')


def GetArchStageTarball(tarballArch, version):
  """Returns the URL for a given arch/version"""
  D = { 'x86_64': 'cros-sdk-' }
  try:
    return DEFAULT_URL + D[tarballArch] + version + '.tbz2'
  except KeyError:
    sys.exit('Unsupported arch: ' + arch)


def FetchRemoteTarball(url):
  """Fetches a tarball given by url, and place it in sdk/."""
  tarball_dest = os.path.join(SDK_DIR,
      os.path.basename(urlparse.urlparse(url).path))

  print 'Downloading sdk: "%s"' % url
  cmd = ['curl', '-f', '--retry', '5', '-L', '-y', '30',
         '--output', tarball_dest]

  if not url.startswith('file://'):
    # Only resume for remote URLs. If the file is local, there's no
    # real speedup, and using the same filename for different files
    # locally will cause issues.
    cmd.extend(['-C', '-'])

  cmd.append(url)

  cros_build_lib.RunCommand(cmd)
  return tarball_dest


def BootstrapChroot(chroot_path, stage_url, replace):
  """Builds a new chroot from source"""
  cmd = MAKE_CHROOT + ['--chroot', chroot_path,
                       '--nousepkg']

  stage = None
  if stage_url:
    stage = FetchRemoteTarball(stage_url)

  if stage:
    cmd.extend(['--stage3_path', stage])

  if replace:
    cmd.append('--replace')

  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    print 'Running %r failed!' % cmd
    sys.exit(1)


def CreateChroot(sdk_url, sdk_version, chroot_path, replace):
  """Creates a new chroot from a given SDK"""
  if not os.path.exists(SDK_DIR):
    cros_build_lib.RunCommand(['mkdir', '-p', SDK_DIR], print_cmd=False)

  # Based on selections, fetch the tarball
  if sdk_url:
    url = sdk_url
  else:
    arch = GetHostArch()
    if sdk_version:
      url = GetArchStageTarball(arch, sdk_version)
    else:
      url = GetArchStageTarball(arch)

  sdk = FetchRemoteTarball(url)

  # TODO(zbehan): Unpack and install
  # For now, we simply call make_chroot on the prebuilt chromeos-sdk.
  # make_chroot provides a variety of hacks to make the chroot useable.
  # These should all be eliminated/minimised, after which, we can change
  # this to just unpacking the sdk.
  cmd = MAKE_CHROOT + ['--stage3_path', sdk,
                       '--chroot', chroot_path]

  if replace:
    cmd.append('--replace')

  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    print 'Running %r failed!' % cmd
    sys.exit(1)


def DeleteChroot(chroot_path):
  """Deletes an existing chroot"""
  cmd = MAKE_CHROOT + ['--chroot', chroot_path,
                       '--delete']
  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    print 'Running %r failed!' % cmd
    sys.exit(1)


def EnterChroot(chroot_path, chrome_root, chrome_root_mount, additional_args):
  """Enters an existing SDK chroot"""
  cmd = ENTER_CHROOT + ['--chroot', chroot_path]
  if chrome_root:
    cmd.extend(['--chrome_root', chrome_root])
  if chrome_root_mount:
    cmd.extend(['--chrome_root_mount', chrome_root_mount])
  if len(additional_args) > 0:
    cmd.append('--')
    cmd.extend(additional_args)
  try:
    cros_build_lib.RunCommand(cmd, print_cmd=False)
  except cros_build_lib.RunCommandError:
    print 'Running %r failed!' % cmd
    sys.exit(1)


def RefreshSudoCredentials():
  """Runs sudo on a harmless command to request sudo credentials explicitly."""
  cros_build_lib.RunCommand(['sudo', 'true'], print_cmd=False)


def main():
  usage="""usage: %prog [options] [VAR1=val1 .. VARn=valn -- <args>]

This script manages a local CrOS SDK chroot. Depending on the flags,
it can download, build or enter a chroot.

Action taken is the following:
--enter  (default)  .. Installs and enters a chroot
--download          .. Just download a chroot (enter if combined with --enter)
--bootstrap         .. Builds a chroot from source (enter if --enter)
--delete            .. Removes a chroot
"""
  sdk_latest_version = GetLatestVersion()
  parser = optparse.OptionParser(usage)
  # Actions:
  parser.add_option('', '--bootstrap',
                    action='store_true', dest='bootstrap', default=False,
                    help=('Build a new SDK chroot from source'))
  parser.add_option('', '--delete',
                    action='store_true', dest='delete', default=False,
                    help=('Delete the current SDK chroot'))
  parser.add_option('', '--download',
                    action='store_true', dest='download', default=False,
                    help=('Download and download a prebuilt SDK'))
  parser.add_option('', '--enter',
                    action='store_true', dest='enter', default=False,
                    help=('Enter the SDK chroot, possibly (re)create first'))

  # Global options:
  parser.add_option('', '--chroot',
                    dest='chroot', default=DEFAULT_CHROOT_DIR,
                    help=('SDK chroot dir name [%s]' % DEFAULT_CHROOT_DIR))

  # Additional options:
  parser.add_option('', '--chrome_root',
                    dest='chrome_root', default='',
                    help=('Mount this chrome root into the SDK chroot'))
  parser.add_option('', '--chrome_root_mount',
                    dest='chrome_root_mount', default='',
                    help=('Mount chrome into this path inside SDK chroot'))
  parser.add_option('-r', '--replace',
                    action='store_true', dest='replace', default=False,
                    help=('Replace an existing SDK chroot'))
  parser.add_option('-u', '--url',
                    dest='sdk_url', default='',
                    help=('''Use sdk tarball located at this url.
                             Use file:// for local files.'''))
  parser.add_option('-v', '--version',
                    dest='sdk_version', default='',
                    help=('Use this sdk version [%s]' % sdk_latest_version))
  (options, remaining_arguments) = parser.parse_args()

  # Some sanity checks first, before we ask for sudo credentials.
  if cros_build_lib.IsInsideChroot():
    print "This needs to be ran outside the chroot"
    sys.exit(1)

  if not CheckPrerequisites(NEEDED_TOOLS):
    sys.exit(1)

  # Default action is --enter, if no other is selected.
  if not (options.bootstrap or options.download or options.delete):
    options.enter = True

  # Only --enter can process additional args as passthrough commands.
  # Warn and exit for least surprise.
  if len(remaining_arguments) > 0 and not options.enter:
    print "Additional arguments not permitted, unless running with --enter"
    parser.print_help()
    sys.exit(1)

  # Some actions can be combined, as they merely modify how is the chroot
  # going to be made. The only option that hates all others is --delete.
  if options.delete and \
    (options.enter or options.download or options.bootstrap):
    print "--delete cannot be combined with --enter, --download or --bootstrap"
    parser.print_help()
    sys.exit(1)
  # NOTE: --delete is a true hater, it doesn't like other options either, but
  # those will hardly lead to confusion. Nobody can expect to pass --version to
  # delete and actually change something.

  if options.bootstrap and options.download:
    print "Either --bootstrap or --download, not both"
    sys.exit(1)

  # Bootstrap will start off from a non-selectable stage3 tarball. Attempts to
  # select sdk by version are confusing. Warn and exit. We can still specify a
  # tarball by path or URL though.
  if options.bootstrap and options.sdk_version:
    print "Cannot use --version when bootstrapping"
    parser.print_help()
    sys.exit(1)

  chroot_path = os.path.join(SRC_ROOT, options.chroot)

  if not options.sdk_version:
    sdk_version = sdk_latest_version
  else:
    sdk_version = options.sdk_version

  if options.delete and not os.path.exists(chroot_path):
    print "Not doing anything. The chroot you want to remove doesn't exist."
    sys.exit(0)

  # Request sudo credentials before we do anything else, to not ask for them
  # inside of a lengthy process.
  RefreshSudoCredentials()

  if options.delete:
    DeleteChroot(chroot_path)
    sys.exit(0)

  # Print a suggestion for replacement, but not if running just --enter.
  if os.path.exists(chroot_path) and not options.replace and \
     (options.bootstrap or options.download):
      print "Chroot already exists. Run with --replace to re-create."

  # Chroot doesn't exist or asked to replace.
  if not os.path.exists(chroot_path) or options.replace:
    if options.bootstrap:
      BootstrapChroot(chroot_path, options.sdk_url,
                      options.replace)
    else:
      CreateChroot(options.sdk_url, sdk_version,
                   chroot_path, options.replace)

  if options.enter:
    EnterChroot(chroot_path, options.chrome_root, options.chrome_root_mount,
                remaining_arguments)


if __name__ == '__main__':
  main()
