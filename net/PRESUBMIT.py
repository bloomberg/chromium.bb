# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/net.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def GetPreferredTrySlaves(project, change):
  slaves = ['win', 'linux', 'mac', 'win_sync', 'linux_sync', 'mac_sync']
  # Changes that touch NSS files will likely need a corresponding OpenSSL edit.
  # Conveniently, this one glob also matches _openssl.* changes too.
  if any('nss' in f.LocalPath() for f in change.AffectedFiles()):
    slaves.append('linux_redux')
  return slaves
