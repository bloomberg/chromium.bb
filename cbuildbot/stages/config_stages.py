# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the config stages."""

from __future__ import print_function

import os
import re
import traceback
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.cbuildbot import repository
from chromite.lib import cros_logging as logging
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils

GS_GE_TEMPLATE_BUCKET = 'gs://chromeos-build-release-console/'
GS_GE_TEMPLATE_TOT = GS_GE_TEMPLATE_BUCKET + 'build_config.ToT.json'
GS_GE_TEMPLATE_RELEASE = GS_GE_TEMPLATE_BUCKET + 'build_config.release-R*'
GE_BUILD_CONFIG_FILE = 'ge_build_config.json'

class UpdateConfigException(Exception):
  """Failed to update configs."""

class BranchNotFoundException(Exception):
  """Didn't find the corresponding branch."""

def GetWorkDir(project):
  """Return temporary work directory."""
  return os.path.join('tmp', 'tmp_%s' % project)

def CreateTmpGitRepo(project, project_url):
  """Create a temporary git repo locally.

  Args:
    project: git project name to clone.
    project_url: git project url to clone.

  Returns:
    project_dir: local project directory.
  """

  work_dir = GetWorkDir(project)

  if not cros_build_lib.IsInsideChroot():
    work_dir = os.path.join(constants.SOURCE_ROOT,
                            constants.DEFAULT_CHROOT_DIR,
                            work_dir)

  # Delete the work_dir built by previous runs.
  osutils.RmDir(work_dir, ignore_missing=True, sudo=True)

  # Safely create work_dir
  osutils.SafeMakedirs(work_dir)

  project_dir = os.path.join(work_dir, project)
  ref = os.path.join(constants.SOURCE_ROOT, project)

  logging.info('Cloning %s %s to %s', project_url, ref, project_dir)
  repository.CloneWorkingRepo(dest=project_dir,
                              url=project_url,
                              reference=ref)
  return project_dir

def GetBranchName(template_file):
  """Parse the template gs path and return the right branch name"""
  match = re.search(r'build_config\.(.+?)\.json', template_file)
  if match:
    if match.group(1) == 'ToT':
      # Given 'build_config.ToT.json',
      # return branch name 'master'.
      return 'master'
    else:
      # Given 'build_config.release-R51-8172.B.json',
      # return branch name 'release-R51-8172.B'.
      return match.group(1)
  else:
    return None

class CheckTemplateStage(generic_stages.BuilderStage):
  """Stage that checks template files from GE bucket.

  This stage lists template files from GE bucket,
  triggers config updates if necessary.
  """

  def __init__(self, builder_run, **kwargs):
    super(CheckTemplateStage, self).__init__(builder_run, **kwargs)
    self.ctx = gs.GSContext(init_boto=True)

  def SortAndGetReleasePaths(self, release_list):
    def _GetMilestone(file_name):
      # Given 'build_config.release-R51-8172.B.json',
      # search for milestone number '51'.
      match = re.search(r'build_config\.release-R(.+?)-.+?\.json',
                        os.path.basename(file_name))
      if match:
        return int(match.group(1))
      return None

    milestone_path_pairs = []
    for release_template in release_list:
      milestone_num = _GetMilestone(release_template)
      # Enable config-updater builder for master branch
      # and release branches with milestone_num > 53
      if milestone_num and milestone_num > 53:
        milestone_path_pairs.append((milestone_num, release_template))
    milestone_path_pairs.sort(reverse=True)

    if len(release_list) <= 3:
      return [i[1] for i in milestone_path_pairs]
    else:
      return [i[1] for i in milestone_path_pairs[0: 3]]

  def _ListTemplates(self):
    template_gs_paths = []
    tot_gs_path = self.ctx.LS(GS_GE_TEMPLATE_TOT)
    if tot_gs_path:
      template_gs_paths.extend(tot_gs_path)

    release_gs_paths = self.SortAndGetReleasePaths(
        self.ctx.LS(GS_GE_TEMPLATE_RELEASE))
    if release_gs_paths:
      template_gs_paths.extend(release_gs_paths)

    return template_gs_paths

  def PerformStage(self):
    template_gs_paths = self._ListTemplates()

    if not template_gs_paths:
      logging.info('No template files found. No need to update configs.')
      return

    chromite_dir = CreateTmpGitRepo('chromite', constants.CHROMITE_URL)
    successful = True
    failed_templates = []
    for template_gs_path in template_gs_paths:
      try:
        branch = GetBranchName(os.path.basename(template_gs_path))
        if branch:
          UpdateConfigStage(self._run, template_gs_path, branch,
                            chromite_dir, suffix='_' + branch).Run()
      except Exception as e:
        successful = False
        failed_templates.append(template_gs_path)
        logging.error('Failed to update configs for %s: %s',
                      template_gs_path, e)
        traceback.print_exc()

    # If UpdateConfigStage failures happened, raise a exception
    if not successful:
      raise UpdateConfigException('Failed to update config for %s' %
                                  failed_templates)

class UpdateConfigStage(generic_stages.BuilderStage):
  """Stage that verifies and updates configs.

  This stage gets the template file from GE bucket,
  checkout the corresponding branch, generates configs
  based on the new template file, verifies the changes,
  and submits the changes to the corresponding branch.
  """

  CONFIG_DUMP_PATH = os.path.join('cbuildbot', 'config_dump.json')
  WATERFALL_DUMP_PATH = os.path.join('cbuildbot', 'waterfall_layout_dump.txt')
  GE_CONFIG_LOCAL_PATH = os.path.join('cbuildbot', GE_BUILD_CONFIG_FILE)

  def __init__(self, builder_run, template_gs_path,
               branch, chromite_dir, **kwargs):
    super(UpdateConfigStage, self).__init__(builder_run, **kwargs)
    self.template_gs_path = template_gs_path
    self.chromite_dir = chromite_dir
    self.branch = branch

    self.ctx = gs.GSContext(init_boto=True)
    self.dryrun = self._run.options.debug

  def _CheckoutBranch(self):
    """Checkout to the corresponding branch in the temp repository.

    Raises:
      BranchNotFoundException if failed to checkout to the branch.
    """
    git.RunGit(self.chromite_dir, ['checkout', self.branch])

    output = git.RunGit(self.chromite_dir,
                        ['rev-parse', '--abbrev-ref', 'HEAD']).output
    current_branch = output.rstrip()

    if current_branch != self.branch:
      raise BranchNotFoundException(
          "Failed to checkout to branch %s." % self.branch)

  def _DownloadTemplate(self):
    """Download the template file from gs."""
    dest_path = os.path.join(self.chromite_dir, 'cbuildbot',
                             GE_BUILD_CONFIG_FILE)
    self.ctx.Copy(self.template_gs_path, dest_path)

  def _ContainsUpdates(self, mod_files):
    """Check if updates exist and requires a push.

    Args:
      mod_files: files to check updates.

    Returns:
      True if updates exist; otherwise False.
    """
    modifications = git.RunGit(self.chromite_dir,
                               ['status', '--porcelain', '--'] + mod_files,
                               capture_output=True, print_cmd=True).output
    if modifications:
      logging.info('Changed files: %s ' % modifications)
      return True
    else:
      return False

  def _UpdateConfigDump(self):
    """Generate and dump configs base on the new template_file"""
    # Clear the cached SiteConfig, if there was one.
    config_lib.ClearConfigCache()

    view_config_path = os.path.join(
        self.chromite_dir, 'bin', 'cbuildbot_view_config')
    cmd = [view_config_path, '--update']

    try:
      cros_build_lib.RunCommand(cmd, cwd=os.path.dirname(self.chromite_dir))
    except:
      logging.error('Failed to update configs. Please check the format of the '
                    'remote template file %s and the local template copy %s',
                    self.template_gs_path, self.CONFIG_DUMP_PATH)
      raise

    show_waterfall_path = os.path.join(
        self.chromite_dir, 'bin', 'cros_show_waterfall_layout')
    cmd = [show_waterfall_path]
    layout_file_name = os.path.join(
        self.chromite_dir, 'cbuildbot/waterfall_layout_dump.txt')
    with cros_build_lib.OutputCapturer(stdout_path=layout_file_name):
      cros_build_lib.RunCommand(cmd, cwd=os.path.dirname(self.chromite_dir))

  def _RunUnitTest(self):
    """Run chromeos_config_unittest on top of the changes."""
    logging.debug("Running chromeos_config_unittest")
    rel_path = os.path.join('..', '..', 'chroot', GetWorkDir('chromite'))
    unit_test_paths = [os.path.join(rel_path, 'chromite', 'cbuildbot',
                                    'chromeos_config_unittest')]
    cmd = ['cros_sdk', '--'] + unit_test_paths
    cros_build_lib.RunCommand(cmd, cwd=os.path.dirname(self.chromite_dir))

  def _PushCommits(self, candidate_files):
    """Commit and push changes to current branch.

    Args:
      candidate_files: candidate files to commit and update.
    """

    git.RunGit(self.chromite_dir, ['add'] + candidate_files)
    commit_msg = "Update config settings by config-updater."
    git.RunGit(self.chromite_dir,
               ['commit', '-m', commit_msg],
               print_cmd=True)

    git.RunGit(self.chromite_dir, ['config', 'push.default', 'tracking'])
    git.PushWithRetry(self.branch, self.chromite_dir, dryrun=self.dryrun)

  def PerformStage(self):
    logging.info('Update configs for branch %s, template gs path %s',
                 self.branch, self.template_gs_path)
    candidate_files = [self.GE_CONFIG_LOCAL_PATH,
                       self.CONFIG_DUMP_PATH,
                       self.WATERFALL_DUMP_PATH]
    try:
      self._CheckoutBranch()
      self._DownloadTemplate()
      self._UpdateConfigDump()
      self._RunUnitTest()
      if self._ContainsUpdates(candidate_files):
        self._PushCommits(candidate_files)
      else:
        logging.info('Nothing changed. No need to update configs for %s',
                     self.template_gs_path)
    finally:
      git.CleanAndDetachHead(self.chromite_dir)
