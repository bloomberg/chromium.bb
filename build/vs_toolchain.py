# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import pipes
import shutil
import subprocess
import sys
import tempfile


script_dir = os.path.dirname(os.path.realpath(__file__))
chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(1, os.path.join(chrome_src, 'tools'))
sys.path.insert(0, os.path.join(chrome_src, 'tools', 'gyp', 'pylib'))


import gyp


def GetDesiredVsToolchainHashes():
  """Load a list of SHA1s corresponding to the toolchains that we want installed
  to build with."""
  sha1path = os.path.join(script_dir, 'toolchain_vs2013.hash')
  with open(sha1path, 'rb') as f:
    return f.read().strip().splitlines()


def DownloadVsToolchain():
  """Download the Visual Studio toolchain on Windows.

  If on Windows, request that depot_tools install/update the automatic
  toolchain, and then use it (unless opted-out) and return a tuple containing
  the x64 and x86 paths. Otherwise return None.
  """
  vs2013_runtime_dll_dirs = None
  depot_tools_win_toolchain = \
      bool(int(os.environ.get('DEPOT_TOOLS_WIN_TOOLCHAIN', '1')))
  if sys.platform in ('win32', 'cygwin') and depot_tools_win_toolchain:
    import find_depot_tools
    depot_tools_path = find_depot_tools.add_depot_tools_to_path()
    temp_handle, data_file = tempfile.mkstemp(suffix='.json')
    os.close(temp_handle)
    get_toolchain_args = [
        sys.executable,
        os.path.join(depot_tools_path,
                    'win_toolchain',
                    'get_toolchain_if_necessary.py'),
        '--output-json', data_file,
      ] + GetDesiredVsToolchainHashes()
    subprocess.check_call(get_toolchain_args)

    with open(data_file, 'r') as tempf:
      toolchain_data = json.load(tempf)
    os.unlink(data_file)

    toolchain = toolchain_data['path']
    version = toolchain_data['version']
    version_is_pro = version[-1] != 'e'
    win8sdk = toolchain_data['win8sdk']
    wdk = toolchain_data['wdk']
    # TODO(scottmg): The order unfortunately matters in these. They should be
    # split into separate keys for x86 and x64. (See CopyVsRuntimeDlls call
    # below). http://crbug.com/345992
    vs2013_runtime_dll_dirs = toolchain_data['runtime_dirs']

    os.environ['GYP_MSVS_OVERRIDE_PATH'] = toolchain
    os.environ['GYP_MSVS_VERSION'] = version
    # We need to make sure windows_sdk_path is set to the automated
    # toolchain values in GYP_DEFINES, but don't want to override any
    # otheroptions.express
    # values there.
    gyp_defines_dict = gyp.NameValueListToDict(gyp.ShlexEnv('GYP_DEFINES'))
    gyp_defines_dict['windows_sdk_path'] = win8sdk
    os.environ['GYP_DEFINES'] = ' '.join('%s=%s' % (k, pipes.quote(str(v)))
        for k, v in gyp_defines_dict.iteritems())
    os.environ['WINDOWSSDKDIR'] = win8sdk
    os.environ['WDK_DIR'] = wdk
    # Include the VS runtime in the PATH in case it's not machine-installed.
    runtime_path = ';'.join(vs2013_runtime_dll_dirs)
    os.environ['PATH'] = runtime_path + ';' + os.environ['PATH']
  return vs2013_runtime_dll_dirs


def CopyVsRuntimeDlls(output_dir, runtime_dirs):
  """Copies the VS runtime DLLs from the given |runtime_dirs| to the output
  directory so that even if not system-installed, built binaries are likely to
  be able to run.

  This needs to be run after gyp has been run so that the expected target
  output directories are already created.
  """
  assert sys.platform.startswith(('win32', 'cygwin'))

  def copy_runtime(target_dir, source_dir, dll_pattern):
    """Copy both the msvcr and msvcp runtime DLLs, only if the target doesn't
    exist, but the target directory does exist."""
    for which in ('p', 'r'):
      dll = dll_pattern % which
      target = os.path.join(target_dir, dll)
      source = os.path.join(source_dir, dll)
      # If gyp generated to that output dir, and the runtime isn't already
      # there, then copy it over.
      if (os.path.isdir(target_dir) and
          (not os.path.isfile(target) or
            os.stat(target).st_mtime != os.stat(source).st_mtime)):
        print 'Copying %s to %s...' % (source, target)
        if os.path.exists(target):
          os.unlink(target)
        shutil.copy2(source, target)

  x86, x64 = runtime_dirs
  out_debug = os.path.join(output_dir, 'Debug')
  out_debug_nacl64 = os.path.join(output_dir, 'Debug', 'x64')
  out_release = os.path.join(output_dir, 'Release')
  out_release_nacl64 = os.path.join(output_dir, 'Release', 'x64')
  out_debug_x64 = os.path.join(output_dir, 'Debug_x64')
  out_release_x64 = os.path.join(output_dir, 'Release_x64')

  if os.path.exists(out_debug) and not os.path.exists(out_debug_nacl64):
    os.makedirs(out_debug_nacl64)
  if os.path.exists(out_release) and not os.path.exists(out_release_nacl64):
    os.makedirs(out_release_nacl64)
  copy_runtime(out_debug,          x86, 'msvc%s120d.dll')
  copy_runtime(out_release,        x86, 'msvc%s120.dll')
  copy_runtime(out_debug_x64,      x64, 'msvc%s120d.dll')
  copy_runtime(out_release_x64,    x64, 'msvc%s120.dll')
  copy_runtime(out_debug_nacl64,   x64, 'msvc%s120d.dll')
  copy_runtime(out_release_nacl64, x64, 'msvc%s120.dll')


def main():
  if len(sys.argv) < 2:
    print >>sys.stderr, 'Expected either "get_toolchain_dir" or "copy_dlls"'
    return 1
  if sys.argv[1] == 'get_toolchain_dir':
    DownloadVsToolchain()
    print '["%s", "%s"]' % (
        os.environ['GYP_MSVS_OVERRIDE_PATH'], os.environ['WINDOWSSDKDIR'])
  else:
    print >>sys.stderr, 'TODO: not implemented "%s"' % sys.argv[1]
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
