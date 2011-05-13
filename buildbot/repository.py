# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Repository module to handle different types of repositories the Builders use.
"""

import logging
import os

from chromite.lib import cros_build_lib as cros_lib

class SrcCheckOutException(Exception):
  """Exception gets thrown for failure to sync sources"""
  pass


class RepoRepository(object):
  """ A Class that encapsulates a repo repository.
  Args:
    repo_url: gitserver URL to fetch repo manifest from.
    directory: local path where to checkout the repository.
    branch: Branch to check out the manifest at.
    manifest: optional non-default manifest to sync at.
    is_mirror: If set, this repository is a mirror rather than a real checkout.
    local_mirror: If set, sync from git mirror rather than repo url.
  """
  def __init__(self, repo_url, directory, branch=None, manifest=None,
               is_mirror=False, local_mirror=None, clobber=False):
    self.repo_url = repo_url
    self.directory = directory
    self.branch = None
    self.manifest = manifest
    self.is_mirror = is_mirror
    self.local_mirror = local_mirror

    if clobber:
      cros_lib.RunCommand(['sudo', 'rm', '-rf', self.directory], error_ok=True)

  def Initialize(self):
    """Initializes a repository."""
    assert not os.path.exists(os.path.join(self.directory, '.repo')), \
        'Repo already initialized.'
    # Base command.
    init_cmd = ['repo', 'init', '--manifest-url', self.repo_url]

    # Handle mirrors and references.
    if self.is_mirror:
      init_cmd.append('--mirror')
    elif self.local_mirror:
      # Always sync local mirror before referencing.
      self.local_mirror.Sync()
      init_cmd.extend(['--reference', self.local_mirror.directory])

    # Handle branch / manifest options.
    if self.branch: init_cmd.extend(['--manifest-branch', self.branch])
    if self.manifest: init_cmd.extend(['--manifest-name', self.manifest])
    cros_lib.RunCommand(init_cmd, cwd=self.directory, input='\n\ny\n')

  def Sync(self):
    """Sync/update the source"""
    try:
      if self.local_mirror: self.local_mirror.Sync()
      if not os.path.exists(self.directory):
        os.makedirs(self.directory)
        self.Initialize()

      cros_lib.RunCommand(['repo', 'sync', '--quiet', '--jobs', '4'],
                          cwd=self.directory)
    except cros_lib.RunCommandError, e:
      err_msg = 'Failed to sync sources %s' % e.message
      logging.error(err_msg)
      raise SrcCheckOutException(err_msg)
