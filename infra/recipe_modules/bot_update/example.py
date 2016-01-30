# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine.recipe_api import Property

DEPS = [
  'bot_update',
  'gclient',
  'recipe_engine/path',
  'recipe_engine/properties',
]

PROPERTIES = {
  'clobber': Property(default=False, kind=bool),
  'no_shallow': Property(default=False, kind=bool),
  'oauth2': Property(default=False, kind=bool),
  'output_manifest': Property(default=False, kind=bool),
  'patch': Property(default=True, kind=bool),
  'refs': Property(default=[]),
  'revision': Property(default=None),
  'revisions': Property(default={}),
  'root_solution_revision': Property(default=None),
  'suffix': Property(default=None),
  'with_branch_heads': Property(default=False, kind=bool),
}

def RunSteps(api, clobber, no_shallow, oauth2, output_manifest, patch, refs,
             revision, revisions, root_solution_revision, suffix,
             with_branch_heads):
  api.gclient.use_mirror = True

  src_cfg = api.gclient.make_config()
  soln = src_cfg.solutions.add()
  soln.name = 'src'
  soln.url = 'svn://svn.chromium.org/chrome/trunk/src'
  soln.revision = revision
  api.gclient.c = src_cfg
  api.gclient.c.revisions = revisions
  api.gclient.c.got_revision_mapping['src'] = 'got_cr_revision'

  api.bot_update.ensure_checkout(no_shallow=no_shallow,
                                 patch=patch,
                                 with_branch_heads=with_branch_heads,
                                 output_manifest=output_manifest,
                                 refs=refs, patch_oauth2=oauth2,
                                 clobber=clobber,
                                 root_solution_revision=root_solution_revision,
                                 suffix=suffix)


def GenTests(api):
  yield api.test('basic') + api.properties(
      patch=False,
      revision='abc'
  )
  yield api.test('basic_with_branch_heads') + api.properties(
      with_branch_heads=True,
      suffix='with branch heads'
  )
  yield api.test('basic_output_manifest') + api.properties(
      output_manifest=True,
  )
  yield api.test('tryjob') + api.properties(
      issue=12345,
      patchset=654321,
      patch_url='http://src.chromium.org/foo/bar'
  )
  yield api.test('trychange') + api.properties(
      refs=['+refs/change/1/2/333'],
  )
  yield api.test('trychange_oauth2') + api.properties(
      oauth2=True,
  )
  yield api.test('tryjob_fail') + api.properties(
      issue=12345,
      patchset=654321,
      patch_url='http://src.chromium.org/foo/bar',
  ) + api.step_data('bot_update', retcode=1)
  yield api.test('tryjob_fail_patch') + api.properties(
      issue=12345,
      patchset=654321,
      patch_url='http://src.chromium.org/foo/bar',
      fail_patch='apply',
  ) + api.step_data('bot_update', retcode=88)
  yield api.test('tryjob_fail_patch_download') + api.properties(
      issue=12345,
      patchset=654321,
      patch_url='http://src.chromium.org/foo/bar',
      fail_patch='download'
  ) + api.step_data('bot_update', retcode=87)
  yield api.test('no_shallow') + api.properties(
      no_shallow=True
  )
  yield api.test('clobber') + api.properties(
      clobber=True
  )
  yield api.test('reset_root_solution_revision') + api.properties(
      root_solution_revision='revision',
  )
  yield api.test('tryjob_v8') + api.properties(
      issue=12345,
      patchset=654321,
      patch_url='http://src.chromium.org/foo/bar',
      patch_project='v8',
      revisions={'src/v8': 'abc'}
  )
