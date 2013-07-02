# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test
from telemetry.page import page_test


class PixelTestFailure(Exception):
  pass


class PixelValidator(page_test.PageTest):
  def __init__(self):
    super(PixelValidator, self).__init__('ValidatePage')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--enable-webgl')

  def ValidatePage(self, page, tab, results):
    # TODO(bajones): Grab screenshot, compare to reference.
    # page.reference_image
    pass


class Pixel(test.Test):
  enabled = False
  test = PixelValidator
  page_set = 'page_sets/pixel_tests.json'
