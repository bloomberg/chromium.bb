# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page_test

class PixelTestFailure(Exception):
  pass

class PixelTest(page_test.PageTest):
  def __init__(self):
    super(PixelTest, self).__init__('ValidatePage')

  def ValidatePage(self, page, tab, results):
    # TODO(bajones): Grab screenshot, compare to reference.
    # page.reference_image
    pass
