# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mimetypes
import os

from compiled_file_system import SingleFile
from directory_zipper import DirectoryZipper
from docs_server_utils import ToUnicode
from future import Gettable, Future
from third_party.handlebar import Handlebar


class ContentAndType(object):
  '''Return value from ContentProvider.GetContentAndType.
  '''

  def __init__(self, content, content_type):
    self.content = content
    self.content_type = content_type


class ContentProvider(object):
  '''Returns file contents correctly typed for their content-types (in the HTTP
  sense). Content-type is determined from Python's mimetype library which
  guesses based on the file extension.

  Typically the file contents will be either str (for binary content) or
  unicode (for text content). However, HTML files *may* be returned as
  Handlebar templates (if supports_templates is True on construction), in which
  case the caller will presumably want to Render them.
  '''

  def __init__(self,
               name,
               compiled_fs_factory,
               file_system,
               supports_templates=False,
               supports_zip=False):
    # Public.
    self.name = name
    self.file_system = file_system
    # Private.
    self._content_cache = compiled_fs_factory.Create(file_system,
                                                     self._CompileContent,
                                                     ContentProvider)
    self._supports_templates = supports_templates
    if supports_zip:
      self._directory_zipper = DirectoryZipper(compiled_fs_factory, file_system)
    else:
      self._directory_zipper = None

  @SingleFile
  def _CompileContent(self, path, text):
    assert text is not None, path
    mimetype = mimetypes.guess_type(path)[0]
    if mimetype is None:
      content = text
      mimetype = 'text/plain'
    elif mimetype == 'text/html':
      content = ToUnicode(text)
      if self._supports_templates:
        content = Handlebar(content, name=path)
    elif (mimetype.startswith('text/') or
          mimetype in ('application/javascript', 'application/json')):
      content = ToUnicode(text)
    else:
      content = text
    return ContentAndType(content, mimetype)

  def GetContentAndType(self, path):
    path = path.lstrip('/')
    base, ext = os.path.splitext(path)

    # Check for a zip file first, if zip is enabled.
    if self._directory_zipper and ext == '.zip':
      zip_future = self._directory_zipper.Zip(base)
      return Future(delegate=Gettable(
          lambda: ContentAndType(zip_future.Get(), 'application/zip')))

    return self._content_cache.GetFromFile(path)

  def Cron(self):
    # Running Refresh() on the file system is enough to pull GitHub content,
    # which is all we need for now while the full render-every-page cron step
    # is in effect.
    # TODO(kalman): Walk over the whole filesystem and compile the content.
    return self.file_system.Refresh()

  def __repr__(self):
    return 'ContentProvider of <%s>' % repr(self.file_system)
