# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the config stages."""

from __future__ import print_function

import errno
import os
import re
import textwrap
import traceback

from chromite.cbuildbot import repository
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import path_util

GS_GE_TEMPLATE_BUCKET = 'gs://chromeos-build-release-console/'
GS_GE_TEMPLATE_TOT = GS_GE_TEMPLATE_BUCKET + 'build_config.ToT.json'
GS_GE_TEMPLATE_RELEASE = GS_GE_TEMPLATE_BUCKET + 'build_config.release-R*'
GE_BUILD_CONFIG_FILE = 'ge_build_config.json'


class UpdateConfigException(Exception):
  """Failed to update configs."""


class BranchNotFoundException(Exception):
  """Didn't find the corresponding branch."""


class ConfigNotFoundException(Exception):
  """Didn't find existing config files in branch."""


def GetProjectTmpDir(project):
  """Return the project tmp directory inside chroot.

  Args:
    project: The name of the project to create tmp dir.
  """
  return os.path.join('tmp', 'tmp_%s' % project)


def GetProjectWorkDir(project):
  """Return the project work directory.

  Args:
    project: The name of the project to create work dir.
  """
  project_work_dir = GetProjectTmpDir(project)

  if not cros_build_lib.IsInsideChroot():
    project_work_dir = os.path.join(
        constants.SOURCE_ROOT, constants.DEFAULT_CHROOT_DIR, project_work_dir)

  return project_work_dir


def GetProjectRepoDir(project, project_url, clean_old_dir=False):
  """Clone the project repo locally and return the repo directory.

  Args:
    project: git project name to clone.
    project_url: git project url to clone.
    clean_old_dir: Boolean to indicate whether to clean old work_dir. Default
      to False.

  Returns:
    project_dir: local project directory.
  """
  work_dir = GetProjectWorkDir(project)

  if clean_old_dir:
    # Delete the work_dir built by previous runs.
    osutils.RmDir(work_dir, ignore_missing=True, sudo=True)

  osutils.SafeMakedirs(work_dir)

  project_dir = os.path.join(work_dir, project)
  if not os.path.exists(project_dir):
    ref = os.path.join(constants.SOURCE_ROOT, project)
    logging.info('Cloning %s %s to %s', project_url, ref, project_dir)
    repository.CloneWorkingRepo(
        dest=project_dir, url=project_url, reference=ref)

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

  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, **kwargs):
    super(CheckTemplateStage, self).__init__(builder_run, buildstore, **kwargs)
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
      return [i[1] for i in milestone_path_pairs[0:3]]

  def _ListTemplates(self):
    """List and return template files from GS bucket.

    Returns:
      A list of template files.
    """
    template_gs_paths = []

    try:
      tot_gs_path = self.ctx.LS(GS_GE_TEMPLATE_TOT)
      if tot_gs_path:
        template_gs_paths.extend(tot_gs_path)
    except gs.GSNoSuchKey as e:
      logging.warning('No matching objects for %s: %s', GS_GE_TEMPLATE_TOT, e)

    try:
      release_gs_paths = self.SortAndGetReleasePaths(
          self.ctx.LS(GS_GE_TEMPLATE_RELEASE))
      if release_gs_paths:
        template_gs_paths.extend(release_gs_paths)
    except gs.GSNoSuchKey as e:
      logging.warning('No matching objects for %s: %s', GS_GE_TEMPLATE_RELEASE,
                      e)

    return template_gs_paths

  def PerformStage(self):
    template_gs_paths = self._ListTemplates()

    if not template_gs_paths:
      logging.info('No template files found. No need to update configs.')
      return

    chromite_dir = GetProjectRepoDir(
        'chromite', constants.CHROMITE_URL, clean_old_dir=True)
    successful = True
    failed_templates = []
    for template_gs_path in template_gs_paths:
      try:
        branch = GetBranchName(os.path.basename(template_gs_path))
        UpdateConfigStage(
            self._run,
            self.buildstore,
            template_gs_path,
            branch,
            chromite_dir,
            self._run.options.debug,
            suffix='_' + branch).Run()
      except Exception as e:
        successful = False
        failed_templates.append(template_gs_path)
        logging.error('Failed to update configs for %s: %s', template_gs_path,
                      e)
        traceback.print_exc()

    # If UpdateConfigStage failures happened, raise a exception
    if not successful:
      raise UpdateConfigException(
          'Failed to update config for %s' % failed_templates)


class UpdateConfigStage(generic_stages.BuilderStage):
  """Stage that verifies and updates configs.

  This stage gets the template file from GE bucket,
  checkout the corresponding branch, generates configs
  based on the new template file, verifies the changes,
  and submits the changes to the corresponding branch.
  """

  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, template_gs_path, branch,
               chromite_dir, dry_run, **kwargs):
    super(UpdateConfigStage, self).__init__(builder_run, buildstore, **kwargs)
    self.template_gs_path = template_gs_path
    self.chromite_dir = chromite_dir
    self.branch = branch

    self.ctx = gs.GSContext(init_boto=True)
    self.dry_run = dry_run

    # Filled in by _SetupConfigPaths, will cause errors if not filled in.
    self.config_dir = None
    self.config_paths = None
    self.ge_config_local_path = None

  def _CheckoutBranch(self):
    """Checkout to the corresponding branch in the temp repository.

    Raises:
      BranchNotFoundException if failed to checkout to the branch.
    """
    logging.info('Checking out %s in %s', self.branch, self.chromite_dir)
    git.RunGit(self.chromite_dir, ['checkout', self.branch])

    output = git.RunGit(self.chromite_dir,
                        ['rev-parse', '--abbrev-ref', 'HEAD']).output
    current_branch = output.rstrip()

    if current_branch != self.branch:
      raise BranchNotFoundException(
          "Failed to checkout to branch %s." % self.branch)

  def _SetupConfigPaths(self):
    """These config files can move based on the branch.

    Detect and save off the paths to them for the current path.
    """
    # These are the two directories inside cbuildbot where these files can
    # exist, and order of preference.
    dirs = ('config', 'cbuildbot')
    files = (GE_BUILD_CONFIG_FILE, 'config_dump.json',
             'waterfall_layout_dump.txt')

    for d in dirs:
      self.config_dir = d
      self.config_paths = [os.path.join(self.chromite_dir, d, f) for f in files]
      self.ge_config_local_path = self.config_paths[0]
      if os.path.exists(self.ge_config_local_path):
        logging.info('Found config in %s', self.config_dir)
        break
    else:
      raise ConfigNotFoundException(
          'Failed to find configs in branch %s.' % self.branch)

  def _DownloadTemplate(self):
    """Download the template file from gs."""
    self.ctx.Copy(self.template_gs_path, self.ge_config_local_path)

  def _ContainsConfigUpdates(self):
    """Check if updates exist and requires a push.

    Returns:
      True if updates exist; otherwise False.
    """
    modifications = git.RunGit(
        self.chromite_dir, ['status', '--porcelain', '--'] + self.config_paths,
        capture_output=True,
        print_cmd=True).output
    if modifications:
      logging.info('Changed files: %s ', modifications)
      return True
    else:
      return False

  def _RunUnitTest(self):
    """Run chromeos_config_unittest on top of the changes."""
    logging.debug("Running chromeos_config_unittest")
    test_path = path_util.ToChrootPath(
        os.path.join(self.chromite_dir, self.config_dir,
                     'chromeos_config_unittest'))

    # Because of --update, this updates our generated files.
    cmd = ['cros_sdk', '--', test_path, '--update']
    cros_build_lib.RunCommand(cmd, cwd=os.path.dirname(self.chromite_dir))

  def _CreateConfigPatch(self):
    """Create and return a diff patch file for config changes."""
    config_change_patch = os.path.join(self.chromite_dir, 'config_change.patch')
    try:
      os.remove(config_change_patch)
    except OSError as e:
      if e.errno != errno.ENOENT:
        raise

    result = git.RunGit(
        self.chromite_dir, ['diff'] + self.config_paths, print_cmd=True)
    with open(config_change_patch, 'w') as f:
      f.write(result.output)

    return config_change_patch

  def _RunBinhostTest(self):
    """Run BinhostTest stage for master branch."""
    config_change_patch = self._CreateConfigPatch()

    # Apply config patch.
    git.RunGit(
        constants.CHROMITE_DIR, ['apply', config_change_patch], print_cmd=True)

    test_stages.BinhostTestStage(
        self._run, self.buildstore, suffix='_' + self.branch).Run()

    # Clean config patch.
    git.RunGit(constants.CHROMITE_DIR, ['checkout', '.'], print_cmd=True)

  def _PushCommits(self):
    """Commit and push changes to current branch."""
    git.RunGit(self.chromite_dir, ['add'] + self.config_paths, print_cmd=True)
    commit_msg = "Update config settings by config-updater."
    git.RunGit(self.chromite_dir, ['commit', '-m', commit_msg], print_cmd=True)

    git.RunGit(
        self.chromite_dir, ['config', 'push.default', 'tracking'],
        print_cmd=True)
    git.PushBranch(self.branch, self.chromite_dir, dryrun=self.dry_run)

  def PerformStage(self):
    logging.info('Update configs for branch %s, template gs path %s',
                 self.branch, self.template_gs_path)
    try:
      self._CheckoutBranch()
      self._SetupConfigPaths()
      self._DownloadTemplate()
      self._RunUnitTest()
      if self._ContainsConfigUpdates():
        if self.branch == 'master':
          self._RunBinhostTest()
        self._PushCommits()
      else:
        logging.info('Nothing changed. No need to update configs for %s',
                     self.template_gs_path)
    finally:
      git.CleanAndDetachHead(self.chromite_dir)


class DeployLuciSchedulerStage(generic_stages.BuilderStage):
  """Stage that deploys updates to luci_scheduler.cfg.

  We autogenerate luci_scheduler.cfg, and submit that file into chromite
  for review purposes. However, it must be submitted into the LUCI Project
  config "chromeos" to be deployed. This stage autodeploys scheduler changes
  from chromite.
  """

  category = constants.CI_INFRA_STAGE

  PROJECT_URL = os.path.join(constants.INTERNAL_GOB_URL,
                             'chromeos/infra/config')
  PROJECT_BRANCH = 'master'

  def __init__(self, builder_run, buildstore, **kwargs):
    super(DeployLuciSchedulerStage, self).__init__(builder_run, buildstore,
                                                   **kwargs)
    self.legacy_project_dir = None
    self.project_dir = None

  def _RunUnitTest(self):
    """Run chromeos_config_unittest to confirm a clean scheduler config."""
    logging.debug("Running chromeos_config_unittest, to confirm sane state.")
    test_path = path_util.ToChrootPath(
        os.path.join(constants.CHROMITE_DIR, 'config',
                     'chromeos_config_unittest'))
    cmd = ['cros_sdk', '--', test_path]
    cros_build_lib.RunCommand(cmd, cwd=constants.CHROMITE_DIR)

  def _MakeWorkDir(self, name):
    """Makes and returns the path to a temporary directory.

    Args:
      name: name to use in the creation of the temporary directory.
    """
    path = GetProjectWorkDir(name)
    osutils.RmDir(path, ignore_missing=True, sudo=True)
    osutils.SafeMakedirs(path)
    return path

  def _CheckoutLuciProject(self):
    """Checkout the LUCI project config.

    Raises:
      BranchNotFoundException if failed to checkout to the branch.
    """
    self.project_dir = self._MakeWorkDir('luci_config')

    git.Clone(self.project_dir, self.PROJECT_URL, branch=self.PROJECT_BRANCH)

    logging.info('Checked out luci config %s:%s in %s',
                 self.PROJECT_URL, self.PROJECT_BRANCH, self.project_dir)

  def _UpdateLuciProject(self):
    chromite_source_file = os.path.join(constants.CHROMITE_DIR, 'config',
                                        'luci-scheduler.cfg')
    generated_source_file = os.path.join(self.project_dir, 'generated',
                                         'luci-scheduler.cfg')

    target_file = os.path.join(self.project_dir, 'luci', 'luci-scheduler.cfg')

    concatenated_content = (osutils.ReadFile(chromite_source_file) + "\n\n"
                            + osutils.ReadFile(generated_source_file))

    if concatenated_content == osutils.ReadFile(target_file):
      logging.PrintBuildbotStepText(
          'luci-scheduler.cfg current: No Update.')
      return

    chromite_rev = git.RunGit(
        constants.CHROMITE_DIR,
        ['rev-parse', 'HEAD:config/luci-scheduler.cfg']).output.rstrip()

    message = textwrap.dedent('''\
      luci-scheduler.cfg: Chromite %s

      Auto update to match generated file in chromite and luci config.
      ''' % chromite_rev)

    with open(target_file, "w") as f:
      f.write(concatenated_content)

    git.RunGit(self.project_dir, ['add', '-A'])
    git.RunGit(self.project_dir, ['commit', '-m', message])

    push_to = git.RemoteRef('origin', self.PROJECT_BRANCH)
    logging.info('Pushing to branch (%s) with message: %s %s', push_to, message,
                 ' (dryrun)' if self._run.options.debug else '')
    git.RunGit(
        self.project_dir, ['config', 'push.default', 'tracking'],
        print_cmd=True)
    git.PushBranch(self.PROJECT_BRANCH, self.project_dir,
                   dryrun=self._run.options.debug)
    logging.PrintBuildbotStepText('luci-scheduler.cfg: Updated.')

  def PerformStage(self):
    """Perform the DeployLuciSchedulerStage."""
    logging.info('Update luci_scheduler.cfg at %s:%s.',
                 self.PROJECT_URL, self.PROJECT_BRANCH)

    self._RunUnitTest()
    self._CheckoutLuciProject()
    self._UpdateLuciProject()
