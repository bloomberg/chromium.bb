#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install GTest and GMock that can be linked to a NaCl module.  By default
this script builds both the 32- and 64-bit versions of the libraries, and
installs them in <toolchain>/nacl/usr/lib and <toolchain>/nacl64/usr/lib.  The
header files are also installed in <toolchain>/<ncal_spec>/usr/include.
"""

import build_utils
import os
import shutil
import subprocess
import sys
import tempfile
import urllib

from optparse import OptionParser

# Default values for the --toolchain and --bit-spec command line arguments.
BIT_SPEC_DEFAULT = "32,64"

# The original gtest distro can be found here:
# http://code.google.com/p/googletest/downloads/detail?name=gtest-1.5.0.tar.gz
GTEST_URL = ("http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/"
             "gtest-1.5.0.tgz")
GTEST_PATH = "gtest-1.5.0"
GTEST_PATCH_FILE = "nacl-gtest-1.5.0.patch"

# The original gmock distro can be found here:
# http://googlemock.googlecode.com/files/gmock-1.5.0.tar.gz
GMOCK_URL = ("http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/"
             "gmock-1.5.0.tgz")
GMOCK_PATH = "gmock-1.5.0"
GMOCK_PATCH_FILE = "nacl-gmock-1.5.0.patch"


# Create a temporary working directory.  This is where all the tar files go
# and where the packages get built prior to installation in the toolchain.
def MakeWorkingDir(options):
  # Pick work directory.
  if not options.working_dir:
    options.working_dir = tempfile.mkdtemp(prefix='gtest')
    options.must_clean_up = True
  # Go into working area.
  options.old_cwd = os.getcwd()
  os.chdir(options.working_dir)


# Download GTest and GMock into the working directory, then extract them.
def DownloadAndExtractAll(options):
  def DownloadAndExtract(url, path):
    print "Download: %s" % url
    (zip_file, headers) = urllib.urlretrieve(url, '%s.tgz' % path)
    p = subprocess.Popen('tar xzf %s' % (zip_file),
                         env=options.shell_env,
                         shell=True)
    assert p.wait() == 0

  os.chdir(options.working_dir)
  try:
    DownloadAndExtract(GTEST_URL, GTEST_PATH)
    DownloadAndExtract(GMOCK_URL, GMOCK_PATH)
  except (URLError, ContentTooShortError):
    os.chdir(options.old_cwd)
    print "Error retrieving %s" % url
    raise

  os.chdir(options.old_cwd)


# Apply the patch files to the extracted GTest and GMock pacakges.
def PatchAll(options):
  def Patch(abs_path, patch_file):
    print "Patching %s with: %s" % (abs_path, patch_file)
    p = subprocess.Popen('chmod -R a+w . && patch -p0 < %s' % (patch_file),
                         cwd=abs_path,
                         env=options.shell_env,
                         shell=True)
    assert p.wait() == 0

  Patch(options.working_dir, os.path.join(options.script_dir, GTEST_PATCH_FILE))
  Patch(options.working_dir, os.path.join(options.script_dir, GMOCK_PATCH_FILE))


# Build GTest and GMock, then install them into the toolchain.  Note that
# GTest has to be built and installed into the toolchain before GMock can be
# built, because GMock relies on headers from GTest.  This method sets up all
# the necessary shell environment variables for the makefiles, such as CC and
# CXX.
def BuildAndInstallAll(options):
  def BuildInPath(abs_path, shell_env):
    # Run make clean and make in |abs_path|.  Assumes there is a makefile in
    # |abs_path|, if not then the assert will trigger.
    print "Building in %s" % (abs_path)
    p = subprocess.Popen('make clean && make -j4',
                         cwd=abs_path,
                         env=shell_env,
                         shell=True)
    assert p.wait() == 0

  def InstallLib(lib, src_path, dst_path, shell_env):
    # Use the install untility to install |lib| from |src_path| into
    # |dst_path|.
    p = subprocess.Popen("install -m 644 %s %s" % (lib, dst_path),
                         cwd=src_path,
                         env=shell_env,
                         shell=True)
    assert p.wait() == 0

  # Build and install for each bit-spec.
  for bit_spec in options.bit_spec.split(','):
    if bit_spec == '64':
      nacl_spec = 'x86_64-nacl'
    else:
      nacl_spec = 'i686-nacl'

    print 'Building gtest and gmock for NaCl spec: %s.' % nacl_spec
    # Make sure the target directories exist.
    nacl_usr_include = os.path.join(options.toolchain,
                                    nacl_spec,
                                    'usr',
                                    'include')
    nacl_usr_lib = os.path.join(options.toolchain,
                                nacl_spec,
                                'usr',
                                'lib')
    build_utils.ForceMakeDirs(nacl_usr_include)
    build_utils.ForceMakeDirs(nacl_usr_lib)

    # Set up the nacl-specific environment variables used by make.
    build_env = options.shell_env.copy()
    toolchain_bin = os.path.join(options.toolchain, 'bin')
    build_env['CC'] = os.path.join(toolchain_bin, '%s-gcc' % nacl_spec)
    build_env['CXX'] = os.path.join(toolchain_bin, '%s-g++' % nacl_spec)
    build_env['AR'] = os.path.join(toolchain_bin, '%s-ar' % nacl_spec)
    build_env['RANLIB'] = os.path.join(toolchain_bin, '%s-ranlib' % nacl_spec)
    build_env['LD'] = os.path.join(toolchain_bin, '%s-ld' % nacl_spec)
    build_env['NACL_TOOLCHAIN_ROOT'] = options.toolchain

    # GTest has to be built & installed before GMock can be built.
    gtest_path = os.path.join(options.working_dir, GTEST_PATH)
    BuildInPath(gtest_path, build_env)
    gtest_tar_excludes = "--exclude='gtest-death-test.h' \
                          --exclude='gtest-death-test-internal.h'"
    tar_cf = subprocess.Popen("tar cf - %s gtest" % gtest_tar_excludes,
                              cwd=os.path.join(gtest_path, 'include'),
                              env=build_env,
                              shell=True,
                              stdout=subprocess.PIPE)
    tar_xf = subprocess.Popen("tar xfp -",
                              cwd=nacl_usr_include,
                              env=build_env,
                              shell=True,
                              stdin=tar_cf.stdout)
    tar_copy_err = tar_xf.communicate()[1]
    InstallLib('libgtest.a', gtest_path, nacl_usr_lib, build_env)

    gmock_path = os.path.join(options.working_dir, GMOCK_PATH)
    BuildInPath(gmock_path, build_env)
    tar_cf = subprocess.Popen("tar cf - gmock",
                              cwd=os.path.join(gmock_path, 'include'),
                              env=options.shell_env,
                              shell=True,
                              stdout=subprocess.PIPE)
    tar_xf = subprocess.Popen("tar xfp -",
                              cwd=nacl_usr_include,
                              env=options.shell_env,
                              shell=True,
                              stdin=tar_cf.stdout)
    tar_copy_err = tar_xf.communicate()[1]
    InstallLib('libgmock.a', gmock_path, nacl_usr_lib, build_env)


# Main driver method that creates a working directory, then downloads and
# extracts GTest and GMock, patches and builds them both, then installs them
# into the toolchain specified in |options.toolchain|.
def InstallTestingLibs(options):
  MakeWorkingDir(options)
  try:
    DownloadAndExtractAll(options)
    PatchAll(options)
  except:
    return 1

  BuildAndInstallAll(options)
  # Clean up.
  if options.must_clean_up:
    shutil.rmtree(options.working_dir, ignore_errors=True)
  return 0


# Parse the command-line args and set up the options object.  There are two
# command-line switches:
#   --toolchain=<path to the platform-specific toolchain>
#               e.g.: --toolchain=../toolchain/mac-x86
#               default is |TOOLCHAIN_AUTODETECT| which means try to determine
#               the toolchain dir from |sys.platform|.
#   --bit-spec=<comma-separated list of instruction set bit-widths
#              e.g.: --bit_spec=32,64
#              default is "32,64" which means build 32- and 64-bit versions
#              of the libraries.
def main(argv):
  shell_env = os.environ;
  if not build_utils.CheckPatchVersion(shell_env):
    sys.exit(0)

  parser = OptionParser()
  parser.add_option(
      '-t', '--toolchain', dest='toolchain',
      default=build_utils.TOOLCHAIN_AUTODETECT,
      help='where to put the testing libraries')
  parser.add_option(
      '-b', '--bit-spec', dest='bit_spec',
      default=BIT_SPEC_DEFAULT,
      help='comma separated list of instruction set bit-widths')
  parser.add_option(
      '-w', '--working_dir', dest='working_dir',
      default=None,
      help='where to untar and build the test libs.')
  (options, args) = parser.parse_args(argv)
  if args:
    print 'ERROR: invalid argument: %s' % str(args)
    parser.print_help()
    sys.exit(1)

  options.must_clean_up = False
  options.shell_env = shell_env
  options.script_dir = os.path.abspath(os.path.dirname(__file__))
  options.toolchain = build_utils.NormalizeToolchain(options.toolchain)
  print "Installing testing libs into toolchain %s" % options.toolchain

  return InstallTestingLibs(options)


if __name__ == '__main__':
  main(sys.argv[1:])
