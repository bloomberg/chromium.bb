# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import types

from recipe_engine.config import config_item_context, ConfigGroup, BadConf
from recipe_engine.config import ConfigList, Dict, Single, Static, Set, List

from . import api as gclient_api


def BaseConfig(USE_MIRROR=True, GIT_MODE=False, CACHE_DIR=None,
               PATCH_PROJECT=None, BUILDSPEC_VERSION=None,
               **_kwargs):
  deps = '.DEPS.git' if GIT_MODE else 'DEPS'
  cache_dir = str(CACHE_DIR) if GIT_MODE and CACHE_DIR else None
  return ConfigGroup(
    solutions = ConfigList(
      lambda: ConfigGroup(
        name = Single(basestring),
        url = Single(basestring),
        deps_file = Single(basestring, empty_val=deps, required=False,
                           hidden=False),
        managed = Single(bool, empty_val=True, required=False, hidden=False),
        custom_deps = Dict(value_type=(basestring, types.NoneType)),
        custom_vars = Dict(value_type=basestring),
        safesync_url = Single(basestring, required=False),

        revision = Single(
            (basestring, gclient_api.RevisionResolver),
            required=False, hidden=True),
      )
    ),
    deps_os = Dict(value_type=basestring),
    hooks = List(basestring),
    target_os = Set(basestring),
    target_os_only = Single(bool, empty_val=False, required=False),
    cache_dir = Static(cache_dir, hidden=False),

    # If supplied, use this as the source root (instead of the first solution's
    # checkout).
    src_root = Single(basestring, required=False, hidden=True),

    # Maps 'solution' -> build_property
    got_revision_mapping = Dict(hidden=True),

    # Addition revisions we want to pass in.  For now theres a duplication
    # of code here of setting custom vars AND passing in --revision. We hope
    # to remove custom vars later.
    revisions = Dict(
        value_type=(basestring, gclient_api.RevisionResolver),
        hidden=True),

    # TODO(iannucci): HACK! The use of None here to indicate that we apply this
    #   to the solution.revision field is really terrible. I mostly blame
    #   gclient.
    # Maps 'parent_build_property' -> 'custom_var_name'
    # Maps 'parent_build_property' -> None
    # If value is None, the property value will be applied to
    # solutions[0].revision. Otherwise, it will be applied to
    # solutions[0].custom_vars['custom_var_name']
    parent_got_revision_mapping = Dict(hidden=True),
    delete_unversioned_trees = Single(bool, empty_val=True, required=False),

    # Check out refs/branch-heads.
    # TODO (machenbach): Only implemented for bot_update atm.
    with_branch_heads = Single(
        bool,
        empty_val=False,
        required=False,
        hidden=True),

    GIT_MODE = Static(bool(GIT_MODE)),
    USE_MIRROR = Static(bool(USE_MIRROR)),
    PATCH_PROJECT = Static(str(PATCH_PROJECT), hidden=True),
    BUILDSPEC_VERSION= Static(BUILDSPEC_VERSION, hidden=True),
  )

config_ctx = config_item_context(BaseConfig)
