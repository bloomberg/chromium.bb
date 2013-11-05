# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from operator import itemgetter
import posixpath

from chroot_file_system import ChrootFileSystem
from content_provider import ContentProvider
from svn_constants import JSON_PATH
from third_party.json_schema_compiler.memoize import memoize


_CONFIG_PATH = '%s/content_providers.json' % JSON_PATH


class ContentProviders(object):
  '''Implements the content_providers.json configuration; see
  chrome/common/extensions/docs/templates/json/content_providers.json for its
  current state and a description of the format.

  Returns ContentProvider instances based on how they're configured there.
  '''

  def __init__(self, compiled_fs_factory, host_file_system):
    self._compiled_fs_factory = compiled_fs_factory
    self._host_file_system = host_file_system
    self._cache = compiled_fs_factory.ForJson(host_file_system)

  @memoize
  def GetByName(self, name):
    '''Gets the ContentProvider keyed by |name| in content_providers.json, or
    None of there is no such content provider.
    '''
    config = self._GetConfig().get(name)
    if config is None:
      logging.error('No content provider found with name "%s"' % name)
      return None
    return self._CreateContentProvider(name, config)

  @memoize
  def GetByServeFrom(self, path):
    '''Gets a (content_provider, path_in_content_provider) tuple, where
    content_provider is the ContentProvider with the longest "serveFrom"
    property that is a subpath of |path|, and path_in_content_provider is the
    remainder of |path|.

    For example, if content provider A serves from "foo" and content provider B
    serves from "foo/bar", GetByServeFrom("foo/bar/baz") will return (B, "baz").

    Returns (None, |path|) if no ContentProvider serves from |path|.
    '''
    serve_from_to_config = dict(
        (config['serveFrom'], (name, config))
        for name, config in self._GetConfig().iteritems())
    path_parts = path.split('/')
    for i in xrange(len(path_parts), -1, -1):
      name_and_config = serve_from_to_config.get('/'.join(path_parts[:i]))
      if name_and_config is not None:
        return (self._CreateContentProvider(name_and_config[0],
                                            name_and_config[1]),
                '/'.join(path_parts[i:]))
    return None, path

  def _GetConfig(self):
    return self._cache.GetFromFile(_CONFIG_PATH).Get()

  def _CreateContentProvider(self, name, config):
    supports_templates = config.get('supportsTemplates', False)
    supports_zip = config.get('supportsZip', False)

    if 'chromium' in config:
      chromium_config = config['chromium']
      if 'dir' not in chromium_config:
        logging.error('"chromium" must have a "dir" property')
        return None
      file_system = ChrootFileSystem(self._host_file_system,
                                     chromium_config['dir'])
    else:
      logging.error('Content provider type "%s" not supported', type_)
      return None

    return ContentProvider(name,
                           self._compiled_fs_factory,
                           file_system,
                           supports_templates=supports_templates,
                           supports_zip=supports_zip)

  def Cron(self):
    for name, config in self._GetConfig().iteritems():
      self._CreateContentProvider(name, config).Cron()
