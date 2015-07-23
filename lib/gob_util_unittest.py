# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for gob_util.py"""

from __future__ import print_function

import httplib
import mock
import tempfile

from chromite.cbuildbot import config_lib
from chromite.lib import cros_test_lib
from chromite.lib import gob_util


site_config = config_lib.GetConfig()


class FakeHTTPResponse(object):
  """Enough of a HTTPResponse for FetchUrl.

  See https://docs.python.org/2/library/httplib.html#httpresponse-objects
  for more details.
  """

  def __init__(self, body='', headers=(), reason=None, status=200, version=11):
    if reason is None:
      reason = httplib.responses[status]

    self.body = body
    self.headers = dict(headers)
    self.msg = None
    self.reason = reason
    self.status = status
    self.version = version

  def read(self):
    return self.body

  def getheader(self, name, default=None):
    return self.headers.get(name, default)

  def getheaders(self):
    return tuple(self.headers.items())


class FakeHTTPConnection(object):
  """Enough of a HTTPConnection result for FetchUrl."""

  def __init__(self, req_url='/', req_method='GET', req_headers=None,
               req_body=None, **kwargs):
    self.kwargs = kwargs.copy()
    self.req_params = {
        'url': req_url,
        'method': req_method,
        'headers': req_headers,
        'body': req_body,
    }

  def getresponse(self):
    return FakeHTTPResponse(**self.kwargs)


class GobTest(cros_test_lib.MockTestCase):
  """Unittests that use mocks."""

  def testUtf8Response(self):
    """Handle gerrit responses w/UTF8 in them."""
    utf8_data = 'That\xe2\x80\x99s an error. That\xe2\x80\x99s all we know.'
    with mock.patch.object(gob_util, 'CreateHttpConn', autospec=False) as m:
      m.return_value = FakeHTTPConnection(body=utf8_data)
      gob_util.FetchUrl('', '')

      m.return_value = FakeHTTPConnection(body=utf8_data, status=502)
      self.assertRaises(gob_util.InternalGOBError, gob_util.FetchUrl, '', '')


class GetCookieTests(cros_test_lib.TestCase):
  """Unittests for GetCookies()"""

  def testSimple(self):
    f = tempfile.NamedTemporaryFile()
    f.write('.googlesource.com\tTRUE\t/f\tTRUE\t2147483647\to\tfoo=bar')
    f.flush()
    cookies = gob_util.GetCookies('foo.googlesource.com', '/foo', [f.name])
    self.assertEqual(cookies, {'o': 'foo=bar'})
    cookies = gob_util.GetCookies('google.com', '/foo', [f.name])
    self.assertEqual(cookies, {})
    cookies = gob_util.GetCookies('foo.googlesource.com', '/', [f.name])
    self.assertEqual(cookies, {})


@cros_test_lib.NetworkTest()
class NetworkGobTest(cros_test_lib.TestCase):
  """Unittests that talk to real Gerrit."""

  def test200(self):
    """Test successful loading of change."""
    gob_util.FetchUrlJson(site_config.params.EXTERNAL_GOB_HOST,
                          'changes/227254/detail')

  def test404(self):
    gob_util.FetchUrlJson(site_config.params.EXTERNAL_GOB_HOST, 'foo/bar/baz')

  def test404Exception(self):
    with self.assertRaises(gob_util.GOBError) as ex:
      gob_util.FetchUrlJson(site_config.params.EXTERNAL_GOB_HOST, 'foo/bar/baz',
                            ignore_404=False)
    self.assertEqual(ex.exception.http_status, 404)


def main(_argv):
  gob_util.TRY_LIMIT = 1
  cros_test_lib.main(module=__name__)
