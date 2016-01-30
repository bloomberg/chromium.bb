# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

PATCH_STORAGE_GIT = 'git'

class HackyTryserverDetectionApi(recipe_api.RecipeApi):
  @property
  def patch_url(self):
    """Reads patch_url property and corrects it if needed."""
    url = self.m.properties.get('patch_url')
    return url

  @property
  def is_tryserver(self):
    """Returns true iff we can apply_issue or patch."""
    return (self.can_apply_issue or self.is_patch_in_svn or
            self.is_patch_in_git or self.is_gerrit_issue)

  @property
  def can_apply_issue(self):
    """Returns true iff the properties exist to apply_issue from rietveld."""
    return (self.m.properties.get('rietveld')
            and 'issue' in self.m.properties
            and 'patchset' in self.m.properties)

  @property
  def is_gerrit_issue(self):
    """Returns true iff the properties exist to match a Gerrit issue."""
    return ('event.patchSet.ref' in self.m.properties and
            'event.change.url' in self.m.properties and
            'event.change.id' in self.m.properties)

  @property
  def is_patch_in_svn(self):
    """Returns true iff the properties exist to patch from a patch URL."""
    return self.patch_url

  @property
  def is_patch_in_git(self):
    return (self.m.properties.get('patch_storage') == PATCH_STORAGE_GIT and
            self.m.properties.get('patch_repo_url') and
            self.m.properties.get('patch_ref'))

