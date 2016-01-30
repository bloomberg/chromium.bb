# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'recipe_engine/step',
  'hacky_tryserver_detection',
]


def RunSteps(api):
  api.step(
      "am i a tryserver?", ['echo', api.hacky_tryserver_detection.is_tryserver])


def GenTests(api):
  yield api.test('basic')
