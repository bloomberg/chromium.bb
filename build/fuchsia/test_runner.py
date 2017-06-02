#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages a user.bootfs for a Fuchsia QEMU image, pulling in the runtime
dependencies of a test binary, and then uses QEMU from the Fuchsia SDK to run
it. Does not yet implement running on real hardware, or symbolization of
crashes."""

import argparse
import os
import subprocess
import sys
import tempfile


DIR_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
SDK_ROOT = os.path.join(DIR_SOURCE_ROOT, 'third_party', 'fuchsia-sdk')


def RunAndCheck(dry_run, args):
  if dry_run:
    print 'Run:', args
  else:
    subprocess.check_call(args)


def DumpFile(dry_run, name, description):
  """Prints out the contents of |name| if |dry_run|."""
  if not dry_run:
    return
  print
  print 'Contents of %s (for %s)' % (name, description)
  print '-' * 80
  with open(name) as f:
    sys.stdout.write(f.read())
  print '-' * 80


def MakeTargetImageName(common_prefix, output_directory, location):
  """Generates the relative path name to be used in the file system image.
  common_prefix: a prefix of both output_directory and location that
                 be removed.
  output_directory: an optional prefix on location that will also be removed.
  location: the file path to relativize.

  Examples:

  >>> MakeTargetImageName(common_prefix='/work/cr/src/',
  ...                     output_directory='/work/cr/src/out/fuch',
  ...                     location='/work/cr/src/base/test/data/xyz.json')
  'base/test/data/xyz.json'

  >>> MakeTargetImageName(common_prefix='/work/cr/src/',
  ...                     output_directory='/work/cr/src/out/fuch',
  ...                     location='/work/cr/src/out/fuch/icudtl.dat')
  'icudtl.dat'
  """
  assert output_directory.startswith(common_prefix)
  output_dir_no_common_prefix = output_directory[len(common_prefix):]
  assert location.startswith(common_prefix)
  loc = location[len(common_prefix):]
  if loc.startswith(output_dir_no_common_prefix):
    loc = loc[len(output_dir_no_common_prefix)+1:]
  return loc


def AddToManifest(manifest_file, target_name, source, mapper):
  """Appends |source| to the given |manifest_file| (a file object) in a format
  suitable for consumption by mkbootfs.

  If |source| is a file it's directly added. If |source| is a directory, its
  contents are recursively added.

  |source| must exist on disk at the time this function is called.
  """
  if os.path.isdir(source):
    files = [os.path.join(dp, f) for dp, dn, fn in os.walk(source) for f in fn]
    for f in files:
      # We pass None as the mapper because this should never recurse a 2nd time.
      AddToManifest(manifest_file, mapper(f), f, None)
  elif os.path.exists(source):
    manifest_file.write('%s=%s\n' % (target_name, source))
  else:
    raise Exception('%s does not exist' % source)


def BuildBootfs(output_directory, runtime_deps_path, test_name, gtest_filter,
                dry_run):
  with open(runtime_deps_path) as f:
    lines = f.readlines()

  locations_to_add = [os.path.abspath(os.path.join(output_directory, x.strip()))
                      for x in lines]
  locations_to_add.append(
      os.path.abspath(os.path.join(output_directory, test_name)))

  common_prefix = os.path.commonprefix(locations_to_add)
  target_source_pairs = zip(
      [MakeTargetImageName(common_prefix, output_directory, loc)
       for loc in locations_to_add],
      locations_to_add)

  # Add extra .so's that are required for running to system/lib
  sysroot_libs = [
    'libc++abi.so.1',
    'libc++.so.2',
    'libunwind.so.1',
  ]
  sysroot_lib_path = os.path.join(SDK_ROOT, 'sysroot', 'x86_64-fuchsia', 'lib')
  for lib in sysroot_libs:
    target_source_pairs.append(
        ('lib/' + lib, os.path.join(sysroot_lib_path, lib)))

  # Generate a little script that runs the test binaries and then shuts down
  # QEMU.
  autorun_file = tempfile.NamedTemporaryFile()
  autorun_file.write('#!/bin/sh\n')
  autorun_file.write('/system/' + os.path.basename(test_name))
  autorun_file.write(' --test-launcher-retry-limit=0')
  if gtest_filter:
    autorun_file.write(' --gtest_filter=' + gtest_filter)
  autorun_file.write('\n')
  # If shutdown happens too soon after the test completion, log statements from
  # the end of the run will be lost, so sleep for a bit before shutting down.
  autorun_file.write('sleep 3\n')
  autorun_file.write('dm poweroff\n')
  autorun_file.flush()
  os.chmod(autorun_file.name, 0750)
  DumpFile(dry_run, autorun_file.name, 'autorun')
  target_source_pairs.append(('autorun', autorun_file.name))

  # Generate an initial.config for application_manager that tells it to run
  # our autorun script with sh.
  initial_config_file = tempfile.NamedTemporaryFile()
  initial_config_file.write('''{
  "initial-apps": [
    [ "file:///boot/bin/sh", "/system/autorun" ]
  ]
}
''')
  initial_config_file.flush()
  DumpFile(dry_run, initial_config_file.name, 'initial.config')
  target_source_pairs.append(('data/application_manager/initial.config',
                              initial_config_file.name))

  manifest_file = tempfile.NamedTemporaryFile()
  bootfs_name = runtime_deps_path + '.bootfs'

  for target, source in target_source_pairs:
    AddToManifest(manifest_file.file, target, source,
                  lambda x: MakeTargetImageName(
                                common_prefix, output_directory, x))

  mkbootfs_path = os.path.join(SDK_ROOT, 'tools', 'mkbootfs')

  manifest_file.flush()
  DumpFile(dry_run, manifest_file.name, 'manifest')
  RunAndCheck(dry_run,
              [mkbootfs_path, '-o', bootfs_name,
               '--target=boot', os.path.join(SDK_ROOT, 'bootdata.bin'),
               '--target=system', manifest_file.name,
              ])
  print 'Wrote bootfs to', bootfs_name
  return bootfs_name


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--dry-run', '-n', action='store_true', default=False,
                      help="Just print commands, don't execute them.")
  parser.add_argument('--output-directory',
                      type=os.path.realpath,
                      help=('Path to the directory in which build files are'
                            ' located (must include build type).'))
  parser.add_argument('--runtime-deps-path',
                      type=os.path.realpath,
                      help='Runtime data dependency file from GN.')
  parser.add_argument('--test-name',
                      type=os.path.realpath,
                      help='Name of the the test')
  parser.add_argument('--gtest_filter',
                      help='GTest filter to use in place of any default')
  args = parser.parse_args()

  bootfs = BuildBootfs(args.output_directory, args.runtime_deps_path,
                       args.test_name, args.gtest_filter, args.dry_run)

  qemu_path = os.path.join(SDK_ROOT, 'qemu', 'bin', 'qemu-system-x86_64')

  if int(os.environ.get('CHROME_HEADLESS', 0)) == 0:
    args_for_kvm_and_cpu = ['-enable-kvm', '-cpu', 'host,migratable=no']
  else:
    args_for_kvm_and_cpu = ['-cpu', 'Haswell,+smap,-check']
  RunAndCheck(args.dry_run,
      [qemu_path, '-m', '2048', '-nographic', '-net', 'none', '-smp', '4',
       '-machine', 'q35',
       '-kernel', os.path.join(SDK_ROOT, 'kernel', 'magenta.bin'),
       '-initrd', bootfs,
       '-append', 'TERM=xterm-256color kernel.halt_on_panic=true'] +
       args_for_kvm_and_cpu)

  return 0


if __name__ == '__main__':
  sys.exit(main())
