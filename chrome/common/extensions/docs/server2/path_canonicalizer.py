# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import posixpath
import traceback

from branch_utility import BranchUtility
from file_system import FileNotFoundError
from third_party.json_schema_compiler.model import UnixName
import svn_constants

def _SimplifyFileName(file_name):
  return (posixpath.splitext(file_name)[0]
      .lower()
      .replace('.', '')
      .replace('-', '')
      .replace('_', ''))

class PathCanonicalizer(object):
  '''Transforms paths into their canonical forms. Since the dev server has had
  many incarnations - e.g. there didn't use to be apps/ - there may be old
  paths lying around the webs. We try to redirect those to where they are now.
  '''
  def __init__(self, compiled_fs_factory):
    # Map of simplified API names (for typo detection) to their real paths.
    def make_public_apis(_, file_names):
      return dict((_SimplifyFileName(name), name) for name in file_names)
    self._public_apis = compiled_fs_factory.Create(make_public_apis,
                                                   PathCanonicalizer)

  def Canonicalize(self, path):
    '''Returns the canonical path for |path|, and whether that path is a
    permanent canonicalisation (e.g. when we redirect from a channel to a
    channel-less URL) or temporary (e.g. when we redirect from an apps-only API
    to an extensions one - we may at some point enable it for extensions).
    '''
    class ReturnType(object):
      def __init__(self, path, permanent):
        self.path = path
        self.permanent = permanent

      # Catch incorrect comparisons by disabling ==/!=.
      def __eq__(self, _): raise NotImplementedError()
      def __ne__(self, _): raise NotImplementedError()

    # Strip any channel info off it. There are no channels anymore.
    for channel_name in BranchUtility.GetAllChannelNames():
      channel_prefix = channel_name + '/'
      if path.startswith(channel_prefix):
        # Redirect now so that we can set the permanent-redirect bit.  Channel
        # redirects are the only things that should be permanent redirects;
        # anything else *could* change, so is temporary.
        return ReturnType(path[len(channel_prefix):], True)

    # No further work needed for static.
    if path.startswith('static/'):
      return ReturnType(path, False)

    # People go to just "extensions" or "apps". Redirect to the directory.
    if path in ('extensions', 'apps'):
      return ReturnType(path + '/', False)

    # The rest of this function deals with trying to figure out what API page
    # for extensions/apps to redirect to, if any. We see a few different cases
    # here:
    #  - Unqualified names ("browserAction.html"). These are easy to resolve;
    #    figure out whether it's an extension or app API and redirect.
    #     - but what if it's both? Well, assume extensions. Maybe later we can
    #       check analytics and see which is more popular.
    #  - Wrong names ("apps/browserAction.html"). This really does happen,
    #    damn it, so do the above logic but record which is the default.
    if path.startswith(('extensions/', 'apps/')):
      default_platform, reference_path = path.split('/', 1)
    else:
      default_platform, reference_path = ('extensions', path)

    try:
      apps_public = self._public_apis.GetFromFileListing(
          '/'.join((svn_constants.PUBLIC_TEMPLATE_PATH, 'apps')))
      extensions_public = self._public_apis.GetFromFileListing(
          '/'.join((svn_constants.PUBLIC_TEMPLATE_PATH, 'extensions')))
    except FileNotFoundError:
      # Probably offline.
      logging.warning(traceback.format_exc())
      return ReturnType(path, False)

    simple_reference_path = _SimplifyFileName(reference_path)
    apps_path = apps_public.get(simple_reference_path)
    extensions_path = extensions_public.get(simple_reference_path)

    if apps_path is None:
      if extensions_path is None:
        # No idea. Just return the original path. It'll probably 404.
        pass
      else:
        path = 'extensions/%s' % extensions_path
    else:
      if extensions_path is None:
        path = 'apps/%s' % apps_path
      else:
        assert apps_path == extensions_path
        path = '%s/%s' % (default_platform, apps_path)

    return ReturnType(path, False)
