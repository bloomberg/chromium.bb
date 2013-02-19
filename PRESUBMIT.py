# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Documentation on PRESUBMIT.py can be found at:
# http://www.chromium.org/developers/how-tos/depottools/presubmit-scripts

import os.path
import sys

# List of directories to not apply presubmit project checks, relative
# to the NaCl top directory
EXCLUDE_PROJECT_CHECKS_DIRS = [
    # The following contain test data (including automatically generated),
    # and do not follow our conventions.
    'src/trusted/validator_x86/testdata/32',
    'src/trusted/validator_x86/testdata/64',
    'src/trusted/validator/x86/decoder/generator/testdata/32',
    'src/trusted/validator/x86/decoder/generator/testdata/64',
    # The following directories contains automatically generated source,
    # which may not follow our conventions.
    'src/trusted/validator_x86/gen',
    'src/trusted/validator/x86/decoder/gen',
    'src/trusted/validator/x86/decoder/generator/gen',
    'src/trusted/validator/x86/ncval_seg_sfi/gen',
    'src/trusted/validator_arm/gen',
    'src/trusted/validator_ragel/gen',
    ]

def NaClTopDir():
  # git-cl and gcl run PRESUBMIT.py with the current directory set to
  # the native_client directory (which may have a different basename
  # in a standalone checkout).
  return os.getcwd()

def _CommonChecks(input_api, output_api):
  """Checks for both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api, project_name='Native Client',
      excluded_paths=tuple(EXCLUDE_PROJECT_CHECKS_DIRS)))
  return results

def IsFileInDirectories(f, dirs):
  """ Returns true if f is in list of directories"""
  for d in dirs:
    if d is os.path.commonprefix([f , d]):
      return True
  return False

def CheckChangeOnUpload(input_api, output_api):
  """Verifies all changes in all files.
  Args:
    input_api: the limited set of input modules allowed in presubmit.
    output_api: the limited set of output modules allowed in presubmit.
  """
  report = []
  report.extend(_CommonChecks(input_api, output_api))

  # The commit queue assumes PRESUBMIT.py is standalone.
  # TODO(bradnelson): Migrate code_hygiene to a common location so that
  # it can be used by the commit queue.
  old_sys_path = list(sys.path)
  try:
    sys.path.append(os.path.join(NaClTopDir(), 'tools'))
    sys.path.append(os.path.join(NaClTopDir(), 'build'))
    import code_hygiene
  finally:
    sys.path = old_sys_path
    del old_sys_path

  affected_files = input_api.AffectedFiles(include_deletes=False)
  exclude_dirs = [ NaClTopDir() + '/' + x + '/'
                   for x in EXCLUDE_PROJECT_CHECKS_DIRS ]
  for filename in affected_files:
    filename = filename.AbsoluteLocalPath()
    if not IsFileInDirectories(filename, exclude_dirs):
      errors, warnings = code_hygiene.CheckFile(filename, False)
      for e in errors:
        report.append(output_api.PresubmitError(e, items=errors[e]))
      for w in warnings:
        report.append(output_api.PresubmitPromptWarning(w, items=warnings[w]))

  return report


def CheckChangeOnCommit(input_api, output_api):
  """Verifies all changes in all files and verifies that the
  tree is open and can accept a commit.
  Args:
    input_api: the limited set of input modules allowed in presubmit.
    output_api: the limited set of output modules allowed in presubmit.
  """
  report = []
  report.extend(CheckChangeOnUpload(input_api, output_api))
  report.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api,
      json_url='http://nativeclient-status.appspot.com/current?format=json'))
  return report


def GetPreferredTrySlaves(_, change):

  # This is a heuristic to (conservatively) detect if the change only
  # affects the pnacl toolchain and not the main builders, and run the
  # appropriate trybots.
  is_pnacl_toolchain_only = True
  for f in change.AffectedFiles(include_dirs=True):
    if not f.LocalPath().startswith('pnacl'):
      is_pnacl_toolchain_only = False
  if is_pnacl_toolchain_only:
    return [
        'nacl-toolchain-linux-pnacl-x86_64',
        'nacl-toolchain-linux-pnacl-x86_32',
        'nacl-toolchain-mac-pnacl-x86_32',
        'nacl-toolchain-win7-pnacl-x86_64',
    ]

  # If the change only affects the new toolchain_build scheme,
  # then run only the trybots doing that build.
  if all(IsFileInDirectories(file.AbsoluteLocalPath(),
                             [os.path.join(NaClTopDir(), 'toolchain_build')])
         for file in change.AffectedFiles(include_dirs=True)):
    return [
        'nacl-toolchain-lucid64-newlib-arm',
        'nacl-toolchain-mac-newlib-arm',
        'nacl-toolchain-win7-newlib-arm',
        ]

  return [
      'nacl-lucid32_newlib_dbg',
      'nacl-lucid32_newlib_opt',
      'nacl-lucid32_glibc_opt',
      'nacl-lucid64_newlib_dbg',
      'nacl-lucid64_newlib_opt',
      'nacl-lucid64_glibc_opt',
      'nacl-lucid64_newlib_dbg_valgrind',
      'nacl-lucid64_glibc_dbg_valgrind',
      'nacl-mac10.6_newlib_opt',
      'nacl-mac10.6_glibc_opt',
      'nacl-mac10.7_newlib_opt',
      'nacl-mac10.7_glibc_opt',
      'nacl-win32_newlib_opt',
      'nacl-win32_glibc_opt',
      'nacl-win64_newlib_dbg',
      'nacl-win64_newlib_opt',
      'nacl-win64_glibc_opt',
      'nacl-arm_opt_panda',
      # arm-nacl-gcc bots
      'nacl-win7_64_arm_newlib_opt',
      'nacl-mac10.7_arm_newlib_opt',
      'nacl-lucid64_arm_newlib_opt',
      # Clang bots
      'nacl-lucid_64-newlib-dbg-clang',
      'nacl-mac10.6-newlib-dbg-clang',
      # pnacl scons bots
      'nacl-lucid_64-newlib-arm_qemu-pnacl',
      'nacl-lucid_64-newlib-x86_32-pnacl',
      'nacl-lucid_64-newlib-x86_64-pnacl',
      'nacl-precise_64-newlib-x86_64-pnacl',
      # pnacl spec2k bots
      'nacl-lucid_64-newlib-arm_qemu-pnacl-spec',
      'nacl-lucid_64-newlib-x86_32-pnacl-spec',
      'nacl-lucid_64-newlib-x86_64-pnacl-spec',
  ]
