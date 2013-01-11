#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

from file_system import FileNotFoundError
from in_memory_object_store import InMemoryObjectStore
from reference_resolver import ReferenceResolver

class FakeAPIDataSource(object):
  def __init__(self, json_data):
    self._json = json_data

  def get(self, key):
    if key not in self._json:
      raise FileNotFoundError(key)
    return self._json[key]

  def GetAllNames(self):
    return self._json.keys()

class APIDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join(sys.path[0], 'test_data', 'test_json')

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def testGetLink(self):
    data_source = FakeAPIDataSource(
        json.loads(self._ReadLocalFile('fake_data_source.json')))
    resolver = ReferenceResolver(data_source,
                                 data_source,
                                 InMemoryObjectStore(''))
    self.assertEqual({
      'href': 'foo.html#type-foo_t1',
      'text': 'foo.foo_t1',
      'name': 'foo_t1'
    }, resolver.GetLink('foo.foo_t1', namespace='baz'))
    self.assertEqual({
      'href': 'baz.html#event-baz_e1',
      'text': 'baz_e1',
      'name': 'baz_e1'
    }, resolver.GetLink('baz.baz_e1', namespace='baz'))
    self.assertEqual({
      'href': 'baz.html#event-baz_e1',
      'text': 'baz_e1',
      'name': 'baz_e1'
    }, resolver.GetLink('baz_e1', namespace='baz'))
    self.assertEqual({
      'href': 'foo.html#method-foo_f1',
      'text': 'foo.foo_f1',
      'name': 'foo_f1'
    }, resolver.GetLink('foo.foo_f1', namespace='baz'))
    self.assertEqual({
      'href': 'foo.html#property-foo_p3',
      'text': 'foo.foo_p3',
      'name': 'foo_p3'
    }, resolver.GetLink('foo.foo_p3', namespace='baz'))
    self.assertEqual({
      'href': 'bar.bon.html#type-bar_bon_t3',
      'text': 'bar.bon.bar_bon_t3',
      'name': 'bar_bon_t3'
    }, resolver.GetLink('bar.bon.bar_bon_t3', namespace='baz'))
    self.assertEqual({
      'href': 'bar.bon.html#property-bar_bon_p3',
      'text': 'bar_bon_p3',
      'name': 'bar_bon_p3'
    }, resolver.GetLink('bar_bon_p3', namespace='bar.bon'))
    self.assertEqual({
      'href': 'bar.bon.html#property-bar_bon_p3',
      'text': 'bar_bon_p3',
      'name': 'bar_bon_p3'
    }, resolver.GetLink('bar.bon.bar_bon_p3', namespace='bar.bon'))
    self.assertEqual({
      'href': 'bar.html#event-bar_e2',
      'text': 'bar_e2',
      'name': 'bar_e2'
    }, resolver.GetLink('bar.bar_e2', namespace='bar'))
    self.assertEqual({
      'href': 'bar.html#type-bon',
      'text': 'bon',
      'name': 'bon'
    }, resolver.GetLink('bar.bon', namespace='bar'))
    self.assertEqual({
      'href': 'foo.html#event-foo_t3-foo_t3_e1',
      'text': 'foo_t3.foo_t3_e1',
      'name': 'foo_t3_e1'
    }, resolver.GetLink('foo_t3.foo_t3_e1', namespace='foo'))
    self.assertEqual({
      'href': 'foo.html#event-foo_t3-foo_t3_e1',
      'text': 'foo_t3.foo_t3_e1',
      'name': 'foo_t3_e1'
    }, resolver.GetLink('foo.foo_t3.foo_t3_e1', namespace='foo'))
    self.assertEqual({
      'href': 'foo.html#event-foo_t3-foo_t3_e1',
      'text': 'foo_t3.foo_t3_e1',
      'name': 'foo_t3_e1'
    }, resolver.GetLink('foo.foo_p1.foo_t3_e1', namespace='foo'))
    self.assertEqual({
      'href': 'bar.html#property-bar_t1-bar_t1_p1',
      'text': 'bar.bar_t1.bar_t1_p1',
      'name': 'bar_t1_p1'
    }, resolver.GetLink('bar.bar_p3.bar_t1_p1', namespace='foo'))
    self.assertEqual({
      'href': 'bar.html#property-bar_t1-bar_t1_p1',
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
        'Hello <a href="bar.bon.html#property-bar_bon_p3">bar_bon_p3</a>, '
            '<a href="bar.bon.html#property-bar_bon_p3">Bon Bon</a>, '
            '<a href="bar.bon.html#property-bar_bon_p3">bar_bon_p3</a>',
        resolver.ResolveAllLinks(
            'Hello $ref:bar_bon_p3, $ref:[bar_bon_p3 Bon Bon], $ref:bar_bon_p3',
            namespace='bar.bon'))
    self.assertEqual(
        'I like <a href="bar.html#property-bar_t1-bar_t1_p1">food</a>.',
        resolver.ResolveAllLinks('I like $ref:[bar.bar_p3.bar_t1_p1 food].',
                                 namespace='foo'))
    self.assertEqual(
        'Ref <a href="bar.html#type-bon">bon</a>',
        resolver.ResolveAllLinks('Ref $ref:[bar.bon]', namespace='bar'))

if __name__ == '__main__':
  unittest.main()
