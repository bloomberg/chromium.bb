# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create the NSIS installer for the SDK."""

import os
import subprocess

from build_tools import build_utils
from build_tools import install_nsis
from build_tools import nsis_script

# TODO(dspringer): |toolchain_manifests| is not currently used by any callers.
def MakeNsisInstaller(installer_dir,
                      sdk_version=None,
                      cwd=None,
                      toolchain_manifests=None):
  '''Create the NSIS installer

  Args:
    installer_dir: The directory containing all the artifacts that get packaged
        in the NSIS installer.

    sdk_version: A string representing the SDK version.  The string is expected
        to be a '.'-separated triple representing <major>.<minor>.<build>.  If
         this argument is None, then the default is the value returned by
        build_utils.RawVersion()

    cwd: The current working directory.  Various artifacts (such as the NSIS
        installer) are expected to be in this directory.  Defaults to the
        script's directory.

    toolchain_manifests: A dictionary of manifests for things in the
        toolchain directory.  The dictionary can have these keys:
          'files': a set of plain files
          'dirs': a set of directories
          'symlinks': a dictionary of symbolic links
          'links': a dictionary of hard links.
        For more details on these sets and dictionaries, please see the
        tar_archive module.
  '''
  if not sdk_version:
    sdk_version = build_utils.RawVersion()
  sdk_full_name = 'native_client_sdk_%s' % sdk_version.replace('.', '_')

  if not cwd:
    cwd = os.path.abspath(os.path.dirname(__file__))

  install_nsis.Install(cwd)
  script = nsis_script.NsisScript(os.path.join(cwd, 'make_sdk_installer.nsi'))
  script.install_dir = os.path.join('C:%s' % os.sep, sdk_full_name)
  script.InitFromDirectory(installer_dir)
  if toolchain_manifests:
    toolchain_manifests.PrependPath(installer_dir)
    script |= toolchain_manifests
  script.Compile()
