# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from third_party.json_schema_compiler.model import UnixName
import svn_constants

class PathCanonicalizer(object):
  '''Transforms paths into their canonical forms. Since the dev server has had
  many incarnations - e.g. there didn't use to be apps/ - there may be old
  paths lying around the webs. We try to redirect those to where they are now.
  '''
  def __init__(self, channel, compiled_fs_factory):
    self._channel = channel
    self._identity_fs = compiled_fs_factory.CreateIdentity(PathCanonicalizer)

  def Canonicalize(self, path):
    starts_with_channel, path_without_channel = (False, path)
    if path.startswith('%s/' % self._channel):
      starts_with_channel, path_without_channel = (
          True, path[len(self._channel) + 1:])

    if any(path.startswith(prefix)
           for prefix in ('extensions/', 'apps/', 'static/')):
      return path

    if '/' in path_without_channel or path_without_channel == '404.html':
      return path

    apps_templates = self._identity_fs.GetFromFileListing(
        '/'.join((svn_constants.PUBLIC_TEMPLATE_PATH, 'apps')))
    extensions_templates = self._identity_fs.GetFromFileListing(
        '/'.join((svn_constants.PUBLIC_TEMPLATE_PATH, 'extensions')))

    if self._channel is None or not starts_with_channel:
      apps_path = 'apps/%s' % path_without_channel
      extensions_path = 'extensions/%s' % path_without_channel
    else:
      apps_path = '%s/apps/%s' % (self._channel, path_without_channel)
      extensions_path = '%s/extensions/%s' % (self._channel,
                                              path_without_channel)

    unix_path = UnixName(os.path.splitext(path_without_channel)[0])
    if unix_path in extensions_templates:
      return extensions_path
    if unix_path in apps_templates:
      return apps_path
    return extensions_path
