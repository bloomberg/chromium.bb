# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=unused-argument

from __future__ import print_function

import contextlib
import sys
import unittest

from google.protobuf import empty_pb2
import mock

from chromite.lib.luci import net
from chromite.lib.luci.prpc import client as prpc_client
from chromite.lib.luci.prpc import codes
from chromite.lib.luci.prpc.test import test_pb2, test_prpc_pb2
from chromite.lib.luci.test_support import test_case, test_env

test_env.setup_test_env()


class PRPCClientTestCase(test_case.TestCase):

  def make_test_client(self):
    return prpc_client.Client('localhost', test_prpc_pb2.TestServiceDescription)

  def test_generated_methods(self):
    expected_methods = {
        'Give',
        'Take',
        'Echo',
    }

    members = dir(self.make_test_client())
    for m in expected_methods:
      self.assertIn(m, members)

  @contextlib.contextmanager
  def mocked_request(self, res=None):
    res = res or empty_pb2.Empty()
    with mock.patch('chromite.lib.luci.net.request', autospec=True) as m:
      m.return_value = res.SerializeToString()
      yield

  def test_request(self):
    with self.mocked_request():
      req = test_pb2.GiveRequest(m=1)
      self.make_test_client().Give(req)

      net.request.assert_called_with(
          url='https://localhost/prpc/test.Test/Give',
          method='POST',
          payload=req.SerializeToString(),
          headers={
              'Content-Type': 'application/prpc; encoding=binary',
              'Accept': 'application/prpc; encoding=binary',
              'X-Prpc-Timeout': '10S',
          },
          include_auth=True,
          deadline=10,
          max_attempts=4,
      )

  def give_creds(self, creds):
    self.make_test_client().Give(test_pb2.GiveRequest(), credentials=creds)

  def test_request_credentials_service_account(self):
    with self.mocked_request():
      self.give_creds(prpc_client.include_auth())
      _, kwargs = net.request.call_args
      self.assertEqual(kwargs['include_auth'], True)

  def test_request_credentials_service_account_key(self):
    with self.mocked_request():
      self.give_creds(prpc_client.include_auth())
      _, kwargs = net.request.call_args
      self.assertEqual(kwargs['include_auth'], True)

  def test_request_timeout(self):
    with self.mocked_request():
      self.make_test_client().Give(test_pb2.GiveRequest(), timeout=20)
      _, kwargs = net.request.call_args
      self.assertEqual(kwargs['deadline'], 20)
      self.assertEqual(kwargs['headers']['X-Prpc-Timeout'], '20S')

  def test_response_ok(self):
    expected = test_pb2.TakeResponse(k=1)
    with self.mocked_request(res=expected):
      actual = self.make_test_client().Take(empty_pb2.Empty())
      self.assertEqual(actual, expected)

  @mock.patch('chromite.lib.luci.net.request', autospec=True)
  def test_response_protocol_error(self, request):
    request.side_effect = net.NotFoundError(
        msg='not found',
        status_code=404,
        response='not found',
        headers={
            # no X-Prpc-Grpc-Code header.
        },
    )
    with self.assertRaises(prpc_client.ProtocolError):
      self.make_test_client().Take(empty_pb2.Empty())

  @mock.patch('chromite.lib.luci.net.request', autospec=True)
  def test_response_rpc_error(self, request):
    request.side_effect = net.NotFoundError(
        msg='not found',
        status_code=404,
        response='not found',
        headers={
            'X-Prpc-Grpc-Code': str(codes.StatusCode.NOT_FOUND[0]),
        },
    )
    with self.assertRaises(prpc_client.RpcError) as cm:
      self.make_test_client().Take(empty_pb2.Empty())
    self.assertEqual(cm.exception.status_code, codes.StatusCode.NOT_FOUND)


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
