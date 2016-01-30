# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import struct
import sys
from recipe_engine import recipe_test_api


class BotUpdateTestApi(recipe_test_api.RecipeTestApi):
  def output_json(self, root, first_sln, revision_mapping, git_mode,
                  force=False, fail_patch=False, output_manifest=False,
                  fixed_revisions=None):
    """Deterministically synthesize json.output test data for gclient's
    --output-json option.
    """
    active = True

    output = {
        'did_run': active,
        'patch_failure': False
    }

    # Add in extra json output if active.
    if active:
      properties = {
          property_name: self.gen_revision(project_name, git_mode)
          for project_name, property_name in revision_mapping.iteritems()
      }
      properties.update({
          '%s_cp' % property_name: ('refs/heads/master@{#%s}' %
                                    self.gen_revision(project_name, False))
          for project_name, property_name in revision_mapping.iteritems()
      })

      output.update({
          'patch_root': root or first_sln,
          'root': first_sln,
          'properties': properties,
          'step_text': 'Some step text'
      })

      if output_manifest:
        output.update({
          'manifest': {
            project_name: {
              'repository': 'https://fake.org/%s.git' % project_name,
              'revision': self.gen_revision(project_name, git_mode),
            }
            for project_name in revision_mapping
          }
        })

      if fixed_revisions:
        output['fixed_revisions'] = fixed_revisions

      if fail_patch:
        output['log_lines'] = [('patch error', 'Patch failed to apply'),]
        output['patch_failure'] = True
        output['patch_apply_return_code'] = 1
        if fail_patch == 'download':
          output['patch_apply_return_code'] = 3
    return self.m.json.output(output)

  @staticmethod
  def gen_revision(project, GIT_MODE):
    """Hash project to bogus deterministic revision values."""
    h = hashlib.sha1(project)
    if GIT_MODE:
      return h.hexdigest()
    else:
      return struct.unpack('!I', h.digest()[:4])[0] % 300000
