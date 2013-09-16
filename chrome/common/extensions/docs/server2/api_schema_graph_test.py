#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from api_schema_graph import APISchemaGraph


API_SCHEMA = [{
  'namespace': 'tabs',
  'properties': {
    'lowercase': {
      'properties': {
        'one': { 'value': 1 },
        'two': { 'description': 'just as bad as one' }
      }
    },
    'TAB_PROPERTY_ONE': { 'value': 'magic' },
    'TAB_PROPERTY_TWO': {}
  },
  'types': [
    {
      'id': 'Tab',
      'properties': {
        'id': {},
        'url': {}
      }
    }
  ],
  'functions': [
    {
      'name': 'get',
      'parameters': [ { 'name': 'tab',
                        'type': 'object',
                        'description': 'gets stuff, never complains'
                      },
                      { 'name': 'tabId' }
                    ]
    }
  ],
  'events': [
    {
      'name': 'onActivated',
      'parameters': [ {'name': 'activeInfo'} ]
    },
    {
      'name': 'onUpdated',
      'parameters': [ {'name': 'updateInfo'} ]
    }
  ]
}]


class APISchemaGraphTest(unittest.TestCase):

  def testLookup(self):
    self._testApiSchema(APISchemaGraph(API_SCHEMA))

  def testIsEmpty(self):
    # A few assertions to make sure that Lookup works on empty sets.
    empty_graph = APISchemaGraph({})
    self.assertTrue(empty_graph.IsEmpty())
    self.assertFalse(empty_graph.Lookup('tabs', 'properties',
                                        'TAB_PROPERTY_ONE'))
    self.assertFalse(empty_graph.Lookup('tabs', 'functions', 'get',
                                        'parameters', 'tab'))
    self.assertFalse(empty_graph.Lookup('tabs', 'functions', 'get',
                                        'parameters', 'tabId'))
    self.assertFalse(empty_graph.Lookup('tabs', 'events', 'onActivated',
                                        'parameters', 'activeInfo'))
    self.assertFalse(empty_graph.Lookup('tabs', 'events', 'onUpdated',
                                        'parameters', 'updateInfo'))

  def testSubtractEmpty(self):
    self._testApiSchema(
        APISchemaGraph(API_SCHEMA).Subtract(APISchemaGraph({})))

  def testSubtractSelf(self):
    self.assertTrue(
        APISchemaGraph(API_SCHEMA).Subtract(APISchemaGraph(API_SCHEMA))
            .IsEmpty())

  def _testApiSchema(self, api_schema_graph):
    self.assertTrue(api_schema_graph.Lookup('tabs', 'properties',
                                            'TAB_PROPERTY_ONE'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'types', 'Tab'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'functions', 'get',
                                            'parameters', 'tab'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'functions', 'get',
                                            'parameters', 'tabId'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'functions', 'get',
                                            'parameters', 'tab', 'type'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'events', 'onActivated',
                                            'parameters', 'activeInfo'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'events', 'onUpdated',
                                            'parameters', 'updateInfo'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'properties', 'lowercase',
                                            'properties', 'one', 'value'))
    self.assertTrue(api_schema_graph.Lookup('tabs', 'properties', 'lowercase',
                                            'properties', 'two', 'description'))

    self.assertFalse(api_schema_graph.Lookup('windows'))
    self.assertFalse(api_schema_graph.Lookup('tabs', 'properties',
                                             'TAB_PROPERTY_DEUX'))
    self.assertFalse(api_schema_graph.Lookup('tabs', 'events', 'onActivated',
                                             'parameters', 'callback'))
    self.assertFalse(api_schema_graph.Lookup('tabs', 'functions', 'getById',
                                             'parameters', 'tab'))
    self.assertFalse(api_schema_graph.Lookup('tabs', 'functions', 'get',
                                             'parameters', 'type'))
    self.assertFalse(api_schema_graph.Lookup('tabs', 'properties', 'lowercase',
                                             'properties', 'two', 'value'))

  def testSubtractDisjointSet(self):
    difference = APISchemaGraph(API_SCHEMA).Subtract(APISchemaGraph({
      'contextMenus': {
        'properties': {
          'CONTEXT_MENU_PROPERTY_ONE': {}
        },
        'types': {
          'Menu': {
            'properties': {
              'id': {},
              'width': {}
            }
          }
        },
        'functions': {
          'get': {
            'parameters': {
              'callback': {}
            }
          }
        },
        'events': {
          'onClicked': {
            'parameters': {
              'clickInfo': {}
            }
          },
          'onUpdated': {
            'parameters': {
              'updateInfo': {}
            }
          }
        }
      }
    }))
    self.assertTrue(difference.Lookup('tabs', 'properties',
                                      'TAB_PROPERTY_ONE'))
    self.assertTrue(difference.Lookup('tabs', 'functions', 'get',
                                      'parameters', 'tab'))
    self.assertTrue(difference.Lookup('tabs', 'events', 'onUpdated',
                                      'parameters', 'updateInfo'))
    self.assertTrue(difference.Lookup('tabs', 'functions', 'get',
                                      'parameters', 'tabId'))
    self.assertFalse(difference.Lookup('contextMenus', 'properties',
                                       'CONTEXT_MENU_PROPERTY_ONE'))
    self.assertFalse(difference.Lookup('contextMenus', 'types', 'Menu'))
    self.assertFalse(difference.Lookup('contextMenus', 'types', 'Menu',
                                       'properties', 'id'))
    self.assertFalse(difference.Lookup('contextMenus', 'functions'))
    self.assertFalse(difference.Lookup('contextMenus', 'events', 'onClicked',
                                       'parameters', 'clickInfo'))
    self.assertFalse(difference.Lookup('contextMenus', 'events', 'onUpdated',
                                       'parameters', 'updateInfo'))

  def testSubtractSubset(self):
    difference = APISchemaGraph(API_SCHEMA).Subtract(APISchemaGraph({
      'tabs': {
        'properties': {
          'TAB_PROPERTY_ONE': { 'value': {} }
        },
        'functions': {
          'get': {
            'parameters': {
              'tab': { 'name': {},
                       'type': {},
                       'description': {}
                     }
            }
          }
        },
        'events': {
          'onUpdated': {
            'parameters': {
              'updateInfo': {
                'name': {},
                'properties': {
                  'tabId': {}
                }
              }
            }
          }
        }
      }
    }))
    self.assertTrue(difference.Lookup('tabs'))
    self.assertTrue(difference.Lookup('tabs', 'properties',
                                      'TAB_PROPERTY_TWO'))
    self.assertTrue(difference.Lookup('tabs', 'properties', 'lowercase',
                                      'properties', 'two', 'description'))
    self.assertTrue(difference.Lookup('tabs', 'types', 'Tab', 'properties',
                                      'url'))
    self.assertTrue(difference.Lookup('tabs', 'events', 'onActivated',
                                      'parameters', 'activeInfo'))
    self.assertFalse(difference.Lookup('tabs', 'events', 'onUpdated',
                                       'parameters', 'updateInfo'))
    self.assertFalse(difference.Lookup('tabs', 'properties',
                                       'TAB_PROPERTY_ONE'))
    self.assertFalse(difference.Lookup('tabs', 'properties',
                                       'TAB_PROPERTY_ONE', 'value'))
    self.assertFalse(difference.Lookup('tabs', 'functions', 'get',
                                       'parameters', 'tab'))

  def testSubtractSuperset(self):
    difference = APISchemaGraph(API_SCHEMA).Subtract(APISchemaGraph({
      'tabs': {
        'namespace': {},
        'properties': {
          'lowercase': {
            'properties': {
              'one': { 'value': {} },
              'two': { 'description': {} }
            }
          },
          'TAB_PROPERTY_ONE': { 'value': {} },
          'TAB_PROPERTY_TWO': {},
          'TAB_PROPERTY_THREE': {}
        },
        'types': {
          'Tab': {
            'id': {},
            'properties': {
              'id': {},
              'url': {}
            }
          },
          'UpdateInfo': {}
        },
        'functions': {
          'get': {
            'name': {},
            'parameters': {
              'tab': { 'name': {},
                       'type': {},
                       'description': {}
                     },
              'tabId': { 'name': {} }
            }
          },
          'getById': {
            'parameters': {
              'tabId': {}
            }
          }
        },
        'events': {
          'onActivated': {
            'name': {},
            'parameters': {
              'activeInfo': { 'name': {} }
            }
          },
          'onUpdated': {
            'name': {},
            'parameters': {
              'updateInfo': { 'name': {} }
            }
          },
          'onClicked': {
            'name': {},
            'parameters': {
              'clickInfo': {}
            }
          }
        }
      }
    }))
    self.assertFalse(difference.Lookup('tabs'))
    self.assertFalse(difference.Lookup('tabs', 'properties',
                                       'TAB_PROPERTY_TWO'))
    self.assertFalse(difference.Lookup('tabs', 'properties'))
    self.assertFalse(difference.Lookup('tabs', 'types', 'Tab', 'properties',
                                       'url'))
    self.assertFalse(difference.Lookup('tabs', 'types', 'Tab', 'properties',
                                       'id'))
    self.assertFalse(difference.Lookup('tabs', 'events', 'onUpdated',
                                       'parameters', 'updateInfo'))
    self.assertFalse(difference.Lookup('tabs', 'properties',
                                       'TAB_PROPERTY_ONE'))
    self.assertFalse(difference.Lookup('tabs', 'functions', 'get',
                                       'parameters', 'tabId'))
    self.assertFalse(difference.Lookup('events', 'onUpdated', 'parameters',
                                       'updateInfo'))


if __name__ == '__main__':
  unittest.main()
