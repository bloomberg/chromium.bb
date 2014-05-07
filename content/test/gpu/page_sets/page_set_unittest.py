# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import discover
from telemetry.page import page_set as page_set_module
from telemetry.page import page_set_archive_info
from telemetry.test_util import page_set_smoke_test


class PageSetUnitTest(page_set_smoke_test.PageSetSmokeTest):

  def testSmoke(self):
    self.RunSmokeTest(os.path.dirname(__file__))
