# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class Request(object):
  '''Request data.
  '''
  def __init__(self, path, headers):
    self.path = path
    self.headers = headers

class _ContentBuilder(object):
  '''Builds the response content.
  '''
  def __init__(self):
    self._buf = []

  def Append(self, content):
    if isinstance(content, unicode):
      content = content.encode('utf-8', 'replace')
    self._buf.append(content)

  def ToString(self):
    self._Collapse()
    return self._buf[0]

  def __str__(self):
    return self.ToString()

  def __len__(self):
    return len(self.ToString())

  def _Collapse(self):
    self._buf = [''.join(self._buf)]

class Response(object):
  '''The response from Get().
  '''
  def __init__(self, content=None, headers=None, status=None):
    self.content = _ContentBuilder()
    if content is not None:
      self.content.Append(content)
    self.headers = {}
    if headers is not None:
      self.headers.update(headers)
    self.status = status

  @staticmethod
  def Ok(content, headers=None):
    '''Returns an OK (200) response.
    '''
    return Response(content=content, headers=headers, status=200)

  @staticmethod
  def Redirect(url, permanent=False):
    '''Returns a redirect (301 or 302) response.
    '''
    if not url.startswith('/'):
      url = '/%s' % url
    status = 301 if permanent else 302
    return Response(headers={'Location': url}, status=status)

  @staticmethod
  def NotFound(content, headers=None):
    '''Returns a not found (404) response.
    '''
    return Response(content=content, headers=headers, status=404)

  @staticmethod
  def InternalError(content, headers=None):
    '''Returns an internal error (500) response.
    '''
    return Response(content=content, headers=headers, status=500)

  def Append(self, content):
    '''Appends |content| to the response content.
    '''
    self.content.append(content)

  def AddHeader(self, key, value):
    '''Adds a header to the response.
    '''
    self.headers[key] = value

  def AddHeaders(self, headers):
    '''Adds several headers to the response.
    '''
    self.headers.update(headers)

  def SetStatus(self, status):
    self.status = status

  def __repr__(self):
    return '{content: %s bytes, status: %s, headers: %s entries}' % (
        len(self.content), self.status, len(self.headers.keys()))

class Servlet(object):
  def __init__(self, request):
    self._request = request

  def Get(self):
    '''Returns a Response.
    '''
    raise NotImplemented()
