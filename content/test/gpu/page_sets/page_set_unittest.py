# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.testing import story_set_smoke_test


class StorySetUnitTest(story_set_smoke_test.StorySetSmokeTest):

  def testSmoke(self):
    story_sets_dir = os.path.dirname(os.path.realpath(__file__))
    top_level_dir = os.path.dirname(story_sets_dir)
    self.RunSmokeTest(story_sets_dir, top_level_dir)
