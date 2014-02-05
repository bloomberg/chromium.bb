# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mimetypes
import posixpath

from compiled_file_system import SingleFile
from directory_zipper import DirectoryZipper
from docs_server_utils import ToUnicode
from file_system import FileNotFoundError
from future import Gettable, Future
from third_party.handlebar import Handlebar
from third_party.markdown import markdown


_MIMETYPE_OVERRIDES = {
  # SVG is not supported by mimetypes.guess_type on AppEngine.
  '.svg': 'image/svg+xml',
}


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
    _, ext = posixpath.splitext(path)
    mimetype = _MIMETYPE_OVERRIDES.get(ext, mimetypes.guess_type(path)[0])
    if ext == '.md':
      # See http://pythonhosted.org/Markdown/extensions
      # for details on "extensions=".
      content = markdown(ToUnicode(text),
                         extensions=('extra', 'headerid', 'sane_lists'))
      if self._supports_templates:
        content = Handlebar(content, name=path)
      mimetype = 'text/html'
    elif mimetype is None:
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

  def _MaybeMarkdown(self, path):
    base, ext = posixpath.splitext(path)
    if ext != '.html':
      return path
    if self.file_system.Exists(path).Get():
      return path
    as_md = base + '.md'
    if self.file_system.Exists(as_md).Get():
      return as_md
    return path

  def GetContentAndType(self, path):
    path = path.lstrip('/')
    base, ext = posixpath.splitext(path)

    # Check for a zip file first, if zip is enabled.
    if self._directory_zipper and ext == '.zip':
      zip_future = self._directory_zipper.Zip(base)
      return Future(delegate=Gettable(
          lambda: ContentAndType(zip_future.Get(), 'application/zip')))

    return self._content_cache.GetFromFile(self._MaybeMarkdown(path))

  def Cron(self):
    # Running Refresh() on the file system is enough to pull GitHub content,
    # which is all we need for now while the full render-every-page cron step
    # is in effect.
    futures = []
    for root, _, files in self.file_system.Walk(''):
      futures += [self.GetContentAndType(posixpath.join(root, filename))
                  for filename in files]
    return Future(delegate=Gettable(lambda: [f.Get() for f in futures]))

  def __repr__(self):
    return 'ContentProvider of <%s>' % repr(self.file_system)
