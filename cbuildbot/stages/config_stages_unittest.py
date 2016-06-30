# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for config stages"""

from __future__ import print_function

from chromite.cbuildbot.stages import config_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot import constants
from chromite.cbuildbot import repository
from chromite.lib import cros_build_lib
from chromite.lib import git

class CheckTemplateStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests for CheckTemplateStage."""

  def setUp(self):
    self._Prepare()
    self.PatchObject(repository, 'CloneWorkingRepo')
    self.update_mock = self.PatchObject(config_stages.UpdateConfigStage, 'Run')

  def ConstructStage(self):
    return config_stages.CheckTemplateStage(self._run)

  def testBasic(self):
    stage = self.ConstructStage()
    tot_path = (config_stages.GS_GE_TEMPLATE_BUCKET +
                'build_config.ToT.json')
    r50_path = (config_stages.GS_GE_TEMPLATE_BUCKET +
                'build_config.release-R50-7978.B.json')
    self.PatchObject(config_stages.CheckTemplateStage, '_ListTemplates',
                     return_value=[tot_path, r50_path])
    stage.PerformStage()
    self.assertTrue(self.update_mock.call_count == 2)

class UpdateConfigStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests for UpdateConfigStage."""

  def setUp(self):
    self._Prepare()
    self.PatchObject(config_stages.UpdateConfigStage, '_DownloadTemplate')
    self.PatchObject(config_stages.UpdateConfigStage, '_CheckoutBranch')
    self.PatchObject(config_stages.UpdateConfigStage, '_UpdateConfigDump')
    self.PatchObject(git, 'PushWithRetry')
    self.PatchObject(git, 'RunGit')
    self.PatchObject(repository, 'CloneWorkingRepo')
    self.PatchObject(cros_build_lib, 'RunCommand')
    self.chromite_dir = config_stages.CreateTmpGitRepo(
        'chromite', constants.CHROMITE_URL)

  def ConstructStage(self):
    template = 'build_config.ToT.json'
    tot_path = config_stages.GS_GE_TEMPLATE_BUCKET + template
    branch = config_stages.GetBranchName(template)
    return config_stages.UpdateConfigStage(
        self._run, tot_path, branch, self.chromite_dir)

  def testBasic(self):
    stage = self.ConstructStage()
    stage.PerformStage()
    self.assertTrue(stage.branch == 'master')

  def ConstructStageForRelease(self):
    template = 'build_config.release-R50-7978.B.json'
    r50_path = config_stages.GS_GE_TEMPLATE_BUCKET + template
    branch = config_stages.GetBranchName(template)
    return config_stages.UpdateConfigStage(
        self._run, r50_path, branch, self.chromite_dir)

  def testReleaseBasic(self):
    stage = self.ConstructStageForRelease()
    stage.PerformStage()
    self.assertTrue(stage.branch == 'release-R50-7978.B')
