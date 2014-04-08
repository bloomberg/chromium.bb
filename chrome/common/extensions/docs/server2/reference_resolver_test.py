#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from future import Future
from reference_resolver import ReferenceResolver
from test_object_store import TestObjectStore
from test_util import Server2Path
from third_party.json_schema_compiler.model import Namespace


_TEST_DATA = {
  'baz': {
    'namespace': 'baz',
    'description': '',
    'types': [
      {
        'id': 'baz_t1',
        'type': 'any',
      },
      {
        'id': 'baz_t2',
        'type': 'any',
      },
      {
        'id': 'baz_t3',
        'type': 'any',
      }
    ],
    'functions': [
      {
        'name': 'baz_f1',
        'type': 'function'
      },
      {
        'name': 'baz_f2',
        'type': 'function'
      },
      {
        'name': 'baz_f3',
        'type': 'function'
      }
    ],
    'events': [
      {
        'name': 'baz_e1',
        'type': 'function'
      },
      {
        'name': 'baz_e2',
        'type': 'function'
      },
      {
        'name': 'baz_e3',
        'type': 'function'
      }
    ],
    'properties': {
      'baz_p1': {'type': 'any'},
      'baz_p2': {'type': 'any'},
      'baz_p3': {'type': 'any'}
    }
  },
  'bar.bon': {
    'namespace': 'bar.bon',
    'description': '',
    'types': [
      {
        'id': 'bar_bon_t1',
        'type': 'any',
      },
      {
        'id': 'bar_bon_t2',
        'type': 'any',
      },
      {
        'id': 'bar_bon_t3',
        'type': 'any',
      }
    ],
    'functions': [
      {
        'name': 'bar_bon_f1',
        'type': 'function'
      },
      {
        'name': 'bar_bon_f2',
        'type': 'function'
      },
      {
        'name': 'bar_bon_f3',
        'type': 'function'
      }
    ],
    'events': [
      {
        'name': 'bar_bon_e1',
        'type': 'function'
      },
      {
        'name': 'bar_bon_e2',
        'type': 'function'
      },
      {
        'name': 'bar_bon_e3',
        'type': 'function'
      }
    ],
    'properties': {
      'bar_bon_p1': {'type': 'any'},
      'bar_bon_p2': {'type': 'any'},
      'bar_bon_p3': {'type': 'any'}
    }
  },
  'bar': {
    'namespace': 'bar',
    'description': '',
    'types': [
      {
        'id': 'bar_t1',
        'type': 'any',
        'properties': {
          'bar_t1_p1': {
            'type': 'any'
          }
        }
      },
      {
        'id': 'bar_t2',
        'type': 'any',
        'properties': {
          'bar_t2_p1': {
            'type': 'any'
          }
        }
      },
      {
        'id': 'bar_t3',
        'type': 'any',
      },
      {
        'id': 'bon',
        'type': 'any'
      }
    ],
    'functions': [
      {
        'name': 'bar_f1',
        'type': 'function'
      },
      {
        'name': 'bar_f2',
        'type': 'function'
      },
      {
        'name': 'bar_f3',
        'type': 'function'
      }
    ],
    'events': [
      {
        'name': 'bar_e1',
        'type': 'function'
      },
      {
        'name': 'bar_e2',
        'type': 'function'
      },
      {
        'name': 'bar_e3',
        'type': 'function'
      }
    ],
    'properties': {
      'bar_p1': {'type': 'any'},
      'bar_p2': {'type': 'any'},
      'bar_p3': {'$ref': 'bar_t1'}
    }
  },
  'foo': {
    'namespace': 'foo',
    'description': '',
    'types': [
      {
        'id': 'foo_t1',
        'type': 'any',
      },
      {
        'id': 'foo_t2',
        'type': 'any',
      },
      {
        'id': 'foo_t3',
        'type': 'any',
        'events': [
          {
            'name': 'foo_t3_e1',
            'type': 'function'
          }
        ]
      }
    ],
    'functions': [
      {
        'name': 'foo_f1',
        'type': 'function'
      },
      {
        'name': 'foo_f2',
        'type': 'function'
      },
      {
        'name': 'foo_f3',
        'type': 'function'
      }
    ],
    'events': [
      {
        'name': 'foo_e1',
        'type': 'function'
      },
      {
        'name': 'foo_e2',
        'type': 'function'
      },
      {
        'name': 'foo_e3',
        'type': 'function'
      }
    ],
    'properties': {
      'foo_p1': {'$ref': 'foo_t3'},
      'foo_p2': {'type': 'any'},
      'foo_p3': {'type': 'any'}
    }
  }
}


class _FakeAPIModels(object):
  def __init__(self, apis):
    self._apis = apis

  def GetNames(self):
    return self._apis.keys()

  def GetModel(self, name):
    return Future(value=Namespace(self._apis[name], 'fake/path.json'))


class ReferenceResolverTest(unittest.TestCase):
  def setUp(self):
    self._base_path = Server2Path('test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def testGetLink(self):
    resolver = ReferenceResolver(_FakeAPIModels(_TEST_DATA),
                                 TestObjectStore('test'))
    self.assertEqual({
      'href': 'foo',
      'text': 'foo',
      'name': 'foo'
    }, resolver.GetLink('foo', namespace='baz'))
    self.assertEqual({
      'href': 'foo#type-foo_t1',
      'text': 'foo.foo_t1',
      'name': 'foo_t1'
    }, resolver.GetLink('foo.foo_t1', namespace='baz'))
    self.assertEqual({
      'href': 'baz#event-baz_e1',
      'text': 'baz_e1',
      'name': 'baz_e1'
    }, resolver.GetLink('baz.baz_e1', namespace='baz'))
    self.assertEqual({
      'href': 'baz#event-baz_e1',
      'text': 'baz_e1',
      'name': 'baz_e1'
    }, resolver.GetLink('baz_e1', namespace='baz'))
    self.assertEqual({
      'href': 'foo#method-foo_f1',
      'text': 'foo.foo_f1',
      'name': 'foo_f1'
    }, resolver.GetLink('foo.foo_f1', namespace='baz'))
    self.assertEqual({
      'href': 'foo#property-foo_p3',
      'text': 'foo.foo_p3',
      'name': 'foo_p3'
    }, resolver.GetLink('foo.foo_p3', namespace='baz'))
    self.assertEqual({
      'href': 'bar.bon#type-bar_bon_t3',
      'text': 'bar.bon.bar_bon_t3',
      'name': 'bar_bon_t3'
    }, resolver.GetLink('bar.bon.bar_bon_t3', namespace='baz'))
    self.assertEqual({
      'href': 'bar.bon#property-bar_bon_p3',
      'text': 'bar_bon_p3',
      'name': 'bar_bon_p3'
    }, resolver.GetLink('bar_bon_p3', namespace='bar.bon'))
    self.assertEqual({
      'href': 'bar.bon#property-bar_bon_p3',
      'text': 'bar_bon_p3',
      'name': 'bar_bon_p3'
    }, resolver.GetLink('bar.bon.bar_bon_p3', namespace='bar.bon'))
    self.assertEqual({
      'href': 'bar#event-bar_e2',
      'text': 'bar_e2',
      'name': 'bar_e2'
    }, resolver.GetLink('bar.bar_e2', namespace='bar'))
    self.assertEqual({
      'href': 'bar#type-bon',
      'text': 'bon',
      'name': 'bon'
    }, resolver.GetLink('bar.bon', namespace='bar'))
    self.assertEqual({
      'href': 'foo#event-foo_t3-foo_t3_e1',
      'text': 'foo_t3.foo_t3_e1',
      'name': 'foo_t3_e1'
    }, resolver.GetLink('foo_t3.foo_t3_e1', namespace='foo'))
    self.assertEqual({
      'href': 'foo#event-foo_t3-foo_t3_e1',
      'text': 'foo_t3.foo_t3_e1',
      'name': 'foo_t3_e1'
    }, resolver.GetLink('foo.foo_t3.foo_t3_e1', namespace='foo'))
    self.assertEqual({
      'href': 'foo#event-foo_t3-foo_t3_e1',
      'text': 'foo_t3.foo_t3_e1',
      'name': 'foo_t3_e1'
    }, resolver.GetLink('foo.foo_p1.foo_t3_e1', namespace='foo'))
    self.assertEqual({
      'href': 'bar#property-bar_t1-bar_t1_p1',
      'text': 'bar.bar_t1.bar_t1_p1',
      'name': 'bar_t1_p1'
    }, resolver.GetLink('bar.bar_p3.bar_t1_p1', namespace='foo'))
    self.assertEqual({
      'href': 'bar#property-bar_t1-bar_t1_p1',
      'text': 'bar_t1.bar_t1_p1',
      'name': 'bar_t1_p1'
    }, resolver.GetLink('bar_p3.bar_t1_p1', namespace='bar'))
    self.assertEqual(
        None,
        resolver.GetLink('bar.bar_p3.bar_t2_p1', namespace='bar'))
    self.assertEqual(
        None,
        resolver.GetLink('bar.bon.bar_e3', namespace='bar'))
    self.assertEqual(
        None,
        resolver.GetLink('bar_p3', namespace='baz.bon'))
    self.assertEqual(
        None,
        resolver.GetLink('falafel.faf', namespace='a'))
    self.assertEqual(
        None,
        resolver.GetLink('bar_p3', namespace='foo'))
    self.assertEqual(
        'Hello <a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="bar.bon#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hello $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            namespace='bar.bon'))
    self.assertEqual(
        'I like <a href="bar#property-bar_t1-bar_t1_p1">food</a>.',
        resolver.ResolveAllLinks('I like $ref:[bar.bar_p3.bar_t1_p1 food].',
                                 namespace='foo'))
    self.assertEqual(
        'Ref <a href="foo">It\'s foo!</a>',
        resolver.ResolveAllLinks('Ref $ref:[foo It\'s foo!]', namespace='bar'))
    self.assertEqual(
        'Ref <a href="bar#type-bon">Bon</a>',
        resolver.ResolveAllLinks('Ref $ref:[bar.bon Bon]', namespace='bar'))

    # Different kinds of whitespace can be significant inside <pre> tags.
    self.assertEqual(
        '<pre><a href="bar#type-bon">bar.bon</a>({\nkey: value})',
        resolver.ResolveAllLinks('<pre>$ref:[bar.bon]({\nkey: value})',
                                 namespace='baz'))

    # Allow bare "$ref:foo.bar." at the end of a string.
    self.assertEqual(
        '<a href="bar#type-bon">bar.bon</a>.',
        resolver.ResolveAllLinks('$ref:bar.bon.',
                                 namespace='baz'))

    # If a request is provided it should construct an appropriate relative link.
    self.assertEqual(
        'Hi <a href="../../bar.bon#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="../../bar.bon#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="../../bar.bon#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hi $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            relative_to='big/long/path/bar.html',
            namespace='bar.bon'))
    self.assertEqual(
        'Hi <a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="bar.bon#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hi $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            relative_to='',
            namespace='bar.bon'))
    self.assertEqual(
        'Hi <a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="bar.bon#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hi $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            relative_to='bar.html',
            namespace='bar.bon'))
    self.assertEqual(
        'Hi <a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="bar.bon#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="bar.bon#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hi $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            relative_to='foo/bar.html',
            namespace='bar.bon'))
    self.assertEqual(
        'Hi <a href="../bar.bon#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="../bar.bon#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="../bar.bon#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hi $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            relative_to='foo/baz/bar.html',
            namespace='bar.bon'))

if __name__ == '__main__':
  unittest.main()
