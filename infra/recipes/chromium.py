# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'build/chromium',
  'build/chromium_swarming',
  'build/chromium_tests',
  'build/test_utils',
  'recipe_engine/properties',
  'recipe_engine/step',
]


from recipe_engine import config_types


def RunSteps(api):
  api.chromium_tests.main_waterfall_steps(api)


def GenTests(api):
  yield (
    api.test('builder') +
    api.properties.generic(
        mastername='chromium.fyi',
        buildername='Linux remote_run Builder',
        path_config='kitchen')
  )

  yield (
    api.test('tester') +
    api.properties.generic(
        mastername='chromium.fyi',
        buildername='Linux remote_run Tester',
        parent_buildername='Linux remote_run Builder',
        path_config='kitchen')
  )
